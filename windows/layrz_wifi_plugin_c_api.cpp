#include "include/layrz_wifi/layrz_wifi_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "layrz_wifi_plugin.h"

void LayrzWifiPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  layrz_wifi::LayrzWifiPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
