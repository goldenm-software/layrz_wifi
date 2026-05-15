package com.layrz.layrz_wifi

import kotlin.test.Test
import kotlin.test.assertNotNull

internal class LayrzWifiPluginTest {
  @Test
  fun pluginInstantiates() {
    val plugin = LayrzWifiPlugin()
    assertNotNull(plugin)
  }
}
