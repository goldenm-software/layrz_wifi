// swift-tools-version: 5.5
import PackageDescription

let package = Package(
  name: "layrz_wifi",
  platforms: [
    .macOS("12.0"),
    .iOS("14.0"),
  ],
  products: [
    .library(name: "layrz-wifi", targets: ["layrz_wifi"]),
  ],
  dependencies: [],
  targets: [
    .target(
      name: "layrz_wifi",
      dependencies: [],
      resources: [
        .process("PrivacyInfo.xcprivacy"),
      ]
    ),
  ]
)
