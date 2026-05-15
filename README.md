# layrz_wifi

[![Pub version](https://img.shields.io/pub/v/layrz_wifi?logo=flutter)](https://pub.dev/packages/layrz_wifi)
[![Pub Points](https://img.shields.io/pub/points/layrz_wifi)](https://pub.dev/packages/layrz_wifi/score)
[![likes](https://img.shields.io/pub/likes/layrz_wifi?logo=flutter)](https://pub.dev/packages/layrz_wifi/score)
[![GitHub license](https://img.shields.io/github/license/goldenm-software/layrz_wifi?logo=github)](https://github.com/goldenm-software/layrz_wifi)

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

## FAQ

### Why is this package called `layrz_wifi`?
All packages developed by [Layrz](https://layrz.com) are prefixed with `layrz_`, check out our other packages on [pub.dev](https://pub.dev/publishers/goldenm.com/packages).

### I need to pay to use this package?
<b>No!</b> This library is free and open source, you can use it in your projects without any cost, but if you want to support us, give us a thumbs up here in [pub.dev](https://pub.dev/packages/layrz_wifi) and star our [Repository](https://github.com/goldenm-software/layrz_wifi)!

### Can I contribute to this package?
<b>Yes!</b> We are open to contributions, feel free to open a pull request or an issue on the [Repository](https://github.com/goldenm-software/layrz_wifi)!

### I have a question, how can I contact you?
If you need more assistance, you open an issue on the [Repository](https://github.com/goldenm-software/layrz_wifi) and we're happy to help you :)

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

This project is maintained by [Golden M](https://goldenm.com) with authorization of [Layrz LTD](https://layrz.com).

## Who are you? / Want to work with us?

<b>Golden M</b> is a software and hardware development company what is working on a new, innovative and disruptive technologies. For more information, contact us at [sales@goldenm.com](mailto:sales@goldenm.com) or via WhatsApp at [+(507)-6979-3073](https://wa.me/50769793073?text="From%20layrz_wifi%20flutter%20library.%20Hello").
