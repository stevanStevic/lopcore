/**
 * @file pkcs11_provider.cpp
 * @brief Implementation of PKCS#11 provider singleton
 */

#include "lopcore/tls/pkcs11_provider.hpp"

#include <core_pkcs11.h>
#include <esp_log.h>

static const char *TAG = "Pkcs11Provider";

namespace lopcore
{
namespace tls
{

// Meyer's singleton - thread-safe in C++11+
Pkcs11Provider &Pkcs11Provider::instance()
{
    static Pkcs11Provider instance;
    return instance;
}

Pkcs11Provider::Pkcs11Provider()
    : session_(CK_INVALID_HANDLE), functionList_(nullptr), initialized_(false), mutex_(nullptr)
{
    // Create mutex for thread safety
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
}

Pkcs11Provider::~Pkcs11Provider()
{
    cleanup();
    if (mutex_ != nullptr)
    {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

esp_err_t Pkcs11Provider::getSession(CK_SESSION_HANDLE *session)
{
    if (session == nullptr)
    {
        ESP_LOGE(TAG, "Session pointer is null");
        return ESP_ERR_INVALID_ARG;
    }

    // Lock for thread safety
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    // Initialize if needed
    if (!initialized_)
    {
        esp_err_t err = initializeInternal();
        if (err != ESP_OK)
        {
            xSemaphoreGive(mutex_);
            return err;
        }
    }

    // Open session if needed
    if (session_ == CK_INVALID_HANDLE)
    {
        esp_err_t err = openSession();
        if (err != ESP_OK)
        {
            xSemaphoreGive(mutex_);
            return err;
        }
    }

    *session = session_;
    xSemaphoreGive(mutex_);

    ESP_LOGD(TAG, "Session handle: %lu", session_);
    return ESP_OK;
}

esp_err_t Pkcs11Provider::getFunctionList(CK_FUNCTION_LIST **functionList)
{
    if (functionList == nullptr)
    {
        ESP_LOGE(TAG, "Function list pointer is null");
        return ESP_ERR_INVALID_ARG;
    }

    // Lock for thread safety
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    // Initialize if needed
    if (!initialized_)
    {
        esp_err_t err = initializeInternal();
        if (err != ESP_OK)
        {
            xSemaphoreGive(mutex_);
            return err;
        }
    }

    *functionList = functionList_;
    xSemaphoreGive(mutex_);

    return ESP_OK;
}

esp_err_t Pkcs11Provider::initialize()
{
    // Lock for thread safety
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    if (initialized_)
    {
        ESP_LOGW(TAG, "PKCS#11 already initialized");
        xSemaphoreGive(mutex_);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = initializeInternal();
    xSemaphoreGive(mutex_);

    return err;
}

esp_err_t Pkcs11Provider::cleanup()
{
    // Lock for thread safety
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    if (!initialized_)
    {
        xSemaphoreGive(mutex_);
        return ESP_OK; // Nothing to cleanup
    }

    // Close session if open
    if (session_ != CK_INVALID_HANDLE)
    {
        closeSession();
    }

    // Finalize PKCS#11
    if (functionList_ != nullptr && functionList_->C_Finalize != nullptr)
    {
        CK_RV rv = functionList_->C_Finalize(nullptr);
        if (rv != CKR_OK)
        {
            ESP_LOGW(TAG, "C_Finalize failed: 0x%lx", rv);
        }
        else
        {
            ESP_LOGI(TAG, "PKCS#11 finalized");
        }
    }

    initialized_ = false;
    functionList_ = nullptr;
    xSemaphoreGive(mutex_);

    return ESP_OK;
}

bool Pkcs11Provider::isInitialized() const
{
    return initialized_;
}

// Private methods

esp_err_t Pkcs11Provider::initializeInternal()
{
    ESP_LOGI(TAG, "Initializing PKCS#11...");

    // Get function list
    CK_RV rv = C_GetFunctionList(&functionList_);
    if (rv != CKR_OK)
    {
        ESP_LOGE(TAG, "C_GetFunctionList failed: 0x%lx", rv);
        return convertPkcs11Error(rv);
    }

    if (functionList_ == nullptr || functionList_->C_Initialize == nullptr)
    {
        ESP_LOGE(TAG, "Invalid function list");
        return ESP_FAIL;
    }

    // Initialize PKCS#11
    rv = functionList_->C_Initialize(nullptr);
    if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED)
    {
        ESP_LOGE(TAG, "C_Initialize failed: 0x%lx", rv);
        return convertPkcs11Error(rv);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "PKCS#11 initialized successfully");

    return ESP_OK;
}

esp_err_t Pkcs11Provider::openSession()
{
    if (functionList_ == nullptr || functionList_->C_OpenSession == nullptr)
    {
        ESP_LOGE(TAG, "Function list not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Opening PKCS#11 session...");

    // Open session with default slot (0)
    CK_SLOT_ID slotId = 0;
    CK_FLAGS flags = CKF_SERIAL_SESSION | CKF_RW_SESSION;

    CK_RV rv = functionList_->C_OpenSession(slotId, flags,
                                            nullptr, // Application context
                                            nullptr, // Callback
                                            &session_);

    if (rv != CKR_OK)
    {
        ESP_LOGE(TAG, "C_OpenSession failed: 0x%lx", rv);
        session_ = CK_INVALID_HANDLE;
        return convertPkcs11Error(rv);
    }

    ESP_LOGI(TAG, "PKCS#11 session opened: %lu", session_);
    return ESP_OK;
}

void Pkcs11Provider::closeSession()
{
    if (session_ == CK_INVALID_HANDLE)
    {
        return; // Already closed
    }

    if (functionList_ != nullptr && functionList_->C_CloseSession != nullptr)
    {
        CK_RV rv = functionList_->C_CloseSession(session_);
        if (rv != CKR_OK)
        {
            ESP_LOGW(TAG, "C_CloseSession failed: 0x%lx", rv);
        }
        else
        {
            ESP_LOGI(TAG, "PKCS#11 session closed");
        }
    }

    session_ = CK_INVALID_HANDLE;
}

esp_err_t Pkcs11Provider::convertPkcs11Error(CK_RV rv)
{
    switch (rv)
    {
        case CKR_OK:
            return ESP_OK;
        case CKR_ARGUMENTS_BAD:
        case CKR_ATTRIBUTE_TYPE_INVALID:
        case CKR_ATTRIBUTE_VALUE_INVALID:
            return ESP_ERR_INVALID_ARG;
        case CKR_DEVICE_MEMORY:
        case CKR_HOST_MEMORY:
            return ESP_ERR_NO_MEM;
        case CKR_CRYPTOKI_NOT_INITIALIZED:
        case CKR_SESSION_CLOSED:
        case CKR_SESSION_HANDLE_INVALID:
            return ESP_ERR_INVALID_STATE;
        case CKR_FUNCTION_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        default:
            return ESP_FAIL;
    }
}

} // namespace tls
} // namespace lopcore
