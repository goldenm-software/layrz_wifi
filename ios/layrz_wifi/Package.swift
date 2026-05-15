// swift-tools-version: 5.9
import PackageDescription

// Sources live in darwin/ — see sharedDarwinSource in pubspec.yaml.
let package = Package(
  name: "layrz_wifi",
  platforms: [.iOS(.v14)],
  products: [
    .library(name: "layrz-wifi", targets: ["layrz_wifi"]),
  ],
  targets: [
    .target(name: "layrz_wifi"),
  ]
)
