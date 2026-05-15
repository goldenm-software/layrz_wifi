# layrz_wifi

> 🤖 Generated using AI Assistance, you may find optimizations along the plugin, feel free to open a Pull Request.

A Flutter plugin to scan nearby WiFi networks and read the currently connected SSID on Android, iOS, macOS, Windows, Linux, and Web.

All platform communication uses [Pigeon](https://pub.dev/packages/pigeon) for type-safe, generated channel code.

## Platform capability matrix

| Platform | `hasDiscovery()` | `hasCurrentSsid()` | Notes |
|---|:---:|:---:|---|
| 🤖 Android | ✅ | ✅ | Requires `ACCESS_FINE_LOCATION` (API < 33) or `NEARBY_WIFI_DEVICES` (API 33+) |
| 🍎 iOS | ❌ | ✅ | Requires **Access WiFi Information** entitlement + `NSLocationWhenInUseUsageDescription` |
| 🍏 macOS | ✅ | ✅ | Requires `com.apple.developer.networking.wifi-info` entitlement + location authorization |
| 🪟 Windows | ✅ | ✅ | Uses `wlanapi.dll` — no special entitlement required |
| 🐧 Linux | ✅ | ✅ | Requires `nmcli` (NetworkManager CLI) to be installed |
| 🌐 Web | ❌ | ❌ | Browsers block all WiFi APIs — manual SSID entry only |

## Quick start

```dart
import 'package:layrz_wifi/layrz_wifi.dart';

final wifi = LayrzWifi.instance;

// Check what this platform supports
final canScan = await wifi.hasDiscovery();
final canReadSsid = await wifi.hasCurrentSsid();

// Request permissions (required on Android and iOS)
final perm = await wifi.ensurePermissions();
if (perm != WifiPermissionStatus.granted && perm != WifiPermissionStatus.notRequired) {
  // Show UI telling the user to grant permission
  return;
}

// Read the current SSID
if (canReadSsid) {
  final ssid = await wifi.currentSsid(); // null if not connected
}

// Scan for nearby networks (not supported on iOS or Web)
if (canScan) {
  final List<WifiNetwork> networks = await wifi.scan();
  for (final n in networks) {
    print('${n.ssid} (${n.security.name}) ${n.signalDbm} dBm');
  }
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

The plugin declares these permissions in its own `AndroidManifest.xml`, which gets merged automatically.

### iOS

1. Enable the **Access WiFi Information** capability for your app in Xcode (Signing & Capabilities tab) **and** on the Apple Developer portal for your App ID.
2. Add to `ios/Runner/Info.plist`:

```xml
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs location access to read the WiFi network name.</string>
```

> **Note**: `hasDiscovery()` always returns `false` on iOS. WiFi scanning requires `NEHotspotHelper`, an Apple-gated MFi-tier entitlement that is not available for general distribution.

### macOS

1. Enable **com.apple.developer.networking.wifi-info** in your macOS entitlements files (`DebugProfile.entitlements` and `Release.entitlements`):

```xml
<key>com.apple.developer.networking.wifi-info</key>
<true/>
```

2. Add to `macos/Runner/Info.plist`:

```xml
<key>NSLocationUsageDescription</key>
<string>This app needs location access to scan WiFi networks.</string>
```

3. Minimum deployment target: **macOS 10.15** (CoreWLAN scan requires location authorization since 10.15).

### Windows

No setup required. `wlanapi.dll` is part of Windows and is linked automatically.

### Linux

Install NetworkManager and its CLI tool:

```bash
sudo apt install network-manager  # Debian/Ubuntu
sudo dnf install NetworkManager   # Fedora/RHEL
```

The plugin shells out to `nmcli` for both scan and current SSID. A future version will use the D-Bus NetworkManager API directly.

### Web

No setup. Both `hasDiscovery()` and `hasCurrentSsid()` return `false`. Show a manual text input instead.

## Why can't I read the list of saved networks?

Every modern OS (Android, iOS, macOS, Windows, Linux, and browsers) deliberately blocks third-party apps from reading the device's stored WiFi credentials and saved network list as a security and privacy measure. This plugin instead **scans nearby networks in real time** and reads the **currently connected SSID**, which covers the practical use case of picking a network from a list during device provisioning.

## API

```dart
class LayrzWifi {
  static LayrzWifi get instance;

  Future<bool> hasDiscovery();
  Future<bool> hasCurrentSsid();
  Future<String?> currentSsid();
  Future<List<WifiNetwork>> scan();
  Future<WifiPermissionStatus> ensurePermissions();
}

class WifiNetwork {
  final String ssid;
  final String? bssid;
  final int? signalDbm;
  final int? frequencyMhz;
  final WifiSecurity security;
  final bool isHidden;
}

enum WifiSecurity { open, wep, wpa, wpa2, wpa3, unknown }
enum WifiPermissionStatus { granted, denied, permanentlyDenied, restricted, notRequired }
```

## Regenerating Pigeon bindings

After modifying `pigeons/messages.dart`:

```bash
dart run pigeon --input pigeons/messages.dart
```

Commit the regenerated `lib/src/messages.g.dart` and the native generated files.

## License

MIT — see [LICENSE](LICENSE).
