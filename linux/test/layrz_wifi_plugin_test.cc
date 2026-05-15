#include <flutter_linux/flutter_linux.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/layrz_wifi/layrz_wifi_plugin.h"

// Placeholder test — the plugin uses Pigeon-generated channels;
// actual integration testing happens via Flutter integration tests.
namespace layrz_wifi {
namespace test {

TEST(LayrzWifiPlugin, PluginRegistrarIsNotNull) {
  // Sanity-check that the plugin type is registered.
  GType t = layrz_wifi_plugin_get_type();
  ASSERT_NE(t, static_cast<GType>(0));
}

}  // namespace test
}  // namespace layrz_wifi
