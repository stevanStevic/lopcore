#include "lopcore/tls/mbedtls_transport.hpp"

#include <string.h>
#include <time.h>

#include "lopcore/tls/pkcs11_provider.hpp"

// Backoff algorithm for retries
#include "backoff_algorithm.h"

// Clock for sleep
#include <clock.h>

// Logging
#include "esp_log.h"

static const char *TAG = "MbedtlsTransport";

namespace lopcore
{
namespace tls
{

// Constants from original C implementation
static constexpr const char *ALPN_PROTOCOL_NAME = "x-amzn-mqtt-ca";
static constexpr uint32_t DEFAULT_SEND_RECV_TIMEOUT_MS = 3000;
static constexpr uint32_t DEFAULT_RETRY_BASE_MS = 500;
static constexpr uint32_t DEFAULT_RETRY_MAX_MS = 5000;
static constexpr uint32_t DEFAULT_RETRY_MAX_ATTEMPTS = 5;

MbedtlsTransport::MbedtlsTransport()
    : connected_(false), tlsContext_(nullptr), networkContext_(nullptr), pkcs11Session_(),
      alpnProtos_{nullptr, nullptr}, mutex_(nullptr)
{
    // Create mutex for thread safety
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
}

MbedtlsTransport::~MbedtlsTransport()
{
    disconnect();

    if (mutex_ != nullptr)
    {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

MbedtlsTransport::MbedtlsTransport(MbedtlsTransport &&other) noexcept
    : connected_(other.connected_), tlsContext_(std::move(other.tlsContext_)),
      networkContext_(std::move(other.networkContext_)), pkcs11Session_(std::move(other.pkcs11Session_)),
      alpnProtos_{other.alpnProtos_[0], other.alpnProtos_[1]}, mutex_(other.mutex_)
{
    other.connected_ = false;
    other.alpnProtos_[0] = nullptr;
    other.alpnProtos_[1] = nullptr;
    other.mutex_ = nullptr;
}

MbedtlsTransport &MbedtlsTransport::operator=(MbedtlsTransport &&other) noexcept
{
    if (this != &other)
    {
        // Disconnect and clean up current state
        disconnect();
        if (mutex_ != nullptr)
        {
            vSemaphoreDelete(mutex_);
        }

        // Move from other
        connected_ = other.connected_;
        tlsContext_ = std::move(other.tlsContext_);
        networkContext_ = std::move(other.networkContext_);
        pkcs11Session_ = std::move(other.pkcs11Session_);
        alpnProtos_[0] = other.alpnProtos_[0];
        alpnProtos_[1] = other.alpnProtos_[1];
        mutex_ = other.mutex_;

        // Reset other
        other.connected_ = false;
        other.alpnProtos_[0] = nullptr;
        other.alpnProtos_[1] = nullptr;
        other.mutex_ = nullptr;
    }
    return *this;
}

esp_err_t MbedtlsTransport::connect(const TlsConfig &config)
{
    // Validate configuration
    esp_err_t err = config.validate();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Invalid TLS configuration: %d", err);
        return err;
    }

    // Take mutex
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    // Check if already connected
    if (connected_)
    {
        xSemaphoreGive(mutex_);
        ESP_LOGW(TAG, "Already connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Get PKCS#11 session
    CK_SESSION_HANDLE sessionHandle = 0;
    auto &provider = Pkcs11Provider::instance();
    err = provider.getSession(&sessionHandle);
    if (err != ESP_OK)
    {
        xSemaphoreGive(mutex_);
        ESP_LOGE(TAG, "Failed to get PKCS#11 session: %d", err);
        return err;
    }

    // Store session in RAII wrapper
    pkcs11Session_ = Pkcs11Session();
    if (!pkcs11Session_.isValid())
    {
        xSemaphoreGive(mutex_);
        ESP_LOGE(TAG, "Failed to create PKCS#11 session wrapper");
        return pkcs11Session_.error();
    }

    // Allocate contexts
    tlsContext_ = std::make_unique<MbedtlsPkcs11Context_t>();
    networkContext_ = std::make_unique<NetworkContext_t>();

    if (!tlsContext_ || !networkContext_)
    {
        xSemaphoreGive(mutex_);
        ESP_LOGE(TAG, "Failed to allocate memory for contexts");
        return ESP_ERR_NO_MEM;
    }

    // Zero-initialize contexts
    memset(tlsContext_.get(), 0, sizeof(MbedtlsPkcs11Context_t));
    memset(networkContext_.get(), 0, sizeof(NetworkContext_t));

    // Set network context to point to TLS context
    networkContext_->pParams = tlsContext_.get();

    // Attempt connection with retry logic
    err = connectWithRetries(config);

    if (err == ESP_OK)
    {
        // Connection successful
        connected_ = true;
        ESP_LOGI(TAG, "Successfully connected to %s:%u", config.hostname.c_str(), config.port);
    }
    else
    {
        // Connection failed - clean up
        tlsContext_.reset();
        networkContext_.reset();
        ESP_LOGE(TAG, "Failed to connect to %s:%u after retries", config.hostname.c_str(), config.port);
    }

    xSemaphoreGive(mutex_);
    return err;
}

void MbedtlsTransport::disconnect() noexcept
{
    if (mutex_ != nullptr)
    {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    if (connected_ && networkContext_)
    {
        ESP_LOGI(TAG, "Disconnecting TLS connection");
        Mbedtls_Pkcs11_Disconnect(networkContext_.get());
        connected_ = false;
    }

    // Clean up contexts
    tlsContext_.reset();
    networkContext_.reset();

    // Reset ALPN
    alpnProtos_[0] = nullptr;
    alpnProtos_[1] = nullptr;

    if (mutex_ != nullptr)
    {
        xSemaphoreGive(mutex_);
    }
}

esp_err_t MbedtlsTransport::send(const void *data, size_t size, size_t *bytesSent)
{
    if (data == nullptr || size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    if (!connected_ || !networkContext_)
    {
        xSemaphoreGive(mutex_);
        return ESP_ERR_INVALID_STATE;
    }

    // Call C function to send data
    int32_t result = Mbedtls_Pkcs11_Send(networkContext_.get(), data, size);

    xSemaphoreGive(mutex_);

    if (result < 0)
    {
        ESP_LOGE(TAG, "Send failed: %ld", result);
        return ESP_FAIL;
    }

    if (bytesSent != nullptr)
    {
        *bytesSent = static_cast<size_t>(result);
    }

    return ESP_OK;
}

esp_err_t MbedtlsTransport::recv(void *buffer, size_t size, size_t *bytesReceived)
{
    if (buffer == nullptr || size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    if (!connected_ || !networkContext_)
    {
        xSemaphoreGive(mutex_);
        return ESP_ERR_INVALID_STATE;
    }

    // Call C function to receive data
    int32_t result = Mbedtls_Pkcs11_Recv(networkContext_.get(), buffer, size);

    xSemaphoreGive(mutex_);

    if (result < 0)
    {
        ESP_LOGE(TAG, "Receive failed: %ld", result);
        return ESP_FAIL;
    }

    if (result == 0)
    {
        // Zero means timeout/retry - this is not an error
        return ESP_ERR_TIMEOUT;
    }

    if (bytesReceived != nullptr)
    {
        *bytesReceived = static_cast<size_t>(result);
    }

    return ESP_OK;
}

bool MbedtlsTransport::isConnected() const noexcept
{
    return connected_;
}

void *MbedtlsTransport::getNetworkContext() noexcept
{
    if (mutex_ != nullptr)
    {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    void *context = connected_ ? networkContext_.get() : nullptr;

    if (mutex_ != nullptr)
    {
        xSemaphoreGive(mutex_);
    }

    return context;
}

esp_err_t MbedtlsTransport::convertMbedtlsError(MbedtlsPkcs11Status_t status)
{
    switch (status)
    {
        case MBEDTLS_PKCS11_SUCCESS:
            return ESP_OK;
        case MBEDTLS_PKCS11_INVALID_PARAMETER:
            return ESP_ERR_INVALID_ARG;
        case MBEDTLS_PKCS11_INSUFFICIENT_MEMORY:
            return ESP_ERR_NO_MEM;
        case MBEDTLS_PKCS11_INVALID_CREDENTIALS:
            return ESP_ERR_INVALID_ARG;
        case MBEDTLS_PKCS11_HANDSHAKE_FAILED:
        case MBEDTLS_PKCS11_INTERNAL_ERROR:
        case MBEDTLS_PKCS11_CONNECT_FAILURE:
        default:
            return ESP_FAIL;
    }
}

esp_err_t MbedtlsTransport::connectWithRetries(const TlsConfig &config)
{
    // Setup credentials
    MbedtlsPkcs11Credentials_t credentials = {0};
    credentials.pRootCaPath = config.caCertPath.empty() ? nullptr : config.caCertPath.c_str();
    credentials.pClientCertLabel = const_cast<char *>(config.clientCertLabel.c_str());
    credentials.pPrivateKeyLabel = const_cast<char *>(config.clientKeyLabel.c_str());
    credentials.p11Session = pkcs11Session_.get();
    credentials.disableSni = !config.enableSni;

    // Setup ALPN if needed (port 443)
    const char **alpnProtos = setupAlpnProtocols(config.port);
    credentials.pAlpnProtos = alpnProtos;

    // Get timeout value (convert chrono to milliseconds)
    uint32_t timeoutMs = static_cast<uint32_t>(config.recvTimeout.count());
    if (timeoutMs == 0)
    {
        timeoutMs = DEFAULT_SEND_RECV_TIMEOUT_MS;
    }

    // Initialize backoff algorithm for retries
    BackoffAlgorithmContext_t backoffContext;
    uint32_t baseMs = static_cast<uint32_t>(config.retryBaseDelay.count());
    uint32_t maxMs = static_cast<uint32_t>(config.retryMaxDelay.count());
    uint32_t maxAttempts = config.maxRetries;

    BackoffAlgorithm_InitializeParams(&backoffContext, baseMs, maxMs, maxAttempts);

    // Seed random number generator for backoff jitter
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec);

    // Retry loop
    bool success = false;
    BackoffAlgorithmStatus_t backoffStatus = BackoffAlgorithmSuccess;
    uint16_t nextBackoffMs = 0;

    do
    {
        ESP_LOGI(TAG, "Attempting TLS connection to %s:%u", config.hostname.c_str(), config.port);

        // Attempt connection
        MbedtlsPkcs11Status_t tlsStatus = Mbedtls_Pkcs11_Connect(networkContext_.get(),
                                                                 config.hostname.c_str(), config.port,
                                                                 &credentials, timeoutMs);

        success = (tlsStatus == MBEDTLS_PKCS11_SUCCESS);

        if (!success)
        {
            // Get next backoff delay
            backoffStatus = BackoffAlgorithm_GetNextBackoff(&backoffContext, rand(), &nextBackoffMs);

            if (backoffStatus == BackoffAlgorithmSuccess)
            {
                ESP_LOGW(TAG, "Connection failed, retrying after %u ms backoff", nextBackoffMs);
                Clock_SleepMs(nextBackoffMs);
            }
            else
            {
                ESP_LOGE(TAG, "Connection failed, all retry attempts exhausted");
            }
        }

    } while (!success && backoffStatus == BackoffAlgorithmSuccess);

    if (!success)
    {
        ESP_LOGE(TAG, "Failed to establish TLS connection after all retries");
        return ESP_FAIL;
    }

    return ESP_OK;
}

const char **MbedtlsTransport::setupAlpnProtocols(uint16_t port)
{
    if (port == 443)
    {
        // AWS IoT MQTT over port 443 requires ALPN
        alpnProtos_[0] = ALPN_PROTOCOL_NAME;
        alpnProtos_[1] = nullptr;
        return alpnProtos_;
    }

    return nullptr;
}

} // namespace tls
} // namespace lopcore
