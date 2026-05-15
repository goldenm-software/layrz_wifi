package com.layrz.layrz_wifi

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.WifiManager
import android.net.wifi.WifiInfo
import android.os.Build
import android.os.Handler
import android.os.Looper
import androidx.core.app.ActivityCompat
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.PluginRegistry
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class LayrzWifiPlugin : FlutterPlugin, ActivityAware, LayrzWifiApi,
    PluginRegistry.RequestPermissionsResultListener {

  private lateinit var context: Context
  private var activityBinding: ActivityPluginBinding? = null
  private var permissionResult: ((WifiPermissionStatus) -> Unit)? = null
  private var events: LayrzWifiEvents? = null
  private var scanReceiver: BroadcastReceiver? = null
  private var scanCancelled = false
  private val mainHandler = Handler(Looper.getMainLooper())

  companion object {
    private const val PERMISSION_REQUEST_CODE = 0x4E57
  }

  override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    context = binding.applicationContext
    events = LayrzWifiEvents(binding.binaryMessenger)
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

  override fun startScan() {
    if (permissionStatus() != WifiPermissionStatus.GRANTED) {
      mainHandler.post { events?.onScanError("Location permission is required to scan for WiFi networks.") {} }
      return
    }
    scanCancelled = false
    CoroutineScope(Dispatchers.IO).launch {
      try {
        val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager

        val receiver = object : BroadcastReceiver() {
          override fun onReceive(context: Context, intent: Intent) {
            if (scanCancelled) return

            try {
              val results = wifiManager.scanResults ?: emptyList()

              // Post each network on the main thread
              for (result in results) {
                if (scanCancelled) break

                val network = WifiNetwork(
                  ssid = result.SSID ?: "",
                  bssid = result.BSSID,
                  signalDbm = result.level.toLong(),
                  frequencyMhz = result.frequency.toLong(),
                  security = parseSecurity(result.capabilities),
                  isHidden = result.SSID.isNullOrEmpty(),
                )

                mainHandler.post {
                  events?.onScanResult(network) { result ->
                    result.onFailure { e ->
                      android.util.Log.e("LayrzWifiPlugin", "Error posting scan result", e)
                    }
                  }
                }
              }

              // Post scan complete on the main thread
              if (!scanCancelled) {
                mainHandler.post {
                  events?.onScanComplete { result ->
                    result.onFailure { e ->
                      android.util.Log.e("LayrzWifiPlugin", "Error posting scan complete", e)
                    }
                  }
                }
              }
            } catch (e: Exception) {
              mainHandler.post {
                events?.onScanError("Scan error: ${e.message}") { result ->
                  result.onFailure { ex ->
                    android.util.Log.e("LayrzWifiPlugin", "Error posting scan error", ex)
                  }
                }
              }
            }

            // Unregister the receiver
            try {
              context.unregisterReceiver(this)
              scanReceiver = null
            } catch (e: Exception) {
              android.util.Log.e("LayrzWifiPlugin", "Error unregistering receiver", e)
            }
          }
        }

        scanReceiver = receiver
        val filter = IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)
        context.registerReceiver(receiver, filter)

        // Trigger the scan
        wifiManager.startScan()
      } catch (e: Exception) {
        mainHandler.post {
          events?.onScanError("Failed to start scan: ${e.message}") { result ->
            result.onFailure { ex ->
              android.util.Log.e("LayrzWifiPlugin", "Error posting scan error", ex)
            }
          }
        }
      }
    }
  }

  override fun stopScan() {
    scanCancelled = true

    if (scanReceiver != null) {
      try {
        context.unregisterReceiver(scanReceiver!!)
        scanReceiver = null
      } catch (e: Exception) {
        android.util.Log.e("LayrzWifiPlugin", "Error unregistering receiver", e)
      }
    }

    mainHandler.post {
      events?.onScanComplete { result ->
        result.onFailure { e ->
          android.util.Log.e("LayrzWifiPlugin", "Error posting scan complete on stop", e)
        }
      }
    }
  }

  override fun requestPermissions(): Boolean {
    val activity = activityBinding?.activity ?: return false

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
      val status = ActivityCompat.checkSelfPermission(context, Manifest.permission.NEARBY_WIFI_DEVICES)
      if (status == PackageManager.PERMISSION_GRANTED) return true
      ActivityCompat.requestPermissions(
        activity,
        arrayOf(Manifest.permission.NEARBY_WIFI_DEVICES),
        PERMISSION_REQUEST_CODE,
      )
    } else {
      val fineStatus = ActivityCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
      if (fineStatus == PackageManager.PERMISSION_GRANTED) return true
      ActivityCompat.requestPermissions(
        activity,
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION),
        PERMISSION_REQUEST_CODE,
      )
    }

    return false
  }

  override fun permissionStatus(): WifiPermissionStatus {
    val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
      Manifest.permission.NEARBY_WIFI_DEVICES
    else
      Manifest.permission.ACCESS_FINE_LOCATION

    return when (ActivityCompat.checkSelfPermission(context, permission)) {
      PackageManager.PERMISSION_GRANTED -> WifiPermissionStatus.GRANTED
      else -> WifiPermissionStatus.DENIED
    }
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
