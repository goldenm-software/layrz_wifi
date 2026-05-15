#include "include/layrz_wifi/layrz_wifi_plugin.h"
#include "messages.g.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#define LAYRZ_WIFI_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), layrz_wifi_plugin_get_type(), LayrzWifiPlugin))

struct _LayrzWifiPlugin {
  GObject parent_instance;
  FlBinaryMessenger* messenger;
  LayrzWifiLayrzWifiEvents* events;
  std::atomic<bool> scanning;
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

// Data structure for passing network info from worker thread to main thread
struct ScanResultData {
  LayrzWifiPlugin* plugin;
  LayrzWifiWifiNetwork* network;
};

struct ScanErrorData {
  LayrzWifiPlugin* plugin;
  gchar* message;
};

// Idle callback to post a scan result on the main thread
static gboolean post_scan_result_idle(gpointer data) {
  auto* d = static_cast<ScanResultData*>(data);

  layrz_wifi_layrz_wifi_events_on_scan_result(
    d->plugin->events,
    d->network,
    nullptr,
    nullptr,
    nullptr
  );

  g_object_unref(d->network);
  g_object_unref(d->plugin);
  delete d;
  return G_SOURCE_REMOVE;
}

// Idle callback to post scan completion on the main thread
static gboolean post_scan_complete_idle(gpointer data) {
  auto* plugin = static_cast<LayrzWifiPlugin*>(data);

  layrz_wifi_layrz_wifi_events_on_scan_complete(
    plugin->events,
    nullptr,
    nullptr,
    nullptr
  );

  g_object_unref(plugin);
  return G_SOURCE_REMOVE;
}

// Idle callback to post scan error on the main thread
static gboolean post_scan_error_idle(gpointer data) {
  auto* d = static_cast<ScanErrorData*>(data);

  layrz_wifi_layrz_wifi_events_on_scan_error(
    d->plugin->events,
    d->message,
    nullptr,
    nullptr,
    nullptr
  );

  g_object_unref(d->plugin);
  g_free(d->message);
  delete d;
  return G_SOURCE_REMOVE;
}

// Worker thread function to perform the WiFi scan
static void scan_worker_thread(LayrzWifiPlugin* plugin) {
  FILE* pipe = popen("nmcli -t -f SSID,BSSID,SIGNAL,SECURITY dev wifi list 2>/dev/null", "r");
  if (!pipe) {
    auto* err_data = new ScanErrorData();
    err_data->plugin = LAYRZ_WIFI_PLUGIN(g_object_ref(plugin));
    err_data->message = g_strdup("Failed to run nmcli command");
    g_idle_add(post_scan_error_idle, err_data);
    return;
  }

  char line[512];
  while (fgets(line, sizeof(line), pipe) && plugin->scanning.load()) {
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

    // Post result to main thread via idle callback
    auto* result_data = new ScanResultData();
    result_data->plugin = LAYRZ_WIFI_PLUGIN(g_object_ref(plugin));
    result_data->network = g_object_ref(net);
    g_idle_add(post_scan_result_idle, result_data);

    g_object_unref(net);
  }

  pclose(pipe);

  // Post completion to main thread
  auto* complete_data = LAYRZ_WIFI_PLUGIN(g_object_ref(plugin));
  g_idle_add(post_scan_complete_idle, complete_data);
}

static LayrzWifiLayrzWifiApiStartScanResponse* handle_start_scan(gpointer user_data) {
  LayrzWifiPlugin* plugin = static_cast<LayrzWifiPlugin*>(user_data);
  plugin->scanning.store(true);

  // Spawn worker thread
  std::thread worker_thread(scan_worker_thread, plugin);
  worker_thread.detach();

  return layrz_wifi_layrz_wifi_api_start_scan_response_new();
}

static LayrzWifiLayrzWifiApiStopScanResponse* handle_stop_scan(gpointer user_data) {
  LayrzWifiPlugin* plugin = static_cast<LayrzWifiPlugin*>(user_data);
  plugin->scanning.store(false);

  return layrz_wifi_layrz_wifi_api_stop_scan_response_new();
}

static LayrzWifiLayrzWifiApiRequestPermissionsResponse* handle_request_permissions(gpointer user_data) {
  return layrz_wifi_layrz_wifi_api_request_permissions_response_new(TRUE);
}

static LayrzWifiLayrzWifiApiPermissionStatusResponse* handle_permission_status(gpointer user_data) {
  return layrz_wifi_layrz_wifi_api_permission_status_response_new(
    LAYRZ_WIFI_WIFI_PERMISSION_STATUS_NOT_REQUIRED
  );
}

static LayrzWifiLayrzWifiApiVTable vtable = {
  handle_has_discovery,
  handle_has_current_ssid,
  handle_current_ssid,
  handle_start_scan,
  handle_stop_scan,
  handle_request_permissions,
  handle_permission_status,
};

static void layrz_wifi_plugin_dispose(GObject* object) {
  LayrzWifiPlugin* self = LAYRZ_WIFI_PLUGIN(object);
  layrz_wifi_layrz_wifi_api_clear_method_handlers(self->messenger, nullptr);
  g_clear_object(&self->events);
  g_clear_object(&self->messenger);
  G_OBJECT_CLASS(layrz_wifi_plugin_parent_class)->dispose(object);
}

static void layrz_wifi_plugin_class_init(LayrzWifiPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = layrz_wifi_plugin_dispose;
}

static void layrz_wifi_plugin_init(LayrzWifiPlugin* self) {
  self->events = nullptr;
  self->scanning.store(false);
}

void layrz_wifi_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  LayrzWifiPlugin* plugin = LAYRZ_WIFI_PLUGIN(
    g_object_new(layrz_wifi_plugin_get_type(), nullptr));

  FlBinaryMessenger* messenger = fl_plugin_registrar_get_messenger(registrar);
  plugin->messenger = FL_BINARY_MESSENGER(g_object_ref(messenger));

  // Create the FlutterApi callback object
  plugin->events = layrz_wifi_layrz_wifi_events_new(messenger, nullptr);

  layrz_wifi_layrz_wifi_api_set_method_handlers(
    messenger, nullptr, &vtable, plugin, g_object_unref);
}
