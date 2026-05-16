#include "layrz_wifi_plugin.h"

#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>

#include <flutter/plugin_registrar_windows.h>

#include <atomic>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

namespace layrz_wifi {

namespace {

std::string WideToUtf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                 static_cast<int>(wide.size()),
                                 nullptr, 0, nullptr, nullptr);
  std::string result(size, 0);
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

WifiSecurity ParseSecurity(DWORD auth, DWORD cipher) {
  switch (auth) {
    case DOT11_AUTH_ALGO_80211_OPEN:
      return WifiSecurity::kOpen;
    case DOT11_AUTH_ALGO_80211_SHARED_KEY:
      return WifiSecurity::kWep;
    case DOT11_AUTH_ALGO_WPA:
    case DOT11_AUTH_ALGO_WPA_PSK:
    case DOT11_AUTH_ALGO_WPA_NONE:
      return WifiSecurity::kWpa;
    case DOT11_AUTH_ALGO_RSNA:
    case DOT11_AUTH_ALGO_RSNA_PSK:
      return WifiSecurity::kWpa2;
    // WPA3 (SAE) = 12
    case 12:
      return WifiSecurity::kWpa3;
    default:
      return WifiSecurity::kUnknown;
  }
}

std::string MacAddressToString(const DOT11_MAC_ADDRESS& mac) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (size_t i = 0; i < sizeof(DOT11_MAC_ADDRESS); ++i) {
    if (i > 0) {
      stream << ":";
    }
    stream << std::setw(2) << static_cast<int>(mac[i]);
  }
  return stream.str();
}

// Tracks pending scan completions across all interfaces on a single handle.
struct ScanCtx {
  HANDLE event;
  LONG pending{0};  // counts interfaces awaiting scan_complete/fail
};

void WINAPI ScanNotifyCallback(PWLAN_NOTIFICATION_DATA pNotifData, PVOID pContext) {
  if (!pContext || pNotifData->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM) {
    return;
  }
  if (pNotifData->NotificationCode == wlan_notification_acm_scan_complete ||
      pNotifData->NotificationCode == wlan_notification_acm_scan_fail) {
    auto* ctx = reinterpret_cast<ScanCtx*>(pContext);
    if (InterlockedDecrement(&ctx->pending) <= 0) {
      SetEvent(ctx->event);
    }
  }
}

}  // namespace

// static
void LayrzWifiPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto plugin = std::make_unique<LayrzWifiPlugin>();
  plugin->ui_thread_ = std::make_unique<LayrzWifiPluginUiThreadHandler>(registrar);
  plugin->events_ = std::make_unique<LayrzWifiEvents>(registrar->messenger());
  LayrzWifiApi::SetUp(registrar->messenger(), plugin.get());
  registrar->AddPlugin(std::move(plugin));
}

LayrzWifiPlugin::LayrzWifiPlugin() {
  cancel_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

LayrzWifiPlugin::~LayrzWifiPlugin() {
  scanning_ = false;
  if (cancel_event_) {
    SetEvent(cancel_event_);
    CloseHandle(cancel_event_);
    cancel_event_ = nullptr;
  }
}

ErrorOr<bool> LayrzWifiPlugin::HasDiscovery() { return true; }

ErrorOr<bool> LayrzWifiPlugin::HasCurrentSsid() { return true; }

ErrorOr<std::optional<std::string>> LayrzWifiPlugin::CurrentSsid() {
  HANDLE handle = nullptr;
  DWORD negVersion = 0;
  if (WlanOpenHandle(2, nullptr, &negVersion, &handle) != ERROR_SUCCESS) {
    return std::optional<std::string>(std::nullopt);
  }

  PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
  if (WlanEnumInterfaces(handle, nullptr, &ifList) != ERROR_SUCCESS) {
    WlanCloseHandle(handle, nullptr);
    return std::optional<std::string>(std::nullopt);
  }

  std::optional<std::string> result = std::nullopt;
  for (DWORD i = 0; i < ifList->dwNumberOfItems && !result.has_value(); i++) {
    PWLAN_CONNECTION_ATTRIBUTES connAttr = nullptr;
    DWORD dataSize = 0;
    if (WlanQueryInterface(handle, &ifList->InterfaceInfo[i].InterfaceGuid,
                           wlan_intf_opcode_current_connection, nullptr,
                           &dataSize,
                           reinterpret_cast<PVOID*>(&connAttr),
                           nullptr) == ERROR_SUCCESS) {
      if (connAttr->isState == wlan_interface_state_connected) {
        const DOT11_SSID& ssid = connAttr->wlanAssociationAttributes.dot11Ssid;
        if (ssid.uSSIDLength > 0) {
          result = std::string(reinterpret_cast<const char*>(ssid.ucSSID),
                               ssid.uSSIDLength);
        }
      }
      WlanFreeMemory(connAttr);
    }
  }

  WlanFreeMemory(ifList);
  WlanCloseHandle(handle, nullptr);
  return result;
}

std::optional<FlutterError> LayrzWifiPlugin::StartScan() {
  if (scanning_.exchange(true)) {
    return FlutterError("SCAN_IN_PROGRESS", "A scan is already in progress.");
  }

  const uint64_t session = scan_session_.fetch_add(1) + 1;
  ResetEvent(cancel_event_);

  std::thread([this, session]() {
    HANDLE handle = nullptr;
    DWORD negVersion = 0;
    if (WlanOpenHandle(2, nullptr, &negVersion, &handle) != ERROR_SUCCESS) {
      if (scan_session_ == session && scanning_.exchange(false)) {
        ui_thread_->Post([this]() {
          events_->OnScanError("Failed to open WLAN handle.",
                               []() {},
                               [](const FlutterError&) {});
        });
      }
      return;
    }

    PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
    if (WlanEnumInterfaces(handle, nullptr, &ifList) != ERROR_SUCCESS) {
      if (scan_session_ == session && scanning_.exchange(false)) {
        ui_thread_->Post([this]() {
          events_->OnScanError("Failed to enumerate WLAN interfaces.",
                               []() {},
                               [](const FlutterError&) {});
        });
      }
      WlanCloseHandle(handle, nullptr);
      return;
    }

    while (scanning_ && scan_session_ == session) {
      // Register one notification handler for all interfaces on this handle.
      ScanCtx ctx;
      ctx.event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
      ctx.pending = 0;

      WlanRegisterNotification(handle, WLAN_NOTIFICATION_SOURCE_ACM, TRUE,
                               ScanNotifyCallback, &ctx, nullptr, nullptr);

      // Trigger a scan on every interface that accepts it.
      for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
        if (!scanning_ || scan_session_ != session) break;
        if (WlanScan(handle, &ifList->InterfaceInfo[i].InterfaceGuid,
                     nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
          InterlockedIncrement(&ctx.pending);
        }
      }

      // Wait for scan notifications, cancellation, or timeout.
      if (scanning_ && scan_session_ == session && ctx.pending > 0) {
        HANDLE waits[] = {ctx.event, cancel_event_};
        WaitForMultipleObjects(2, waits, FALSE, 15000);
      }

      WlanRegisterNotification(handle, WLAN_NOTIFICATION_SOURCE_NONE, TRUE,
                               nullptr, nullptr, nullptr, nullptr);
      CloseHandle(ctx.event);

      if (!scanning_ || scan_session_ != session) {
        break;
      }

      // Read the latest results from every interface after each scan pass.
      for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
        if (!scanning_ || scan_session_ != session) break;

        const GUID& guid = ifList->InterfaceInfo[i].InterfaceGuid;

        std::unordered_map<std::string, WifiSecurity> security_map;
        PWLAN_AVAILABLE_NETWORK_LIST netList = nullptr;
        if (WlanGetAvailableNetworkList(handle, &guid,
                                        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES |
                                        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
                                        nullptr, &netList) == ERROR_SUCCESS) {
          for (DWORD j = 0; j < netList->dwNumberOfItems; j++) {
            const WLAN_AVAILABLE_NETWORK& net = netList->Network[j];
            std::string ssid(reinterpret_cast<const char*>(net.dot11Ssid.ucSSID),
                             net.dot11Ssid.uSSIDLength);
            security_map[ssid] = ParseSecurity(net.dot11DefaultAuthAlgorithm,
                                               net.dot11DefaultCipherAlgorithm);
          }
          WlanFreeMemory(netList);
        }

        PWLAN_BSS_LIST bssList = nullptr;
        if (WlanGetNetworkBssList(handle, &guid, nullptr, dot11_BSS_type_any,
                                  FALSE, nullptr, &bssList) != ERROR_SUCCESS) {
          continue;
        }

        for (DWORD j = 0; j < bssList->dwNumberOfItems; j++) {
          if (!scanning_ || scan_session_ != session) break;

          const WLAN_BSS_ENTRY& bss = bssList->wlanBssEntries[j];
          const DOT11_SSID& ssid = bss.dot11Ssid;
          if (ssid.uSSIDLength == 0) continue;

          std::string ssidStr(reinterpret_cast<const char*>(ssid.ucSSID), ssid.uSSIDLength);
          auto security_it = security_map.find(ssidStr);
          WifiSecurity security = security_it != security_map.end()
              ? security_it->second
              : WifiSecurity::kUnknown;

          WifiNetwork network(ssidStr, security, false);
          network.set_bssid(MacAddressToString(bss.dot11Bssid));
          network.set_signal_dbm(static_cast<int64_t>(bss.lRssi));
          if (bss.ulChCenterFrequency > 0) {
            network.set_frequency_mhz(static_cast<int64_t>(bss.ulChCenterFrequency / 1000));
          }

          ui_thread_->Post([this, network, session]() {
            if (scan_session_ != session || !scanning_) {
              return;
            }
            events_->OnScanResult(network,
                                  []() {},
                                  [](const FlutterError&) {});
          });
        }
        WlanFreeMemory(bssList);
      }

      if (scanning_ && scan_session_ == session &&
          WaitForSingleObject(cancel_event_, 1500) == WAIT_OBJECT_0) {
        break;
      }
    }

    WlanFreeMemory(ifList);
    WlanCloseHandle(handle, nullptr);

    if (scan_session_ == session) {
      scanning_ = false;
    }
  }).detach();

  return std::nullopt;
}

std::optional<FlutterError> LayrzWifiPlugin::StopScan() {
  if (!scanning_) {
    return std::nullopt;
  }
  scanning_ = false;
  scan_session_.fetch_add(1);
  SetEvent(cancel_event_);
  return std::nullopt;
}

ErrorOr<bool> LayrzWifiPlugin::RequestPermissions() {
  return true;
}

ErrorOr<WifiPermissionStatus> LayrzWifiPlugin::PermissionStatus() {
  return WifiPermissionStatus::kNotRequired;
}

}  // namespace layrz_wifi
