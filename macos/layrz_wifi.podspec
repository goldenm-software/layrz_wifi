Pod::Spec.new do |s|
  s.name             = 'layrz_wifi'
  s.version          = '0.1.0'
  s.summary          = 'Flutter plugin to scan nearby WiFi networks and read current SSID.'
  s.description      = 'Flutter plugin that exposes WiFi network discovery and current-network info on every Flutter-supported platform.'
  s.homepage         = 'https://github.com/goldenm-software/layrz_wifi'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Golden M, Inc.' => 'kenny@goldenm.com' }

  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'

  s.resource_bundles = {'layrz_wifi_privacy' => ['Resources/PrivacyInfo.xcprivacy']}

  s.dependency 'FlutterMacOS'

  s.platform = :osx, '10.15'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
