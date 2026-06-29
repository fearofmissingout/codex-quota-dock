// swift-tools-version: 5.10
import PackageDescription

let package = Package(
    name: "CodexQuotaDock",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "CodexQuotaDock", targets: ["CodexQuotaDockApp"]),
        .library(name: "CodexQuotaDockCore", targets: ["CodexQuotaDockCore"]),
    ],
    targets: [
        .target(name: "CodexQuotaDockCore"),
        .executableTarget(
            name: "CodexQuotaDockApp",
            dependencies: ["CodexQuotaDockCore"]
        ),
        .testTarget(
            name: "CodexQuotaDockCoreTests",
            dependencies: ["CodexQuotaDockCore"]
        ),
    ]
)
