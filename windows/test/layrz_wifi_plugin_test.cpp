#include <gtest/gtest.h>

#include "layrz_wifi_plugin.h"

namespace layrz_wifi {
namespace test {

TEST(LayrzWifiPlugin, HasDiscovery) {
  LayrzWifiPlugin plugin;
  auto result = plugin.HasDiscovery();
  EXPECT_FALSE(result.has_error());
  EXPECT_TRUE(result.value());
}

TEST(LayrzWifiPlugin, HasCurrentSsid) {
  LayrzWifiPlugin plugin;
  auto result = plugin.HasCurrentSsid();
  EXPECT_FALSE(result.has_error());
  EXPECT_TRUE(result.value());
}

}  // namespace test
}  // namespace layrz_wifi
