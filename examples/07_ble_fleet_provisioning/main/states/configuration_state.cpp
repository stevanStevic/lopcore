/**
 * @file configuration_state.cpp
 * @brief CONFIGURATION state implementation.
 */

#include "states/configuration_state.hpp"

#include "esp_timer.h"

static const char *TAG = "app_sm:config";

ConfigurationState::ConfigurationState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

ConfigurationState::~ConfigurationState() = default;

void ConfigurationState::onEnter()
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "CONFIGURATION: BLE + AWS Provisioning");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Starting Phase 1: BLE WiFi Provisioning (attempt %d of %d)",
                 ctx_.config.retryCount + 1, ctx_.config.maxRetries);

    // Create provisioning helpers
    bleHelper_ = std::make_unique<BleProvisioningHelper>(ctx_);
    awsHelper_ = std::make_unique<AwsProvisioningHelper>(ctx_);

    // Start Phase 1
    startPhase(Phase::BLE_PROVISIONING);
}

void ConfigurationState::update()
{
    switch (currentPhase_)
    {
        case Phase::BLE_PROVISIONING: {
            LOPCORE_LOGD(TAG, "  [BLE] Waiting for WiFi provisioning...");

            if (bleHelper_->isComplete())
            {
                LOPCORE_LOGI(TAG, "  [BLE] WiFi + AWS credentials received!");
                logPhaseTransition(Phase::BLE_PROVISIONING, Phase::AWAITING_WIFI);
                startPhase(Phase::AWAITING_WIFI);
            }
            else if (isPhaseTimedOut(ctx_.config.ble_timeout_ms))
            {
                handlePhaseFailure("BLE provisioning timeout");
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }

        case Phase::AWAITING_WIFI: {
            if (hasElapsed(ctx_.config.wifi_stabilization_ms))
            {
                LOPCORE_LOGI(TAG, "  [WiFi] Stabilization complete");
                logPhaseTransition(Phase::AWAITING_WIFI, Phase::AWS_PROVISIONING);
                startPhase(Phase::AWS_PROVISIONING);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }

        case Phase::AWS_PROVISIONING: {
            LOPCORE_LOGD(TAG, "  [AWS] Running Fleet Provisioning workflow...");

            if (awsHelper_->provision())
            {
                LOPCORE_LOGI(TAG, "");
                LOPCORE_LOGI(TAG, "  Fleet provisioning succeeded!");
                LOPCORE_LOGI(TAG, "  Thing Name: %s", awsHelper_->getThingName().c_str());
                logPhaseTransition(Phase::AWS_PROVISIONING, Phase::SUCCESS_CLEANUP);
                startPhase(Phase::SUCCESS_CLEANUP);
            }
            else if (isPhaseTimedOut(ctx_.config.aws_timeout_ms))
            {
                handlePhaseFailure("AWS provisioning timeout");
                return;
            }
            else
            {
                // Provisioner failed but didn't timeout — treat as recoverable
                handlePhaseFailure(awsHelper_->getLastError());
                return;
            }
            break;
        }

        case Phase::SUCCESS_CLEANUP: {
            LOPCORE_LOGI(TAG, "  Cleaning up: deleting claim credentials (security hygiene)...");
            awsHelper_->deleteClaimCredentials();
            LOPCORE_LOGI(TAG, "  Device provisioning complete!");
            LOPCORE_LOGI(TAG, "  Transitioning to NOMINAL state...");
            logPhaseTransition(Phase::SUCCESS_CLEANUP, Phase::SUCCESS_CLEANUP);
            sm_->transition(ApplicationState::NOMINAL);
            break;
        }

        case Phase::FAILED_RETRY_WAIT: {
            // Bug fix: ctx_.config is already ConfigurationMetrics; was erroneously
            // written as ctx_.config.config.ble_timeout_ms in the original flat file.
            LOPCORE_LOGI(TAG, "  Waiting before retry (retries left: %d)...",
                         ctx_.config.maxRetries - ctx_.config.retryCount);

            if (hasElapsed(5000))
            {
                if (ctx_.config.retryCount < ctx_.config.maxRetries)
                {
                    ctx_.config.retryCount++;
                    LOPCORE_LOGI(TAG, "  Retrying provisioning (attempt %d of %d)",
                                 ctx_.config.retryCount + 1, ctx_.config.maxRetries);
                    logPhaseTransition(Phase::FAILED_RETRY_WAIT, Phase::BLE_PROVISIONING);
                    startPhase(Phase::BLE_PROVISIONING);
                }
                else
                {
                    LOPCORE_LOGE(TAG, "  Max retries exceeded! Transitioning to FACTORY_RESET");
                    sm_->transition(ApplicationState::FACTORY_RESET);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }

        case Phase::ABORTED:
        default:
            // Should not reach here; abort immediately
            sm_->transition(ApplicationState::FACTORY_RESET);
            break;
    }
}

void ConfigurationState::onExit()
{
    LOPCORE_LOGI(TAG, "[CONFIGURATION] Exit\n");
    bleHelper_.reset();
    awsHelper_.reset();
}

bool ConfigurationState::hasElapsed(uint32_t ms) const
{
    uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() / 1000) - phaseStartTime_;
    return elapsed >= ms;
}

bool ConfigurationState::isPhaseTimedOut(uint32_t timeoutMs) const
{
    return hasElapsed(timeoutMs);
}

void ConfigurationState::startPhase(Phase phase)
{
    currentPhase_ = phase;
    phaseStartTime_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void ConfigurationState::handlePhaseFailure(const std::string &reason)
{
    LOPCORE_LOGE(TAG, "");
    LOPCORE_LOGE(TAG, "  Provisioning failed at phase: %s",
                 (currentPhase_ == Phase::BLE_PROVISIONING)   ? "BLE_PROVISIONING"
                 : (currentPhase_ == Phase::AWS_PROVISIONING) ? "AWS_PROVISIONING"
                                                              : "UNKNOWN");
    LOPCORE_LOGE(TAG, "  Reason: %s", reason.c_str());

    ctx_.config.lastPhaseError = reason;
    logPhaseTransition(currentPhase_, Phase::FAILED_RETRY_WAIT);
    startPhase(Phase::FAILED_RETRY_WAIT);
}

void ConfigurationState::logPhaseTransition(Phase from, Phase to)
{
    const char *fromStr = (from == Phase::BLE_PROVISIONING)    ? "BLE_PROVISIONING"
                          : (from == Phase::AWAITING_WIFI)     ? "AWAITING_WIFI"
                          : (from == Phase::AWS_PROVISIONING)  ? "AWS_PROVISIONING"
                          : (from == Phase::SUCCESS_CLEANUP)   ? "SUCCESS_CLEANUP"
                          : (from == Phase::FAILED_RETRY_WAIT) ? "FAILED_RETRY_WAIT"
                                                               : "UNKNOWN";

    const char *toStr = (to == Phase::BLE_PROVISIONING)    ? "BLE_PROVISIONING"
                        : (to == Phase::AWAITING_WIFI)     ? "AWAITING_WIFI"
                        : (to == Phase::AWS_PROVISIONING)  ? "AWS_PROVISIONING"
                        : (to == Phase::SUCCESS_CLEANUP)   ? "SUCCESS_CLEANUP"
                        : (to == Phase::FAILED_RETRY_WAIT) ? "FAILED_RETRY_WAIT"
                                                           : "UNKNOWN";

    LOPCORE_LOGI(TAG, "  Phase transition: %s -> %s", fromStr, toStr);
}
