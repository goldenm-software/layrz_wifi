import FlutterMacOS
import CoreWLAN
import CoreLocation

public class LayrzWifiPlugin: NSObject, FlutterPlugin, LayrzWifiApi {
  var events: LayrzWifiEvents?

  public static func register(with registrar: FlutterPluginRegistrar) {
    let plugin = LayrzWifiPlugin()
    plugin.events = LayrzWifiEvents(binaryMessenger: registrar.messenger)
    LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger, api: plugin)
  }

  func hasDiscovery() throws -> Bool { true }

  func hasCurrentSsid() throws -> Bool { true }

  func currentSsid() throws -> String? {
    return CWWiFiClient.shared().interface()?.ssid()
  }

  func startScan() throws {
    DispatchQueue.global(qos: .userInitiated).async {
      do {
        guard let iface = CWWiFiClient.shared().interface() else {
          DispatchQueue.main.async {
            self.events?.onScanError(message: "No WiFi interface found.") { _ in }
          }
          return
        }

        let networks = try iface.scanForNetworks(withName: nil)

        for net in networks {
          let wifiNet = WifiNetwork(
            ssid: net.ssid ?? "",
            bssid: net.bssid,
            signalDbm: net.rssiValue != 0 ? Int64(net.rssiValue) : nil,
            frequencyMhz: net.wlanChannel.map { Int64($0.channelNumber) },
            security: self.mapSecurity(net),
            isHidden: net.ssid == nil || net.ssid!.isEmpty
          )

          DispatchQueue.main.async { [wifiNet] in
            self.events?.onScanResult(network: wifiNet) { _ in }
          }
        }

        DispatchQueue.main.async {
          self.events?.onScanComplete { _ in }
        }
      } catch {
        DispatchQueue.main.async {
          self.events?.onScanError(message: error.localizedDescription) { _ in }
        }
      }
    }
  }

  func stopScan() throws {}

  func ensurePermissions() throws -> WifiPermissionStatus {
    let manager = CLLocationManager()
    let status: CLAuthorizationStatus
    if #available(macOS 11.0, *) {
      status = manager.authorizationStatus
    } else {
      status = CLLocationManager.authorizationStatus()
    }
    switch status {
    case .authorizedAlways, .authorized:
      return .granted
    case .denied:
      return .denied
    case .restricted:
      return .restricted
    case .notDetermined:
      // Don't request here — the app layer (permission_handler) owns the prompt.
      return .denied
    @unknown default:
      return .denied
    }
  }

  private func mapSecurity(_ network: CWNetwork) -> WifiSecurity {
    if network.supportsSecurity(.wpa3Personal) || network.supportsSecurity(.wpa3Enterprise) || network.supportsSecurity(.wpa3Transition) {
      return .wpa3
    } else if network.supportsSecurity(.wpa2Personal) || network.supportsSecurity(.wpa2Enterprise) {
      return .wpa2
    } else if network.supportsSecurity(.wpaPersonal) || network.supportsSecurity(.wpaPersonalMixed) || network.supportsSecurity(.wpaEnterprise) || network.supportsSecurity(.wpaEnterpriseMixed) {
      return .wpa
    } else if network.supportsSecurity(.WEP) {
      return .wep
    } else if network.supportsSecurity(.none) {
      return .open
    } else {
      return .unknown
    }
  }
}
