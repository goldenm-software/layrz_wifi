#include "layrz_wifi_plugin.h"

#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>

#include <flutter/plugin_registrar_windows.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>

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

struct ScanCtx {
  HANDLE event;
  GUID guid;
  bool success;
};

void WINAPI ScanNotifyCallback(PWLAN_NOTIFICATION_DATA pNotifData, PVOID pContext) {
  if (!pContext || pNotifData->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM) {
    return;
  }
  auto* ctx = reinterpret_cast<ScanCtx*>(pContext);
  if (!IsEqualGUID(pNotifData->InterfaceGuid, ctx->guid)) {
    return;
  }
  if (pNotifData->NotificationCode == wlan_notification_acm_scan_complete) {
    ctx->success = true;
    SetEvent(ctx->event);
  } else if (pNotifData->NotificationCode == wlan_notification_acm_scan_fail) {
    ctx->success = false;
    SetEvent(ctx->event);
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

  // Reset cancel event so the new scan isn't immediately cancelled
  ResetEvent(cancel_event_);

  std::thread([this]() {
    HANDLE handle = nullptr;
    DWORD negVersion = 0;
    if (WlanOpenHandle(2, nullptr, &negVersion, &handle) != ERROR_SUCCESS) {
      if (scanning_.exchange(false)) {
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
      if (scanning_.exchange(false)) {
        ui_thread_->Post([this]() {
          events_->OnScanError("Failed to enumerate WLAN interfaces.",
                               []() {},
                               [](const FlutterError&) {});
        });
      }
      WlanCloseHandle(handle, nullptr);
      return;
    }

    for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
      if (!scanning_) break;

      const GUID& guid = ifList->InterfaceInfo[i].InterfaceGuid;

      // Set up per-interface scan context before registering notifications
      ScanCtx ctx;
      ctx.event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
      ctx.guid = guid;
      ctx.success = false;

      // Register notification BEFORE triggering scan to avoid race
      WlanRegisterNotification(handle, WLAN_NOTIFICATION_SOURCE_ACM, TRUE,
                               ScanNotifyCallback, &ctx, nullptr, nullptr);

      if (WlanScan(handle, &guid, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        // Wait for scan complete, scan fail, or user cancel (10s timeout)
        HANDLE waits[] = { ctx.event, cancel_event_ };
        WaitForMultipleObjects(2, waits, FALSE, 10000);
      }

      // Unregister before reading results — ctx lifetime ends after this block
      WlanRegisterNotification(handle, WLAN_NOTIFICATION_SOURCE_NONE, TRUE,
                               nullptr, nullptr, nullptr, nullptr);
      CloseHandle(ctx.event);

      if (!scanning_) break;

      PWLAN_AVAILABLE_NETWORK_LIST netList = nullptr;
      if (WlanGetAvailableNetworkList(handle, &guid, 0, nullptr, &netList) != ERROR_SUCCESS) {
        continue;
      }

      for (DWORD j = 0; j < netList->dwNumberOfItems; j++) {
        if (!scanning_) break;

        const WLAN_AVAILABLE_NETWORK& net = netList->Network[j];
        const DOT11_SSID& ssid = net.dot11Ssid;
        std::string ssidStr(reinterpret_cast<const char*>(ssid.ucSSID), ssid.uSSIDLength);
        bool isHidden = ssid.uSSIDLength == 0;

        WifiNetwork network(ssidStr, ParseSecurity(net.dot11DefaultAuthAlgorithm,
                                                    net.dot11DefaultCipherAlgorithm),
                            isHidden);
        network.set_signal_dbm(static_cast<int64_t>(net.wlanSignalQuality));

        ui_thread_->Post([this, network]() {
          events_->OnScanResult(network,
                                []() {},
                                [](const FlutterError&) {});
        });
      }
      WlanFreeMemory(netList);
    }

    WlanFreeMemory(ifList);
    WlanCloseHandle(handle, nullptr);

    if (scanning_.exchange(false)) {
      ui_thread_->Post([this]() {
        events_->OnScanComplete(
            []() {},
            [](const FlutterError&) {});
      });
    }
  }).detach();

  return std::nullopt;
}

std::optional<FlutterError> LayrzWifiPlugin::StopScan() {
  scanning_ = false;
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
