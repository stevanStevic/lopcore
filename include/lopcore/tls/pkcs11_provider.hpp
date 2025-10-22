/**
 * @file pkcs11_provider.hpp
 * @brief PKCS#11 provider singleton for session management
 *
 * Thread-safe singleton that manages PKCS#11 session lifecycle.
 * Uses Meyer's singleton pattern for lazy initialization.
 */

#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Forward declare PKCS#11 types to avoid exposing full API
typedef unsigned long CK_SESSION_HANDLE;
typedef unsigned long CK_RV;
struct CK_FUNCTION_LIST;

namespace lopcore
{
namespace tls
{

/**
 * @brief Singleton provider for PKCS#11 session management
 *
 * This class manages the lifecycle of PKCS#11 sessions in a thread-safe manner.
 * It ensures only one session is active at a time and handles initialization/cleanup.
 *
 * Usage:
 * @code
 * auto& provider = Pkcs11Provider::instance();
 * CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
 * esp_err_t err = provider.getSession(&session);
 * if (err == ESP_OK) {
 *     // Use session...
 * }
 * @endcode
 */
class Pkcs11Provider
{
public:
    /**
     * @brief Get singleton instance (thread-safe lazy initialization)
     */
    static Pkcs11Provider &instance();

    /**
     * @brief Get PKCS#11 session handle
     *
     * Returns the active PKCS#11 session. Initializes PKCS#11 if needed.
     *
     * @param[out] session Pointer to store session handle
     * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
     *         ESP_ERR_INVALID_ARG if session is nullptr
     */
    esp_err_t getSession(CK_SESSION_HANDLE *session);

    /**
     * @brief Get PKCS#11 function list
     *
     * @param[out] functionList Pointer to store function list
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t getFunctionList(CK_FUNCTION_LIST **functionList);

    /**
     * @brief Initialize PKCS#11 library
     *
     * This is called automatically by getSession() if not already initialized.
     * Can be called explicitly for early initialization.
     *
     * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already initialized,
     *         ESP_FAIL on PKCS#11 initialization failure
     */
    esp_err_t initialize();

    /**
     * @brief Cleanup PKCS#11 session and finalize library
     *
     * Should be called before application shutdown.
     *
     * @return ESP_OK on success
     */
    esp_err_t cleanup();

    /**
     * @brief Check if PKCS#11 is initialized
     */
    bool isInitialized() const;

    // Delete copy and move to enforce singleton
    Pkcs11Provider(const Pkcs11Provider &) = delete;
    Pkcs11Provider &operator=(const Pkcs11Provider &) = delete;
    Pkcs11Provider(Pkcs11Provider &&) = delete;
    Pkcs11Provider &operator=(Pkcs11Provider &&) = delete;

private:
    /**
     * @brief Private constructor for singleton
     */
    Pkcs11Provider();

    /**
     * @brief Destructor - calls cleanup
     */
    ~Pkcs11Provider();

    /**
     * @brief Initialize PKCS#11 library (internal)
     */
    esp_err_t initializeInternal();

    /**
     * @brief Open PKCS#11 session (internal)
     */
    esp_err_t openSession();

    /**
     * @brief Close PKCS#11 session (internal)
     */
    void closeSession();

    /**
     * @brief Convert PKCS#11 return value to esp_err_t
     */
    static esp_err_t convertPkcs11Error(CK_RV rv);

    // State
    CK_SESSION_HANDLE session_;
    CK_FUNCTION_LIST *functionList_;
    bool initialized_;

    // Thread safety
    SemaphoreHandle_t mutex_;
};

} // namespace tls
} // namespace lopcore
