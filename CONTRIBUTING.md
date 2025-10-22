# Contributing to LopCore

Thank you for your interest in contributing to LopCore! This document provides guidelines and instructions for
contributing.

## Code of Conduct

-   Be respectful and inclusive
-   Focus on constructive feedback
-   Help others learn and grow
-   Keep discussions technical and on-topic

## How to Contribute

### Reporting Bugs

**Before submitting a bug report:**

1. Check existing issues to avoid duplicates
2. Test with the latest version
3. Isolate the problem (minimal reproduction case)

**Bug report should include:**

-   LopCore version
-   ESP-IDF version
-   Hardware (ESP32/S2/S3/C3/etc.)
-   Clear description of the issue
-   Steps to reproduce
-   Expected vs. actual behavior
-   Relevant logs/code snippets

### Suggesting Features

**Feature requests should include:**

-   Use case description
-   Proposed API/interface
-   Benefits to the community
-   Willingness to implement

### Pull Requests

1. **Fork** the repository
2. **Create** a feature branch
    ```bash
    git checkout -b feature/amazing-feature
    ```
3. **Make** your changes
4. **Test** thoroughly
    ```bash
    cd test/unit && ./run_tests.sh
    ```
5. **Commit** with conventional commits
    ```
    feat: add HTTP client support
    fix: resolve memory leak in storage
    docs: update MQTT examples
    test: add tests for TLS transport
    ```
6. **Push** to your fork
7. **Open** a pull request

## Development Setup

### Prerequisites

```bash
# Install ESP-IDF v5.2+
# Clone LopCore
git clone https://github.com/stevanStevic/lopcore.git
cd lopcore
```

### Building

```bash
# Build tests
cd test/unit
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
# Run all tests
ctest --verbose

# Run specific test suite
./test_logging
./test_storage
./test_mqtt
./test_tls
```

## Coding Standards

### C++ Style

-   **Standard**: C++17
-   **Formatting**: Use `.clang-format` in repo
-   **Naming**:
    -   Classes: `PascalCase`
    -   Methods: `camelCase`
    -   Variables: `camelCase`
    -   Constants: `UPPER_SNAKE_CASE`
    -   Namespaces: `lowercase`

### Code Quality

```cpp
// ✅ Good
class StorageFactory
{
public:
    static std::unique_ptr<IStorage> createSpiffs(const std::string &mountPath);

private:
    StorageFactory() = default;  // Singleton pattern
};

// ❌ Bad
class storage_factory {  // Wrong naming
public:
    IStorage* create_spiffs(char* path);  // Raw pointers, C-style
};
```

### Documentation

```cpp
/**
 * @brief Create SPIFFS storage instance
 *
 * Creates a new SPIFFS storage backend mounted at the specified path.
 * The storage is automatically initialized and formatted if needed.
 *
 * @param mountPath SPIFFS mount point (e.g., "/spiffs")
 * @return Unique pointer to storage instance
 * @throws std::runtime_error if mount fails
 *
 * @code
 * auto storage = StorageFactory::createSpiffs("/spiffs");
 * storage->write("config.json", data);
 * @endcode
 */
static std::unique_ptr<IStorage> createSpiffs(const std::string &mountPath);
```

### Error Handling

```cpp
// ✅ Use std::optional for optional values
std::optional<std::string> read(const std::string &key);

// ✅ Use esp_err_t for ESP-IDF operations
esp_err_t write(const std::string &key, const std::string &value);

// ✅ Use exceptions for unrecoverable errors (rarely)
if (!criticalResource)
{
    throw std::runtime_error("Critical resource unavailable");
}
```

### Memory Management

```cpp
// ✅ Use smart pointers
std::unique_ptr<ILogSink> sink = std::make_unique<ConsoleSink>();
std::shared_ptr<ITlsTransport> transport = std::make_shared<MbedtlsTransport>();

// ✅ Use RAII
{
    auto storage = StorageFactory::createSpiffs("/spiffs");
    storage->write("key", "value");
    // Automatically cleaned up on scope exit
}

// ❌ Avoid raw pointers and manual memory management
ILogSink *sink = new ConsoleSink();  // Who deletes this?
```

## Testing Requirements

### Unit Tests

Every new feature must include unit tests:

```cpp
#include <gtest/gtest.h>
#include "lopcore/storage/nvs_storage.hpp"

TEST(NvsStorageTest, WriteAndRead)
{
    auto storage = StorageFactory::createNvs("test");

    ASSERT_TRUE(storage->write("key", "value"));

    auto result = storage->read("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value");
}
```

### Coverage Requirements

-   New code: Minimum 80% coverage
-   Critical paths: 100% coverage
-   Run coverage report:
    ```bash
    cmake -DCMAKE_BUILD_TYPE=Coverage ..
    make coverage
    ```

### Hardware Testing

For hardware-dependent features:

1. Test on real ESP32 hardware
2. Include test results in PR description
3. Test on multiple chip variants if possible

## Documentation Requirements

### Code Documentation

-   All public APIs must have Doxygen comments
-   Include usage examples in comments
-   Document pre-conditions and post-conditions
-   Explain non-obvious implementation details

### README Updates

Update README.md when:

-   Adding new features
-   Changing public API
-   Adding dependencies
-   Updating requirements

### Example Applications

When adding major features:

-   Create example application in `examples/`
-   Include detailed README
-   Show best practices
-   Cover common use cases

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

**Types:**

-   `feat`: New feature
-   `fix`: Bug fix
-   `docs`: Documentation only
-   `style`: Formatting, no code change
-   `refactor`: Code restructuring
-   `perf`: Performance improvement
-   `test`: Adding tests
-   `chore`: Maintenance tasks

**Examples:**

```
feat(mqtt): add QoS 2 support for CoreMQTT client

fix(storage): resolve memory leak in SPIFFS cleanup
Closes #42

docs(readme): update TLS configuration examples

test(tls): add integration tests for PKCS#11 provider
```

## Review Process

### What Reviewers Look For

1. **Correctness**: Does it work as intended?
2. **Tests**: Adequate test coverage?
3. **Documentation**: Clear and complete?
4. **Style**: Follows coding standards?
5. **Performance**: No unnecessary overhead?
6. **Compatibility**: Works with ESP-IDF versions?

### Response Time

-   Initial review: Within 1 week
-   Follow-up: Within 3 days
-   Maintainers merge after approval

## Release Process

LopCore follows Semantic Versioning:

-   **MAJOR**: Breaking API changes
-   **MINOR**: New features, backward compatible
-   **PATCH**: Bug fixes, backward compatible

## Questions?

-   Open a discussion on GitHub
-   Check existing documentation
-   Ask in pull request comments

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
