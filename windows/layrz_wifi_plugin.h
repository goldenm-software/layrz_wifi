#ifndef FLUTTER_PLUGIN_LAYRZ_WIFI_PLUGIN_H_
#define FLUTTER_PLUGIN_LAYRZ_WIFI_PLUGIN_H_

#include <flutter/plugin_registrar_windows.h>
#include "messages.g.h"
#include "thread_handler.hpp"

#include <atomic>
#include <memory>

namespace layrz_wifi {

class LayrzWifiPlugin : public flutter::Plugin, public LayrzWifiApi {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  LayrzWifiPlugin();
  virtual ~LayrzWifiPlugin();

  LayrzWifiPlugin(const LayrzWifiPlugin&) = delete;
  LayrzWifiPlugin& operator=(const LayrzWifiPlugin&) = delete;

  ErrorOr<bool> HasDiscovery() override;
  ErrorOr<bool> HasCurrentSsid() override;
  ErrorOr<std::optional<std::string>> CurrentSsid() override;
  std::optional<FlutterError> StartScan() override;
  std::optional<FlutterError> StopScan() override;
  ErrorOr<bool> RequestPermissions() override;
  ErrorOr<WifiPermissionStatus> PermissionStatus() override;

 private:
  std::unique_ptr<LayrzWifiEvents> events_;
  std::atomic<bool> scanning_{false};
  std::unique_ptr<LayrzWifiPluginUiThreadHandler> ui_thread_;
};

}  // namespace layrz_wifi

#endif  // FLUTTER_PLUGIN_LAYRZ_WIFI_PLUGIN_H_
