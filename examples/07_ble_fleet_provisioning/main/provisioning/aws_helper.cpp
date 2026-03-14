/**
 * @file aws_helper.cpp
 */
#include "provisioning/aws_helper.hpp"

#include "lopcore/logging/logger.hpp"
#include "lopcore/prov/provisioning_config.hpp"
#include "lopcore/prov/storage_helpers.hpp"

using lopcore::prov::AwsProvisioningConfig;
using lopcore::prov::AwsFleetProvisioner;
using lopcore::prov::CertificateManager;
using lopcore::prov::nvsStorage;

static const char *TAG = "aws_helper";

AwsProvisioningHelper::AwsProvisioningHelper(ProvisioningContext &ctx) : ctx_(ctx) {}

bool AwsProvisioningHelper::provision()
{
    LOPCORE_LOGI(TAG, "  Starting AWS Fleet Provisioning...");

    // Read claim credentials from NVS (written by BleProvisioningHelper)
    ctx_.prov_data.claim_cert           = ctx_.awsNvs->read("claim_cert");
    ctx_.prov_data.claim_key            = ctx_.awsNvs->read("claim_key");
    ctx_.prov_data.aws_endpoint         = ctx_.awsNvs->read("aws_endpoint");
    ctx_.prov_data.root_ca              = ctx_.awsNvs->read("root_ca");
    ctx_.prov_data.provisioning_template = ctx_.awsNvs->read("prov_template");

    // Fallback to compile-time endpoint if BLE did not provide one
    if (!ctx_.prov_data.aws_endpoint.has_value() || ctx_.prov_data.aws_endpoint->empty())
    {
        if (!ctx_.default_aws_endpoint.empty())
            ctx_.prov_data.aws_endpoint = ctx_.default_aws_endpoint;
        else
        {
            lastError_ = "AWS endpoint not set (set DEFAULT_AWS_ENDPOINT in main.cpp or send via BLE)";
            LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
            return false;
        }
    }

    // Validate required credentials
    auto require = [&](const std::optional<std::string> &val, const char *name) -> bool {
        if (!val.has_value() || val->empty())
        {
            lastError_ = std::string(name) + " missing — was BLE provisioning completed?";
            LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
            return false;
        }
        return true;
    };

    if (!require(ctx_.prov_data.claim_cert,           "Claim certificate") ||
        !require(ctx_.prov_data.claim_key,            "Claim private key") ||
        !require(ctx_.prov_data.root_ca,              "Root CA") ||
        !require(ctx_.prov_data.provisioning_template, "Provisioning template"))
        return false;

    LOPCORE_LOGI(TAG, "    Endpoint:  %s", ctx_.prov_data.aws_endpoint->c_str());
    LOPCORE_LOGI(TAG, "    Template:  %s", ctx_.prov_data.provisioning_template->c_str());

    // Import claim certificate into CertificateManager (software keys for this example)
    CertificateManager::Config cmConfig;
    cmConfig.usePkcs11      = false;
    cmConfig.pkcs11Backend  = CertificateManager::Pkcs11Backend::AWS_CORE_PKCS11;
    certManager_ = std::make_shared<CertificateManager>(cmConfig);

    LOPCORE_LOGI(TAG, "    Importing claim certificate...");
    if (!certManager_->importClaimCertificate(ctx_.prov_data.claim_cert.value(),
                                              ctx_.prov_data.claim_key.value()))
    {
        lastError_ = "Failed to import claim certificate";
        LOPCORE_LOGE(TAG, "    %s", lastError_.c_str());
        return false;
    }
    LOPCORE_LOGI(TAG, "    Claim certificate imported OK");

    auto awsStorage = nvsStorage(ctx_.awsNvs);

    // If endpoint came from default fallback (not NVS), persist it so provisioner can read it
    if (ctx_.prov_data.aws_endpoint.has_value())
        ctx_.awsNvs->write("aws_endpoint", ctx_.prov_data.aws_endpoint.value());

    AwsProvisioningConfig awsCfg;
    // AWS-specific setters first (return AwsProvisioningConfig&)
    awsCfg.setCsrSubjectName(ctx_.csr_subject_name.c_str())
        .setDeviceIdProvider([&ctx = ctx_]() { return ctx.deviceId; });
    // Base class setters (return CloudProvisioningConfig&)
    awsCfg.setCertificateManager(certManager_)
        .setRetries(3, 5)
        .addConfigStorage("thing_name",             awsStorage)
        .addConfigStorage("claim_cert",             awsStorage)
        .addConfigStorage("claim_key",              awsStorage)
        .addConfigStorage("aws_endpoint",           awsStorage)
        .addConfigStorage("root_ca",                awsStorage)
        .addConfigStorage("prov_template",           awsStorage);

    mqttAdapter_ = std::make_shared<ProvisioningMqttAdapter>();
    AwsFleetProvisioner<ProvisioningMqttAdapter> provisioner(awsCfg, mqttAdapter_);

    LOPCORE_LOGI(TAG, "    Running Fleet Provisioning workflow...");
    LOPCORE_LOGI(TAG, "    (This may take up to 60 seconds)");

    succeeded_ = provisioner.provision();
    const auto &result = provisioner.getLastResult();

    if (succeeded_)
    {
        thingName_ = result.deviceId;
        LOPCORE_LOGI(TAG, "    Succeeded! Thing Name: %s", thingName_.c_str());
        return true;
    }

    lastError_ = "Fleet Provisioning failed at step " +
                 std::to_string(static_cast<int>(result.lastStep)) + ": " + result.errorMessage;
    LOPCORE_LOGE(TAG, "    Failed: %s", lastError_.c_str());
    return false;
}

void AwsProvisioningHelper::deleteClaimCredentials()
{
    LOPCORE_LOGI(TAG, "    Deleting claim credentials (security hygiene)...");
    if (certManager_)
        certManager_->deleteClaimCredentials();
    ctx_.awsNvs->remove("claim_cert");
    ctx_.awsNvs->remove("claim_key");
    LOPCORE_LOGI(TAG, "    Claim credentials deleted");
}
