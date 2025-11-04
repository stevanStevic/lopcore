# LopCore Header Standards

This document defines the standard file header format for all LopCore source files.

---

## Standard Header Format

All `.hpp` and `.cpp` files should begin with a Doxygen-style header comment:

```cpp
/**
 * @file <filename>
 * @brief <one-line description>
 *
 * <multi-line detailed description>
 * <additional context, features, use cases>
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */
```

---

## Header Components

### Required Fields

1. **@file** - The filename (must match actual filename)
2. **@brief** - One-line summary (ends without period)
3. **Description** - Multi-line explanation (optional but recommended)
4. **@copyright** - Copyright notice
5. **@license** - License type

### Example - Header File (.hpp)

```cpp
/**
 * @file mqtt_client.hpp
 * @brief MQTT client interface and implementation
 *
 * Provides abstract interface and concrete implementations for MQTT
 * connectivity. Supports both ESP-MQTT and CoreMQTT backends with
 * unified API surface.
 *
 * Features:
 * - Pub/sub operations with QoS support
 * - TLS/SSL encrypted connections
 * - Automatic reconnection handling
 * - Message callbacks and statistics
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once
// ... rest of header
```

### Example - Source File (.cpp)

```cpp
/**
 * @file mqtt_client.cpp
 * @brief Implementation of MQTT client
 *
 * Wraps ESP-IDF mqtt_client library with C++ abstractions and
 * integrates with LopCore logging and configuration systems.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/mqtt/mqtt_client.hpp"
// ... rest of implementation
```

### Example - Example File (main.cpp)

```cpp
/**
 * @file main.cpp
 * @brief CoreMQTT Async Client Example
 *
 * Demonstrates basic MQTT communication using CoreMQTT client with TLS:
 * - Secure TLS connection
 * - Automatic message processing with callbacks
 * - Request-response pattern
 * - Async pub/sub operations
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include <stdio.h>
// ... rest of example
```

---

## Guidelines

### Description Content

The description section should include:

1. **Primary Purpose** - What the file does
2. **Key Features** - Bullet list of capabilities (if applicable)
3. **Usage Context** - When to use this component
4. **Integration Notes** - How it fits with other components

### Description Style

- Use present tense ("Provides", "Implements", "Manages")
- Start with action verbs
- Be concise but informative
- Use bullet lists for features
- Avoid implementation details (save for inline comments)

### What to Include

✅ **Good descriptions:**
- High-level purpose and role
- Key features and capabilities
- Integration points
- When to use this component
- Platform support notes (ESP32 vs host)

❌ **Avoid in descriptions:**
- Implementation details (belongs in inline comments)
- Version history (belongs in CHANGELOG)
- TODO items (use inline // TODO comments)
- Author names (git history tracks this)

---

## Component-Specific Examples

### MQTT Components

```cpp
/**
 * @file coremqtt_client.hpp
 * @brief AWS CoreMQTT library wrapper
 *
 * Wraps AWS IoT coreMQTT library with C++ RAII patterns and integrates
 * with LopCore logging, configuration, and TLS transport.
 *
 * Features:
 * - Stateful QoS tracking for AWS IoT reliability
 * - Manual processing capability (processLoop)
 * - TLS/PKCS#11 transport support
 * - Integration with backoff algorithm
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */
```

### Storage Components

```cpp
/**
 * @file spiffs_storage.hpp
 * @brief SPIFFS storage implementation
 *
 * Provides file-based storage using ESP-IDF's SPIFFS library.
 * Supports both ESP32 platform and host-based testing.
 *
 * Best suited for:
 * - Log files and certificates
 * - Configuration files
 * - Small to medium files (<100KB typically)
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */
```

### TLS Components

```cpp
/**
 * @file mbedtls_transport.hpp
 * @brief MbedTLS transport with PKCS#11 support
 *
 * Concrete implementation of ITlsTransport using MbedTLS and PKCS#11.
 * Provides secure TLS connections with hardware security module support
 * for credential management.
 *
 * Features:
 * - PKCS#11 integration for secure credential storage
 * - Exponential backoff retry logic
 * - ALPN protocol support
 * - Server Name Indication (SNI)
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */
```

### Logging Components

```cpp
/**
 * @file logger.hpp
 * @brief Multi-sink logging system
 *
 * Provides flexible logging with configurable log levels,
 * multiple output sinks, and platform-agnostic timestamping.
 *
 * Supports:
 * - Console and file sinks
 * - Colored output
 * - Log rotation
 * - Thread-safe operations
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */
```

---

## Copyright and License

### Standard Format

```cpp
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
```

### Notes

- **Year**: Use current year (update when making significant changes)
- **Contributors**: Use "LopCore Contributors" (not individual names)
- **License**: Currently MIT License (update if changed)

---

## Header Guards vs #pragma once

### Preference: Use `#pragma once`

✅ **Preferred:**
```cpp
/**
 * @file example.hpp
 * @brief Example header
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

// ... rest of header
```

⚠️ **Legacy (still acceptable):**
```cpp
/**
 * @file example.hpp
 * @brief Example header
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#ifndef LOPCORE_EXAMPLE_HPP
#define LOPCORE_EXAMPLE_HPP

// ... rest of header

#endif // LOPCORE_EXAMPLE_HPP
```

### Rationale

- `#pragma once` is simpler and less error-prone
- Supported by all modern compilers
- No naming conflicts with include guards
- Some existing files use traditional guards (okay to keep)

---

## Checklist

When creating or updating a file, ensure:

- [ ] Header comment present at top of file
- [ ] `@file` matches actual filename
- [ ] `@brief` provides clear one-line summary
- [ ] Description explains purpose and features
- [ ] `@copyright` includes year and "LopCore Contributors"
- [ ] `@license` specifies "MIT License"
- [ ] Header uses `#pragma once` (for .hpp files)
- [ ] Consistent formatting with existing files

---

## Tools and Automation

### Checking Headers

```bash
# Find files without headers
find include src examples -name "*.hpp" -o -name "*.cpp" | \
  while read f; do 
    head -1 "$f" | grep -q "^/\*\*" || echo "Missing: $f"
  done

# Verify copyright format
grep -r "@copyright" include src examples | \
  grep -v "LopCore Contributors"
```

### Template for New Files

Save as `.vscode/templates/cpp_file.cpp`:

```cpp
/**
 * @file ${TM_FILENAME}
 * @brief ${1:Brief description}
 *
 * ${2:Detailed description}
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include "${3:header_file.hpp}"

$0
```

---

## Questions?

- See existing files in `include/` and `src/` for reference
- Check `examples/` for example file headers
- Refer to main [README](../README.md) for project overview

---

**Last Updated:** 2025-11-04  
**Standardization Commit:** 8cc7382
