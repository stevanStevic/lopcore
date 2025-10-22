/**
 * @file pkcs11_session.cpp
 * @brief Implementation of PKCS#11 session wrapper
 */

#include "lopcore/tls/pkcs11_session.hpp"

#include <core_pkcs11.h>
#include <esp_log.h>

#include "lopcore/tls/pkcs11_provider.hpp"

static const char *TAG = "Pkcs11Session";

namespace lopcore
{
namespace tls
{

Pkcs11Session::Pkcs11Session() : session_(CK_INVALID_HANDLE), error_(ESP_FAIL)
{
    // Get session from provider singleton
    auto &provider = Pkcs11Provider::instance();
    error_ = provider.getSession(&session_);

    if (error_ != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get PKCS#11 session: %s", esp_err_to_name(error_));
        session_ = CK_INVALID_HANDLE;
    }
    else
    {
        ESP_LOGD(TAG, "Obtained session handle: %lu", session_);
    }
}

bool Pkcs11Session::isValid() const noexcept
{
    return (session_ != CK_INVALID_HANDLE) && (error_ == ESP_OK);
}

} // namespace tls
} // namespace lopcore
