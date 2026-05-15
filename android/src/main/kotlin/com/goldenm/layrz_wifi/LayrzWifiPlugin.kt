package com.goldenm.layrz_wifi

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.WifiManager
import android.net.wifi.WifiInfo
import android.os.Build
import androidx.core.app.ActivityCompat
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.PluginRegistry
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class LayrzWifiPlugin : FlutterPlugin, ActivityAware, LayrzWifiApi,
    PluginRegistry.RequestPermissionsResultListener {

  private lateinit var context: Context
  private var activityBinding: ActivityPluginBinding? = null
  private var permissionResult: ((WifiPermissionStatus) -> Unit)? = null

  companion object {
    private const val PERMISSION_REQUEST_CODE = 0x4E57
  }

  override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    context = binding.applicationContext
    LayrzWifiApi.setUp(binding.binaryMessenger, this)
  }

  override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    LayrzWifiApi.setUp(binding.binaryMessenger, null)
  }

  override fun onAttachedToActivity(binding: ActivityPluginBinding) {
    activityBinding = binding
    binding.addRequestPermissionsResultListener(this)
  }

  override fun onDetachedFromActivityForConfigChanges() {
    activityBinding?.removeRequestPermissionsResultListener(this)
    activityBinding = null
  }

  override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
    activityBinding = binding
    binding.addRequestPermissionsResultListener(this)
  }

  override fun onDetachedFromActivity() {
    activityBinding?.removeRequestPermissionsResultListener(this)
    activityBinding = null
  }

  override fun hasDiscovery(): Boolean = true

  override fun hasCurrentSsid(): Boolean = true

  override fun currentSsid(): String? {
    val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
    val info: WifiInfo = wifiManager.connectionInfo ?: return null
    val ssid = info.ssid ?: return null
    return if (ssid == "<unknown ssid>" || ssid.isEmpty()) null else ssid.removeSurrounding("\"")
  }

  override fun scan(): List<WifiNetwork> {
    val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
    wifiManager.startScan()
    val results = wifiManager.scanResults ?: return emptyList()
    return results.map { result ->
      WifiNetwork(
        ssid = result.SSID ?: "",
        bssid = result.BSSID,
        signalDbm = result.level.toLong(),
        frequencyMhz = result.frequency.toLong(),
        security = parseSecurity(result.capabilities),
        isHidden = result.SSID.isNullOrEmpty(),
      )
    }
  }

  override fun ensurePermissions(): WifiPermissionStatus {
    val activity = activityBinding?.activity ?: return WifiPermissionStatus.DENIED

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
      val status = ActivityCompat.checkSelfPermission(context, Manifest.permission.NEARBY_WIFI_DEVICES)
      if (status == PackageManager.PERMISSION_GRANTED) return WifiPermissionStatus.GRANTED
      ActivityCompat.requestPermissions(
        activity,
        arrayOf(Manifest.permission.NEARBY_WIFI_DEVICES),
        PERMISSION_REQUEST_CODE,
      )
    } else {
      val fineStatus = ActivityCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
      if (fineStatus == PackageManager.PERMISSION_GRANTED) return WifiPermissionStatus.GRANTED
      ActivityCompat.requestPermissions(
        activity,
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION),
        PERMISSION_REQUEST_CODE,
      )
    }

    // Return denied synchronously — the caller should re-invoke after the system dialog.
    return WifiPermissionStatus.DENIED
  }

  override fun onRequestPermissionsResult(
    requestCode: Int,
    permissions: Array<out String>,
    grantResults: IntArray,
  ): Boolean {
    if (requestCode != PERMISSION_REQUEST_CODE) return false
    permissionResult?.invoke(
      if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)
        WifiPermissionStatus.GRANTED
      else
        WifiPermissionStatus.DENIED,
    )
    permissionResult = null
    return true
  }

  private fun parseSecurity(capabilities: String?): WifiSecurity {
    if (capabilities == null) return WifiSecurity.UNKNOWN
    return when {
      capabilities.contains("WPA3") -> WifiSecurity.WPA3
      capabilities.contains("WPA2") -> WifiSecurity.WPA2
      capabilities.contains("WPA") -> WifiSecurity.WPA
      capabilities.contains("WEP") -> WifiSecurity.WEP
      capabilities.contains("ESS") && !capabilities.contains("WPA") && !capabilities.contains("WEP") -> WifiSecurity.OPEN
      else -> WifiSecurity.UNKNOWN
    }
  }
}
