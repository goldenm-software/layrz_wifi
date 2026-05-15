import FlutterMacOS
import CoreWLAN
import CoreLocation

public class LayrzWifiPlugin: NSObject, FlutterPlugin, LayrzWifiApi {
  public static func register(with registrar: FlutterPluginRegistrar) {
    let plugin = LayrzWifiPlugin()
    LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger, api: plugin)
  }

  public func hasDiscovery() throws -> Bool { true }

  public func hasCurrentSsid() throws -> Bool { true }

  public func currentSsid() throws -> String? {
    return CWWiFiClient.shared().interface()?.ssid()
  }

  public func scan() throws -> [WifiNetwork] {
    guard let iface = CWWiFiClient.shared().interface() else {
      throw PigeonError(code: "NO_INTERFACE", message: "No WiFi interface found.", details: nil)
    }
    let networks = try iface.scanForNetworks(withName: nil)
    return networks.map { net in
      WifiNetwork(
        ssid: net.ssid ?? "",
        bssid: net.bssid,
        signalDbm: net.rssiValue != 0 ? Int64(net.rssiValue) : nil,
        frequencyMhz: net.wlanChannel.map { Int64($0.channelNumber) },
        security: mapSecurity(net.security()),
        isHidden: net.ssid == nil || net.ssid!.isEmpty
      )
    }
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
