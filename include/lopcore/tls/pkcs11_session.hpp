/**
 * @file pkcs11_session.hpp
 * @brief RAII wrapper for PKCS#11 session handle
 *
 * Lightweight wrapper that obtains a session handle from Pkcs11Provider.
 * The handle is owned by the provider, this class just provides RAII semantics
 * for scoped session usage.
 */

#pragma once

#include <esp_err.h>

// Forward declare PKCS#11 types
typedef unsigned long CK_SESSION_HANDLE;

namespace lopcore
{
namespace tls
{

/**
 * @brief RAII wrapper for PKCS#11 session handle
 *
 * This lightweight class obtains a session handle from the Pkcs11Provider
 * singleton. The session is owned by the provider - this class does NOT
 * close the session in its destructor (non-owning wrapper).
 *
 * Usage:
 * @code
 * Pkcs11Session session;
 * if (session.isValid()) {
 *     CK_SESSION_HANDLE handle = session.get();
 *     // Use handle for PKCS#11 operations...
 * }
 * @endcode
 */
class Pkcs11Session
{
public:
    /**
     * @brief Constructor - obtains session from provider
     *
     * Attempts to get a session handle from Pkcs11Provider::instance().
     * Check isValid() before using the handle.
     */
    Pkcs11Session();

    /**
     * @brief Destructor - does NOT close session (non-owning)
     *
     * The session is managed by Pkcs11Provider and persists beyond
     * the lifetime of this wrapper object.
     */
    ~Pkcs11Session() = default;

    /**
     * @brief Get the session handle
     *
     * @return Session handle (may be invalid if construction failed)
     */
    CK_SESSION_HANDLE get() const noexcept
    {
        return session_;
    }

    /**
     * @brief Check if session handle is valid
     *
     * @return true if session was obtained successfully
     */
    bool isValid() const noexcept;

    /**
     * @brief Get the error from session acquisition
     *
     * @return ESP_OK if session is valid, error code otherwise
     */
    esp_err_t error() const noexcept
    {
        return error_;
    }

    // Move semantics (transfer handle reference)
    Pkcs11Session(Pkcs11Session &&other) noexcept : session_(other.session_), error_(other.error_)
    {
        other.session_ = 0; // CK_INVALID_HANDLE
        other.error_ = ESP_ERR_INVALID_STATE;
    }

    Pkcs11Session &operator=(Pkcs11Session &&other) noexcept
    {
        if (this != &other)
        {
            session_ = other.session_;
            error_ = other.error_;
            other.session_ = 0; // CK_INVALID_HANDLE
            other.error_ = ESP_ERR_INVALID_STATE;
        }
        return *this;
    }

    // No copying (session reference should not be duplicated)
    Pkcs11Session(const Pkcs11Session &) = delete;
    Pkcs11Session &operator=(const Pkcs11Session &) = delete;

private:
    CK_SESSION_HANDLE session_; ///< Session handle from provider
    esp_err_t error_;           ///< Error from session acquisition
};

} // namespace tls
} // namespace lopcore
