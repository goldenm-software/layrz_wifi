#include "include/layrz_wifi/layrz_wifi_plugin.h"
#include "messages.g.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define LAYRZ_WIFI_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), layrz_wifi_plugin_get_type(), LayrzWifiPlugin))

struct _LayrzWifiPlugin {
  GObject parent_instance;
  FlBinaryMessenger* messenger;
};

G_DEFINE_TYPE(LayrzWifiPlugin, layrz_wifi_plugin, g_object_get_type())

static LayrzWifiLayrzWifiApiHasDiscoveryResponse* handle_has_discovery(gpointer user_data) {
  return layrz_wifi_layrz_wifi_api_has_discovery_response_new(TRUE);
}

static LayrzWifiLayrzWifiApiHasCurrentSsidResponse* handle_has_current_ssid(gpointer user_data) {
  return layrz_wifi_layrz_wifi_api_has_current_ssid_response_new(TRUE);
}

static LayrzWifiLayrzWifiApiCurrentSsidResponse* handle_current_ssid(gpointer user_data) {
  // nmcli -t -f NAME connection show --active returns the active connection name.
  FILE* pipe = popen("nmcli -t -f NAME connection show --active 2>/dev/null", "r");
  if (!pipe) {
    return layrz_wifi_layrz_wifi_api_current_ssid_response_new(nullptr);
  }
  char buf[256] = {};
  const char* result = nullptr;
  std::string ssid;
  if (fgets(buf, sizeof(buf), pipe)) {
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
      buf[--len] = '\0';
    }
    if (len > 0) {
      ssid = buf;
      result = ssid.c_str();
    }
  }
  pclose(pipe);
  return layrz_wifi_layrz_wifi_api_current_ssid_response_new(result);
}

static LayrzWifiLayrzWifiApiScanResponse* handle_scan(gpointer user_data) {
  // nmcli -t -f SSID,BSSID,SIGNAL,SECURITY dev wifi list
  FILE* pipe = popen("nmcli -t -f SSID,BSSID,SIGNAL,SECURITY dev wifi list 2>/dev/null", "r");
  if (!pipe) {
    FlValue* empty = fl_value_new_list();
    auto* resp = layrz_wifi_layrz_wifi_api_scan_response_new(empty);
    fl_value_unref(empty);
    return resp;
  }

  FlValue* list = fl_value_new_list();
  char line[512];
  while (fgets(line, sizeof(line), pipe)) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

    // Fields are colon-separated; BSSID colons are escaped as \:
    // Format: SSID:BSSID:SIGNAL:SECURITY
    // Simple parser: last 3 colons (after unescaping) are separators.
    std::string row(line);
    // Replace escaped colons temporarily
    std::string tmp;
    for (size_t i = 0; i < row.size(); i++) {
      if (row[i] == '\\' && i + 1 < row.size() && row[i + 1] == ':') {
        tmp += '\x01';
        i++;
      } else {
        tmp += row[i];
      }
    }

    // Split on unescaped colons (find last 3)
    std::vector<size_t> colons;
    for (size_t i = 0; i < tmp.size(); i++) {
      if (tmp[i] == ':') colons.push_back(i);
    }
    if (colons.size() < 3) continue;

    size_t c3 = colons[colons.size() - 1];
    size_t c2 = colons[colons.size() - 2];
    size_t c1 = colons[colons.size() - 3];

    std::string ssidRaw = tmp.substr(0, c1);
    std::string bssidRaw = tmp.substr(c1 + 1, c2 - c1 - 1);
    std::string signalRaw = tmp.substr(c2 + 1, c3 - c2 - 1);
    std::string securityRaw = tmp.substr(c3 + 1);

    // Restore escaped colons
    auto restore = [](std::string s) -> std::string {
      std::string out;
      for (char c : s) out += (c == '\x01') ? ':' : c;
      return out;
    };
    std::string ssid = restore(ssidRaw);
    std::string bssid = restore(bssidRaw);

    int signal = signalRaw.empty() ? 0 : std::stoi(signalRaw);
    // Map nmcli 0-100 signal to approximate dBm (-100..-50)
    int64_t dbm = static_cast<int64_t>((signal / 2) - 100);

    LayrzWifiWifiSecurity sec = LAYRZ_WIFI_WIFI_SECURITY_UNKNOWN;
    if (securityRaw.find("WPA3") != std::string::npos) sec = LAYRZ_WIFI_WIFI_SECURITY_WPA3;
    else if (securityRaw.find("WPA2") != std::string::npos) sec = LAYRZ_WIFI_WIFI_SECURITY_WPA2;
    else if (securityRaw.find("WPA") != std::string::npos) sec = LAYRZ_WIFI_WIFI_SECURITY_WPA;
    else if (securityRaw.find("WEP") != std::string::npos) sec = LAYRZ_WIFI_WIFI_SECURITY_WEP;
    else if (securityRaw == "--") sec = LAYRZ_WIFI_WIFI_SECURITY_OPEN;

    gboolean hidden = ssid.empty();
    int64_t dbmVal = dbm;
    LayrzWifiWifiNetwork* net = layrz_wifi_wifi_network_new(
      ssid.c_str(),
      bssid.empty() ? nullptr : bssid.c_str(),
      &dbmVal,
      nullptr,
      sec,
      hidden
    );
    fl_value_append_take(list, fl_value_new_custom(
      131,
      g_object_ref(net),
      g_object_unref
    ));
    g_object_unref(net);
  }
  pclose(pipe);

  auto* resp = layrz_wifi_layrz_wifi_api_scan_response_new(list);
  fl_value_unref(list);
  return resp;
}

static LayrzWifiLayrzWifiApiEnsurePermissionsResponse* handle_ensure_permissions(gpointer user_data) {
  return layrz_wifi_layrz_wifi_api_ensure_permissions_response_new(
    LAYRZ_WIFI_WIFI_PERMISSION_STATUS_NOT_REQUIRED
  );
}

static LayrzWifiLayrzWifiApiVTable vtable = {
  handle_has_discovery,
  handle_has_current_ssid,
  handle_current_ssid,
  handle_scan,
  handle_ensure_permissions,
};

static void layrz_wifi_plugin_dispose(GObject* object) {
  LayrzWifiPlugin* self = LAYRZ_WIFI_PLUGIN(object);
  layrz_wifi_layrz_wifi_api_clear_method_handlers(self->messenger, nullptr);
  g_clear_object(&self->messenger);
  G_OBJECT_CLASS(layrz_wifi_plugin_parent_class)->dispose(object);
}

static void layrz_wifi_plugin_class_init(LayrzWifiPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = layrz_wifi_plugin_dispose;
}

static void layrz_wifi_plugin_init(LayrzWifiPlugin* self) {}

void layrz_wifi_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  LayrzWifiPlugin* plugin = LAYRZ_WIFI_PLUGIN(
    g_object_new(layrz_wifi_plugin_get_type(), nullptr));

  FlBinaryMessenger* messenger = fl_plugin_registrar_get_messenger(registrar);
  plugin->messenger = FL_BINARY_MESSENGER(g_object_ref(messenger));

  layrz_wifi_layrz_wifi_api_set_method_handlers(
    messenger, nullptr, &vtable, plugin, g_object_unref);
}
