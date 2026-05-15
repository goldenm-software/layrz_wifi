import 'dart:async';

import 'src/messages.g.dart';

export 'src/messages.g.dart' show WifiNetwork, WifiSecurity, WifiPermissionStatus;

sealed class WifiScanEvent {}

class WifiScanComplete extends WifiScanEvent {}

class WifiScanError extends WifiScanEvent {
  WifiScanError(this.message);
  final String message;
}

class LayrzWifi implements LayrzWifiEvents {
  static final LayrzWifi _instance = LayrzWifi._();
  LayrzWifi._();
  static LayrzWifi get instance => _instance;

  final LayrzWifiApi _api = LayrzWifiApi();
  late final StreamController<WifiNetwork> _scanResultsController = StreamController<WifiNetwork>.broadcast();
  late final StreamController<WifiScanEvent> _scanEventsController = StreamController<WifiScanEvent>.broadcast();

  /// Broadcast stream of WiFi networks discovered during the current scan.
  Stream<WifiNetwork> get scanResults => _scanResultsController.stream;

  /// Broadcast stream of scan lifecycle events (complete or error).
  Stream<WifiScanEvent> get scanEvents => _scanEventsController.stream;

  Future<bool> hasDiscovery() => _api.hasDiscovery();
  Future<bool> hasCurrentSsid() => _api.hasCurrentSsid();
  Future<String?> currentSsid() => _api.currentSsid();

  Future<void> startScan() async {
    LayrzWifiEvents.setUp(this);
    await _api.startScan();
  }

  Future<void> stopScan() async {
    await _api.stopScan();
    LayrzWifiEvents.setUp(null);
  }

  Future<bool> requestPermissions() => _api.requestPermissions();
  Future<WifiPermissionStatus> permissionStatus() => _api.permissionStatus();

  @override
  void onScanResult(WifiNetwork network) {
    _scanResultsController.add(network);
  }

  @override
  void onScanComplete() {
    _scanEventsController.add(WifiScanComplete());
  }

  @override
  void onScanError(String message) {
    _scanEventsController.add(WifiScanError(message));
  }
}
