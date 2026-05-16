# Changelog

## 1.0.1

- Fixes on Windows, now it really scans for WiFi networks instead of just returning an empty list.

## 1.0.0

- Replace blocking `scan()` with async `startScan()` / `stopScan()` and a `scanResults` broadcast stream for progressive per-network callbacks.
- Add `scanEvents` stream emitting `WifiScanComplete` and `WifiScanError` lifecycle events.
- Replace `ensurePermissions()` with `requestPermissions()` (returns `bool`) and `permissionStatus()` (returns `WifiPermissionStatus`).
- Unify iOS and macOS into a single `darwin/` platform using `#if os(iOS)` / `#if os(macOS)` conditionals and `sharedDarwinSource: true`.
- Add Swift Package Manager support via `darwin/layrz_wifi/Package.swift`.
- Fix macOS: requires `com.apple.security.network.client` entitlement for `CWWiFiClient` access under sandbox.
- Fix macOS: `CLLocationManager` permission dialog shown via plugin on first `requestPermissions()` call.
- Fix Android: package renamed to `com.layrz.layrz_wifi`; permissions must be declared explicitly in app manifest.
- Fix iOS: `hasCurrentSsid` returns `false` to avoid `NEHotspotNetwork.fetchCurrent` deadlock on main thread.
- iOS and Web are no-op platforms — no WiFi scan API is available on either.

## 0.1.0

* Initial release.
* WiFi network scanning on Android, macOS, Windows, and Linux.
* Current SSID reading on Android, iOS, macOS, Windows, and Linux.
* Web stub returns `false` for all capability probes.
* Pigeon-generated type-safe platform channels.
