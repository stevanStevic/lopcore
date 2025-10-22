# Changelog

All notable changes to LopCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

## [Unreleased]

### Planned

-   SD card storage backend
-   Per-tag log level filtering
-   Cloud storage abstraction (AWS S3, etc.)
-   HTTP client with connection pooling
-   WebSocket client
-   OTA update framework
-   More example applications

---

[1.0.0]: https://github.com/stevanStevic/lopcore/releases/tag/v1.0.0
