/**
 * @file ble_helper.cpp
 */
#include "provisioning/ble_helper.hpp"

#include "lopcore/logging/logger.hpp"
#include "lopcore/prov/storage_helpers.hpp"
#include "wifi_provisioning/manager.h"

using lopcore::prov::nvsStorage;
using lopcore::prov::AwsDataEndpointConfig;
using lopcore::prov::AwsDataEndpointHandler;
using lopcore::prov::WiFiProvisioningConfig;
using lopcore::prov::WiFiProvisioning;
using lopcore::prov::ProvisioningTransport;
using lopcore::prov::ProvisioningSecurity;
using lopcore::prov::CustomEndpointConfig;

static const char *TAG = "ble_helper";

BleProvisioningHelper::BleProvisioningHelper(ProvisioningContext &ctx) : ctx_(ctx) {}

BleProvisioningHelper::~BleProvisioningHelper()
{
    stop();
}

bool BleProvisioningHelper::start()
{
    LOPCORE_LOGI(TAG, "  Starting BLE provisioning...");

    auto awsStorage = nvsStorage(ctx_.awsNvs);

    AwsDataEndpointConfig awsEndpointCfg;
    awsEndpointCfg.endpointName = "aws-data";
    // storageMap keys ARE the NVS keys — AwsDataEndpointHandler uses them directly as the
    // key argument to StorageCallbacks.write(). The handler maps JSON field names to storageMap
    // keys internally before calling write(). Use the NVS key names here.
    awsEndpointCfg.storageMap = {
        {"claim_cert",             awsStorage},
        {"claim_key",              awsStorage},
        {"aws_endpoint",           awsStorage},
        {"root_ca",                awsStorage},
        {"prov_template",          awsStorage},
    };

    awsHandler_ = std::make_shared<AwsDataEndpointHandler>(awsEndpointCfg);

    WiFiProvisioningConfig wifiProvConfig;
    wifiProvConfig.setTransport(ProvisioningTransport::BLE)
        .setSecurity(ProvisioningSecurity::SECURITY_0)
        .setServiceName(ctx_.ble_service_name.c_str())
        .addCustomEndpoint(CustomEndpointConfig("aws-data", awsHandler_));

    wifiProv_ = std::make_shared<WiFiProvisioning>();

    if (!wifiProv_->init(wifiProvConfig))
    {
        lastError_ = "WiFiProvisioning::init() failed";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    // If AWS credentials are missing from NVS, force a fresh BLE session by clearing
    // the WiFi provisioned flag. Without this, wifi_prov_mgr sees "already provisioned"
    // (from stale WiFi NVS) and skips BLE advertising even though AWS creds are absent.
    auto claimCert = ctx_.awsNvs->read("claim_cert");
    if (!claimCert.has_value() || claimCert->empty())
    {
        LOPCORE_LOGI(TAG, "  AWS credentials absent — resetting WiFi provisioning state");
        wifi_prov_mgr_reset_provisioning();
    }

    if (!wifiProv_->start())
    {
        lastError_ = "WiFiProvisioning::start() failed";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    LOPCORE_LOGI(TAG, "  BLE advertising as: %s", ctx_.ble_service_name.c_str());
    LOPCORE_LOGI(TAG, "  Use esp_prov.py or a mobile app to provision.");
    return true;
}

void BleProvisioningHelper::stop()
{
    if (wifiProv_)
    {
        wifiProv_->stop();
        wifiProv_.reset();
    }
}

bool BleProvisioningHelper::isComplete() const
{
    if (!wifiProv_)
        return false;
    return wifiProv_->isProvisioned();
}
