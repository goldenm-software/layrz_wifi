// swift-tools-version: 5.9
import PackageDescription

let package = Package(
  name: "layrz_wifi",
  platforms: [
    .iOS(.v14),
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
