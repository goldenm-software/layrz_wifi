#if os(iOS)
  import Flutter
  import UIKit
  import NetworkExtension
  import SystemConfiguration.CaptiveNetwork
#elseif os(macOS)
  import FlutterMacOS
  import CoreWLAN
#endif
import CoreLocation

public class LayrzWifiPlugin: NSObject, FlutterPlugin, LayrzWifiApi, CLLocationManagerDelegate {
  var events: LayrzWifiEvents?
  var locationManager: CLLocationManager?

  public static func register(with registrar: FlutterPluginRegistrar) {
    let plugin = LayrzWifiPlugin()
    #if os(iOS)
      plugin.events = LayrzWifiEvents(binaryMessenger: registrar.messenger())
      LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger(), api: plugin)
    #elseif os(macOS)
      plugin.events = LayrzWifiEvents(binaryMessenger: registrar.messenger)
      LayrzWifiApiSetup.setUp(binaryMessenger: registrar.messenger, api: plugin)
    #endif
    plugin.locationManager = CLLocationManager()
    plugin.locationManager?.delegate = plugin
  }

  func hasDiscovery() throws -> Bool {
    #if os(iOS)
      return false
    #else
      return true
    #endif
  }

  func hasCurrentSsid() throws -> Bool { true }

  func currentSsid() throws -> String? {
    #if os(iOS)
      if #available(iOS 14.0, *) {
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
    #elseif os(macOS)
      return CWWiFiClient.shared().interface()?.ssid()
    #endif
  }

  func startScan() throws {
    guard (try? permissionStatus()) == .granted else {
      DispatchQueue.main.async {
        self.events?.onScanError(message: "Location permission is required to scan for WiFi networks.") { _ in }
      }
      return
    }

    #if os(iOS)
      DispatchQueue.main.async {
        self.events?.onScanError(message: "WiFi scan is not supported on iOS.") { _ in }
      }
    #elseif os(macOS)
      DispatchQueue.global(qos: .userInitiated).async {
        do {
          let client = CWWiFiClient.shared()
          guard let interface = client.interface() else {
            DispatchQueue.main.async {
              self.events?.onScanError(message: "No WiFi interfaces found.") { _ in }
            }
            return
          }

          let networks = try interface.scanForNetworks(withName: nil)

          for net in networks {
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

            DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
            self.events?.onScanComplete { _ in }
          }
        } catch {
          self.log("Something went wrong: \(error.localizedDescription)")
          DispatchQueue.main.async {
            self.events?.onScanError(message: error.localizedDescription) { _ in }
          }
        }
      }
    #endif
  }

  func stopScan() throws {}

  func requestPermissions() throws -> Bool {
    let manager = locationManager ?? CLLocationManager()
    let status = authorizationStatus(manager)
    switch status {
    case .authorizedAlways:
      return true
    #if os(iOS)
    case .authorizedWhenInUse:
      return true
    #endif
    case .notDetermined:
      #if os(iOS)
        manager.requestWhenInUseAuthorization()
      #elseif os(macOS)
        manager.requestAlwaysAuthorization()
      #endif
      return false
    default:
      return false
    }
  }

  func permissionStatus() throws -> WifiPermissionStatus {
    let manager = locationManager ?? CLLocationManager()
    switch authorizationStatus(manager) {
    case .authorizedAlways:
      return .granted
    #if os(iOS)
    case .authorizedWhenInUse:
      return .granted
    #endif
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

  private func authorizationStatus(_ manager: CLLocationManager) -> CLAuthorizationStatus {
    if #available(iOS 14.0, macOS 11.0, *) {
      return manager.authorizationStatus
    } else {
      return CLLocationManager.authorizationStatus()
    }
  }

  public func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {}

  #if os(macOS)
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
  #endif

  #if os(iOS)
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
  #endif

  private func log(_ message: String) {
    NSLog("[LayrzWifiPlugin]: \(message)")
  }
}
