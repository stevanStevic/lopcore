# Changelog

All notable changes to LopCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned

-   CoreMQTT reconnect supervision honoring ReconnectConfig
-   OTA update service (esp_https_ota wrapper with rollback/verify)
-   WiFi connection manager with backoff
-   HTTPS helper + presigned-S3 upload service
-   Diagnostics services (error-log ring, coredump reader, BLE paged-read framing)
-   `lopcore_testing` component (fakes of public seams) and on-target test apps
-   Per-tag log level filtering

## [1.1.0] - 2026-07-06

Backfilled release covering all development since the initial import. Consumers previously
pinned untagged commits; from this release on, every merge to develop is tagged.

### Added

-   Provisioning subsystem: `WiFiProvisioning` (BLE/SoftAP via wifi_prov_mgr), AWS Fleet
    Provisioning workflow (`AwsFleetProvisioner<TMqttClient>`), `CertificateManager` with
    corePKCS11 backend, CBOR request/response serializers, `BleDataFramer`, custom BLE
    endpoint framework and AWS credentials endpoint handler
-   LittleFS storage backend (`LittleFsStorage`)
-   SD card storage backend (`SdCardStorage`, SPI + SDMMC)
-   Ring file storage primitive (`detail::RingFileStorage`) and typed circular record file
    (`RingRecordFile<T>`)
-   Ring file log sink (`RingFileSink`)
-   Asynchronous log sink decorator (`QueuedSink`: bounded drop-oldest queue + drain task)
    and MQTT log sink (`MqttLogSink`, publish-callback based)
-   `Logger::removeSink`
-   Manual MQTT processing mode (`CoreMqttClient::processLoop`)
-   Examples 03-07 (state machine, ESP-MQTT, CoreMQTT async/sync, BLE fleet provisioning)

### Changed

-   MQTT clients are standalone classes with compile-time traits; TLS/PKCS#11 transport
    refactored around `ITlsTransport` dependency injection
-   Storage classes use direct construction with config structs and explicit `initialize()`
-   coreMQTT/corePKCS11 include directories exposed as PUBLIC for consumers
-   `CoreMqttClient` mutex is recursive: publish/subscribe/unsubscribe are legal from
    message and connection callbacks; re-entrant `processLoop()` is rejected
-   Version is single-sourced from the VERSION file (CMake parses it; manifest checked in CI)

### Removed (breaking relative to 1.0.0)

-   `StorageFactory`, `IStorage`, `istorage.hpp` (deleted 2025-11; use direct construction)
-   `MqttClientFactory`, `imqtt_client.hpp`, automatic client selection (use `EspMqttClient`
    or `CoreMqttClient` directly)
-   String-based storage constructors (use config structs)

### Fixed

-   MQTT disconnect deadlock and data races on connect/disconnect
-   MQTT callback re-entrancy deadlock; MQTT-transported log sinks no longer deadlock the
    client (QueuedSink decoupling) (PR #9)
-   Server-side publish packet processing in the coreMQTT receive path
-   mbedTLS handshake timeout handling; receive timeout tuning
-   PKCS#11 certificate naming during fleet provisioning
-   Host build of `NvsStorage` (missing `<cstring>`); `SpiffsStorage::initialize()` never
    marked the instance initialized, so every subsequent operation failed

### Tests and CI

-   Host test suite revived: all targets compile and pass again; placebo suites that
    asserted on local variables or tested in-file reimplementations were deleted
-   Storage/MQTT trait headers realigned with the real class APIs
-   CI re-enabled: host tests + version-consistency check on every push/PR (the previous
    workflow had been disabled since 2025-10-23)

### Known issues (tracked for upcoming releases)

-   `TlsConfig::sendTimeout` is not applied to the socket (sends can block indefinitely)
-   `stopProcessLoopTask()`/`stopDrainTask()` force-delete tasks after a grace period
-   `CoreMqttClient` dispatches by exact topic match; wildcard filters never fire
-   `MqttBudget` is created enabled by default but its revive timer is never started
-   `WiFiProvisioning::resetProvisioning()` does not clear the IDF-side credential store
-   Roughly two thirds of the Kconfig options are not yet wired to code

## [1.0.0] - 2025-10-22

### Added

-   Multi-sink logging system with console and file output
-   Colored console logging with ESP-IDF integration
-   File logging with automatic rotation
-   Unified storage abstraction for NVS and SPIFFS
-   Storage factory pattern for easy backend switching
-   RAII file handles with automatic cleanup
-   Dual MQTT client implementation (ESP-MQTT + CoreMQTT)
-   Automatic MQTT client selection based on broker endpoint
-   Message budgeting to prevent flooding
-   Automatic reconnection with exponential backoff
-   MQTT wildcard topic matching (+ and #)
-   TLS transport with mbedTLS integration
-   PKCS#11 provider for secure credential storage
-   Hardware secure element support (ATECC608A compatible)
-   Certificate chain validation
-   ALPN protocol negotiation
-   Dependency injection for TLS transport
-   Comprehensive unit tests with Google Test (97% coverage)
-   Example applications for logging and storage
-   Full API documentation with inline Doxygen comments
-   Kconfig integration for runtime configuration
-   ESP Component Manager manifest

### Changed

-   Migrated from C to modern C++17
-   Restructured headers with lopcore/ namespace prefix
-   Unified source directory structure (src/ instead of per-component)
-   Improved error handling with std::optional
-   Enhanced type safety with enum class

### Fixed

-   ALPN protocol negotiation for ESP-MQTT client
-   MQTT wildcard topic matching implementation
-   Memory leaks in storage operations
-   Thread safety in logger singleton

### Documentation

-   Comprehensive README with usage examples
-   Example applications with detailed comments
-   Header migration guide
-   Testing documentation
-   Architecture documentation

---

[1.1.0]: https://github.com/stevanStevic/lopcore/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/stevanStevic/lopcore/releases/tag/v1.0.0
