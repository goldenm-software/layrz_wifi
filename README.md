# layrz_wifi

A Flutter plugin to scan nearby WiFi networks and read the currently connected SSID on Android, macOS, Windows, and Linux.

All platform communication uses [Pigeon](https://pub.dev/packages/pigeon) for type-safe, generated channel code.

## Platform capability matrix

| Platform | `hasDiscovery()` | `hasCurrentSsid()` | Notes |
|---|:---:|:---:|---|
| 🤖 Android | ✅ | ✅ | Requires `ACCESS_FINE_LOCATION` (API < 33) or `NEARBY_WIFI_DEVICES` (API 33+) |
| 🍎 iOS | ❌ | ❌ | No WiFi scan API available. `startScan` returns an error. |
| 🍏 macOS | ✅ | ✅ | Requires `com.apple.developer.networking.wifi-info` entitlement + location permission |
| 🪟 Windows | ✅ | ✅ | Uses `wlanapi.dll` — no special entitlement required |
| 🐧 Linux | ✅ | ✅ | Requires `nmcli` (NetworkManager CLI) to be installed |
| 🌐 Web | ❌ | ❌ | Browsers block all WiFi APIs |

## Quick start

```dart
import 'package:layrz_wifi/layrz_wifi.dart';

final wifi = LayrzWifi.instance;

// Check what this platform supports
final canScan = await wifi.hasDiscovery();
final canReadSsid = await wifi.hasCurrentSsid();

// Request permissions before scanning
final granted = await wifi.requestPermissions();
if (!granted) {
  final status = await wifi.permissionStatus();
  print('Permission denied: $status');
  return;
}

// Read the current SSID
if (canReadSsid) {
  final ssid = await wifi.currentSsid(); // null if not connected
}

// Scan for nearby networks
if (canScan) {
  wifi.scanResults.listen((network) {
    print('${network.ssid} (${network.security.name}) ${network.signalDbm} dBm');
  });

  wifi.scanEvents.listen((event) {
    if (event is WifiScanComplete) print('Scan complete');
    if (event is WifiScanError) print('Scan error: ${event.message}');
  });

  await wifi.startScan();
  // Call stopScan() when done
}
```

## Per-platform setup

### Android

Add to `android/app/src/main/AndroidManifest.xml` inside `<manifest>`:

```xml
<!-- API < 33 -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"
    android:maxSdkVersion="32" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"
    android:maxSdkVersion="32" />
<!-- API 33+ -->
<uses-permission android:name="android.permission.NEARBY_WIFI_DEVICES"
    android:usesPermissionFlags="neverForLocation" />
<!-- Always required -->
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_STATE" />
```

### iOS

iOS has no public WiFi scanning API. `hasDiscovery()` and `hasCurrentSsid()` both return `false`. The plugin is a no-op on iOS.

### macOS

1. Add to `macos/Runner/DebugProfile.entitlements` and `Release.entitlements`:

```xml
<key>com.apple.developer.networking.wifi-info</key>
<true/>
<key>com.apple.security.network.client</key>
<true/>
<key>com.apple.security.personal-information.location</key>
<true/>
```

2. Add to `macos/Runner/Info.plist`:

```xml
<key>NSLocationWhenInUseUsageDescription</key>
<string>Location access is required to scan for nearby Wi-Fi networks.</string>
<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
<string>Location access is required to scan for nearby Wi-Fi networks.</string>
```

3. Call `requestPermissions()` before scanning — the plugin will show the system location dialog on first call.

### Windows

No setup required. `wlanapi.dll` is part of Windows and is linked automatically.

### Linux

Install NetworkManager and its CLI tool:

```bash
sudo apt install network-manager  # Debian/Ubuntu
sudo dnf install NetworkManager   # Fedora/RHEL
```

### Web

No setup. Both `hasDiscovery()` and `hasCurrentSsid()` return `false`. Browsers block all WiFi APIs.

## API

```dart
class LayrzWifi {
  static LayrzWifi get instance;

  Future<bool> hasDiscovery();
  Future<bool> hasCurrentSsid();
  Future<String?> currentSsid();

  Future<bool> requestPermissions();
  Future<WifiPermissionStatus> permissionStatus();

  Stream<WifiNetwork> get scanResults;
  Stream<WifiScanEvent> get scanEvents;

  Future<void> startScan();
  Future<void> stopScan();
}

class WifiNetwork {
  final String ssid;
  final String? bssid;
  final int? signalDbm;
  final int? frequencyMhz;
  final WifiSecurity security;
  final bool isHidden;
}

sealed class WifiScanEvent {}
class WifiScanComplete extends WifiScanEvent {}
class WifiScanError extends WifiScanEvent {
  final String message;
}

enum WifiSecurity { open, wep, wpa, wpa2, wpa3, unknown }
enum WifiPermissionStatus { granted, denied, permanentlyDenied, restricted, notRequired }
```

## Regenerating Pigeon bindings

After modifying `pigeons/messages.dart`:

```bash
dart run pigeon --input pigeons/messages.dart
```

The Swift output goes to `darwin/layrz_wifi/Sources/layrz_wifi/Messages.g.swift` — no manual copying needed.

## License

MIT — see [LICENSE](LICENSE).
