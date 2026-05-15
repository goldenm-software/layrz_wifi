import Flutter
import UIKit
import CoreLocation
import NetworkExtension
import SystemConfiguration.CaptiveNetwork

public class LayrzWifiPlugin: NSObject, FlutterPlugin, LayrzWifiApi {
  var events: LayrzWifiEvents?

  public static func register(with registrar: FlutterPluginRegistrar) {
    let plugin = LayrzWifiPlugin()
    plugin.events = LayrzWifiEvents(binaryMessenger: registrar.messenger())
    LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger(), api: plugin)
  }

  func hasDiscovery() throws -> Bool { false }

  func hasCurrentSsid() throws -> Bool { true }

  func currentSsid() throws -> String? {
    if #available(iOS 14.0, *) {
      // NEHotspotNetwork.fetchCurrent requires the Access WiFi Information entitlement.
      // This call is async; we bridge it synchronously via a semaphore because Pigeon
      // currently generates synchronous HostApi methods.
      var result: String? = nil
      let semaphore = DispatchSemaphore(value: 0)
      NEHotspotNetwork.fetchCurrent { network in
        result = network?.ssid
        semaphore.signal()
      }
      semaphore.wait()
      return result
    } else {
      return legacyCopyCurrentSsid()
    }
  }

  func startScan() throws {
    guard (try? permissionStatus()) == .granted else {
      DispatchQueue.main.async {
        self.events?.onScanError(message: "Location permission is required to scan for WiFi networks.") { _ in }
      }
      return
    }
    DispatchQueue.main.async {
      self.events?.onScanError(message: "WiFi scan is not supported on iOS.") { _ in }
    }
  }

  func stopScan() throws {
    // No-op on iOS
  }

  func requestPermissions() throws -> Bool {
    let status = CLLocationManager.authorizationStatus()
    switch status {
    case .authorizedAlways, .authorizedWhenInUse:
      return true
    case .notDetermined:
      CLLocationManager().requestWhenInUseAuthorization()
      return false
    default:
      return false
    }
  }

  func permissionStatus() throws -> WifiPermissionStatus {
    let status = CLLocationManager.authorizationStatus()
    switch status {
    case .authorizedAlways, .authorizedWhenInUse:
      return .granted
    case .denied:
      return .denied
    case .restricted:
      return .restricted
    case .notDetermined:
      return .denied
    @unknown default:
      return .denied
    }
  }

  private func legacyCopyCurrentSsid() -> String? {
    guard let interfaces = CNCopySupportedInterfaces() as? [String] else { return nil }
    for interface in interfaces {
      if let info = CNCopyCurrentNetworkInfo(interface as CFString) as NSDictionary?,
         let ssid = info[kCNNetworkInfoKeySSID as String] as? String {
        return ssid
      }
    }
    return nil
  }
}
