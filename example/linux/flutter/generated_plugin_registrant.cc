//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <layrz_wifi/layrz_wifi_plugin.h>

void fl_register_plugins(FlPluginRegistry* registry) {
  g_autoptr(FlPluginRegistrar) layrz_wifi_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "LayrzWifiPlugin");
  layrz_wifi_plugin_register_with_registrar(layrz_wifi_registrar);
}
