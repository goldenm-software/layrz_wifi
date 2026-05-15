import FlutterMacOS
import CoreWLAN
import CoreLocation

public class LayrzWifiPlugin: NSObject, FlutterPlugin, LayrzWifiApi, CLLocationManagerDelegate {
    var events: LayrzWifiEvents?
    var locationManager: CLLocationManager?

    public static func register(with registrar: FlutterPluginRegistrar) {
        let plugin = LayrzWifiPlugin()
        plugin.events = LayrzWifiEvents(binaryMessenger: registrar.messenger)
        LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger, api: plugin)

        plugin.locationManager = CLLocationManager()
        plugin.locationManager?.delegate = plugin
    }

  func hasDiscovery() throws -> Bool { true }

  func hasCurrentSsid() throws -> Bool { true }

  func currentSsid() throws -> String? {
    let client = CWWiFiClient.shared()
    let iface = client.interface()
    return iface?.ssid()
  }

  func startScan() throws {
    DispatchQueue.global(qos: .userInitiated).async {
        do {
            let client = CWWiFiClient.shared()
            let interface = client.interface()
            
            if interface == nil {
                DispatchQueue.main.async {
                    self.events?.onScanError(message: "No WiFi interfaces found") { _ in }
                }
                return
            }
            
            let networks = try interface!.scanForNetworks(withName: nil)
            
            for net in networks {
                self.log("SSID: \(net.ssid) - BSSID: \(net.bssid)")
                let wifiNet = WifiNetwork(
                    ssid: net.ssid ?? "",
                    bssid: net.bssid,
                    signalDbm: net.rssiValue != 0 ? Int64(net.rssiValue) : nil,
                    frequencyMhz: net.wlanChannel.map { Int64($0.channelNumber) },
                    security: self.mapSecurity(net),
                    isHidden: net.ssid == nil
                )
                
                DispatchQueue.main.async {
                    self.events?.onScanResult(network: wifiNet) { _ in }
                }
            }
            
            DispatchQueue.main.async {
                self.events?.onScanComplete() { _ in }
            }
        } catch {
            self.log("Something went wrong: \(error.localizedDescription)")
            DispatchQueue.main.async {
                self.events?.onScanError(message: error.localizedDescription) { _ in }
            }
        }
    }
  }

  func stopScan() throws {}

  func ensurePermissions() throws -> WifiPermissionStatus {
    let manager = locationManager ?? CLLocationManager()
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
      manager.requestAlwaysAuthorization()
      return .notRequired
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
    
    public func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {}

    private func log(_ message: String) {
        NSLog("[LayrzWifiPlugin/macOS]: \(message)")
    }
}
