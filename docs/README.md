# LopCore Documentation

Comprehensive documentation for LopCore middleware components.

---

## 📚 Component Guides

### MQTT

-   **[MQTT Client Selection Guide](./MQTT_CLIENTS.md)** - Detailed comparison of ESP-MQTT vs CoreMQTT, when
    to use each, and why AWS IoT uses CoreMQTT

### Storage

-   Storage API Guide _(coming soon)_

### Logging

-   Logging Configuration Guide _(coming soon)_

### TLS

-   TLS and PKCS#11 Guide _(coming soon)_

### State Machine

-   State Machine Patterns _(coming soon)_

---

## 🚀 Quick Links

-   [Main README](../README.md) - Project overview and quick start
-   [Examples](../examples/) - Working code examples for all components
-   [API Reference](../include/) - Header files with inline documentation
-   [Header Standards](./HEADER_STANDARDS.md) - File header format and conventions
-   [Contributing](../CONTRIBUTING.md) - How to contribute
-   [Changelog](../CHANGELOG.md) - Version history

---

## 🔍 Finding Help

### By Use Case

| What You Want to Do          | Documentation                                       | Example                                               |
| ---------------------------- | --------------------------------------------------- | ----------------------------------------------------- |
| Connect to AWS IoT Core      | [MQTT Clients](./MQTT_CLIENTS.md)                   | [CoreMQTT Async](../examples/05_mqtt_coremqtt_async/) |
| Connect to MQTT broker       | [MQTT Clients](./MQTT_CLIENTS.md)                   | [ESP-MQTT Client](../examples/04_mqtt_esp_client/)    |
| Implement Fleet Provisioning | [MQTT Clients](./MQTT_CLIENTS.md#manual-processing) | [CoreMQTT Sync](../examples/06_mqtt_coremqtt_sync/)   |
| Store configuration          | Storage Guide                                       | [Storage Basics](../examples/02_storage_basics/)      |
| Add logging                  | Logging Guide                                       | [Basic Logging](../examples/01_basic_logging/)        |
| Build state machine          | State Machine Guide                                 | [State Machine](../examples/03_state_machine/)        |

### By Component

-   **MQTT**: Start with [MQTT_CLIENTS.md](./MQTT_CLIENTS.md)
-   **Storage**: See [examples/02_storage_basics](../examples/02_storage_basics/)
-   **Logging**: See [examples/01_basic_logging](../examples/01_basic_logging/)
-   **State Machine**: See [examples/03_state_machine](../examples/03_state_machine/)

---

## 📖 Documentation Standards

All LopCore documentation follows these standards:

### File Headers

-   Follow [Header Standards](./HEADER_STANDARDS.md) for all source files
-   Use Doxygen format with `@file`, `@brief`, `@copyright`, `@license`
-   Include descriptive comments about purpose and features

### Code Examples

-   Must be **complete** and **compilable**
-   Include **all necessary includes**
-   Show **error handling**
-   Demonstrate **best practices**

### Formatting

-   Use **Markdown** for all documentation
-   Include **code blocks** with syntax highlighting
-   Use **tables** for comparisons
-   Include **diagrams** where helpful (ASCII or Mermaid)

### Structure

-   Start with **Quick Example** (< 20 lines)
-   Explain **When to Use**
-   List **Key Features**
-   Provide **Detailed Examples**
-   Include **Common Pitfalls**
-   End with **Further Reading**

---

## 🤝 Contributing Documentation

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines on:

-   Writing new documentation
-   Improving existing docs
-   Adding examples
-   Reporting documentation issues
