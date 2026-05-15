import 'dart:async';

import 'src/messages.g.dart';

export 'src/messages.g.dart' show WifiNetwork, WifiSecurity, WifiPermissionStatus;

class LayrzWifi implements LayrzWifiEvents {
  static final LayrzWifi _instance = LayrzWifi._();
  LayrzWifi._();
  static LayrzWifi get instance => _instance;

  final LayrzWifiApi _api = LayrzWifiApi();
  late final StreamController<WifiNetwork> _scanResultsController = StreamController<WifiNetwork>.broadcast();

  /// Broadcast stream of WiFi networks discovered during the current scan.
  ///
  /// Emits [WifiNetwork] objects as they are discovered.
  /// Closes when [stopScan] is called or an error occurs.
  Stream<WifiNetwork> get scanResults => _scanResultsController.stream;

  /// Check if the device supports WiFi discovery.
  Future<bool> hasDiscovery() => _api.hasDiscovery();

  /// Check if the device has a current SSID connection.
  Future<bool> hasCurrentSsid() => _api.hasCurrentSsid();

  /// Get the current SSID the device is connected to.
  Future<String?> currentSsid() => _api.currentSsid();

  /// Start an asynchronous WiFi network scan.
  ///
  /// Results are emitted to the [scanResults] stream.
  /// Listen to the stream before calling this method to ensure you don't miss events.
  /// Call [stopScan] to stop the scan.
  Future<void> startScan() async {
    // Set up the event listener before starting the scan
    LayrzWifiEvents.setUp(this);
    await _api.startScan();
  }

  /// Stop the current WiFi network scan.
  ///
  /// Unregisters the event listener after stopping.
  Future<void> stopScan() async {
    await _api.stopScan();
    // Unregister the event listener by passing null
    LayrzWifiEvents.setUp(null);
  }

  /// Request WiFi permissions. Returns true if granted or not required.
  Future<bool> requestPermissions() => _api.requestPermissions();

  /// Returns the current permission status without requesting.
  Future<WifiPermissionStatus> permissionStatus() => _api.permissionStatus();

  // LayrzWifiEvents implementation
  @override
  void onScanResult(WifiNetwork network) {
    _scanResultsController.add(network);
  }

  @override
  void onScanComplete() {
    // Optionally emit a completion signal or just close the stream
    // For now, we keep the stream open to allow listening to multiple scans
  }

  @override
  void onScanError(String message) {
    _scanResultsController.addError(Exception(message));
  }
}
