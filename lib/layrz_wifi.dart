import 'src/messages.g.dart';

export 'src/messages.g.dart' show WifiNetwork, WifiSecurity, WifiPermissionStatus;

class LayrzWifi {
  static final LayrzWifi _instance = LayrzWifi._();
  LayrzWifi._();
  static LayrzWifi get instance => _instance;

  final LayrzWifiApi _api = LayrzWifiApi();

  Future<bool> hasDiscovery() => _api.hasDiscovery();

  Future<bool> hasCurrentSsid() => _api.hasCurrentSsid();

  Future<String?> currentSsid() => _api.currentSsid();

  Future<List<WifiNetwork>> scan() => _api.scan();

  Future<WifiPermissionStatus> ensurePermissions() => _api.ensurePermissions();
}
