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

  public func hasDiscovery() throws -> Bool { true }

  public func hasCurrentSsid() throws -> Bool { true }

  public func currentSsid() throws -> String? {
    return CWWiFiClient.shared().interface()?.ssid()
  }

  public func startScan() throws {
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
            security: self.mapSecurity(net.security()),
            isHidden: net.ssid == nil || net.ssid!.isEmpty
          )

          DispatchQueue.main.async {
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

  public func stopScan() throws {
    // CoreWLAN scan cannot be cancelled mid-call; the existing scan will complete
    // and fire onScanComplete naturally. This is a no-op.
  }

  public func ensurePermissions() throws -> WifiPermissionStatus {
    let status = CLLocationManager.authorizationStatus()
    switch status {
    case .authorizedAlways:
      return .granted
    case .denied:
      return .denied
    case .restricted:
      return .restricted
    case .notDetermined:
      CLLocationManager().requestAlwaysAuthorization()
      return .denied
    @unknown default:
      return .denied
    }
  }

  private func mapSecurity(_ security: CWSecurity) -> WifiSecurity {
    switch security {
    case .none:
      return .open
    case .WEP:
      return .wep
    case .wpaPersonal, .wpaPersonalMixed, .wpaEnterprise, .wpaEnterpriseMixed:
      return .wpa
    case .wpa2Personal, .wpa2Enterprise:
      return .wpa2
    case .wpa3Personal, .wpa3Enterprise, .wpa3Transition:
      return .wpa3
    default:
      return .unknown
    }
  }
}
