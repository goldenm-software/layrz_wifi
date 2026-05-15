import 'package:pigeon/pigeon.dart';

@ConfigurePigeon(
  PigeonOptions(
    dartOut: 'lib/src/messages.g.dart',
    dartOptions: DartOptions(),
    kotlinOut: 'android/src/main/kotlin/com/goldenm/layrz_wifi/Messages.g.kt',
    kotlinOptions: KotlinOptions(package: 'com.goldenm.layrz_wifi'),
    swiftOut: 'ios/Classes/Messages.g.swift',
    gobjectHeaderOut: 'linux/messages.g.h',
    gobjectSourceOut: 'linux/messages.g.cc',
    cppHeaderOut: 'windows/messages.g.h',
    cppSourceOut: 'windows/messages.g.cpp',
    cppOptions: CppOptions(namespace: 'layrz_wifi'),
    copyrightHeader: 'pigeons/copyright.txt',
  ),
)

enum WifiSecurity { open, wep, wpa, wpa2, wpa3, unknown }

enum WifiPermissionStatus { granted, denied, permanentlyDenied, restricted, notRequired }

class WifiNetwork {
  WifiNetwork({
    required this.ssid,
    this.bssid,
    this.signalDbm,
    this.frequencyMhz,
    required this.security,
    required this.isHidden,
  });

  String ssid;
  String? bssid;
  int? signalDbm;
  int? frequencyMhz;
  WifiSecurity security;
  bool isHidden;
}

@HostApi()
abstract class LayrzWifiApi {
  bool hasDiscovery();
  bool hasCurrentSsid();
  String? currentSsid();
  List<WifiNetwork> scan();
  WifiPermissionStatus ensurePermissions();
}
