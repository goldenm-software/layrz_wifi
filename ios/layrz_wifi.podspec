Pod::Spec.new do |s|
  s.name             = 'layrz_wifi'
  s.version          = '0.1.0'
  s.summary          = 'Flutter plugin to scan nearby WiFi networks and read current SSID.'
  s.description      = 'Flutter plugin that exposes WiFi network discovery and current-network info on every Flutter-supported platform.'
  s.homepage         = 'https://github.com/goldenm-software/layrz_wifi'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Golden M, Inc.' => 'kenny@goldenm.com' }
  s.source           = { :path => '.' }
  s.source_files = '../darwin/layrz_wifi/Sources/**/*.swift'
  s.dependency 'Flutter'
  s.platform = :ios, '14.0'

  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'

  s.resource_bundles = {'layrz_wifi_privacy' => ['../darwin/layrz_wifi/Sources/layrz_wifi/PrivacyInfo.xcprivacy']}
end
