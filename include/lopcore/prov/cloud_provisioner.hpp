#pragma once

#include <string>

namespace lopcore {
namespace prov {

/**
 * Provisioning status enumeration
 */
enum class ProvisioningStatus {
    NOT_PROVISIONED,
    PROVISIONED,
    PROVISIONING_IN_PROGRESS,
    PROVISIONING_FAILED
};

/**
 * Cloud Provisioner Interface
 * 
 * Abstract interface for cloud-specific provisioning implementations.
 * Each cloud provider (AWS, GCP, Azure) implements this interface
 * with their specific provisioning flow.
 * 
 * Example implementations:
 * - AwsFleetProvisioner: AWS IoT Fleet Provisioning with claim certificates
 * - GcpDeviceProvisioner: GCP IoT Core device provisioning
 * - AzureDeviceProvisioner: Azure IoT Hub DPS provisioning
 */
class ICloudProvisioner {
public:
    virtual ~ICloudProvisioner() = default;

    /**
     * Check if device is already provisioned
     * 
     * Checks if final device credentials exist in storage.
     * 
     * @return true if device has valid credentials
     */
    virtual bool isProvisioned() const = 0;

    /**
     * Start provisioning process
     * 
     * This is a blocking call that performs the complete provisioning flow:
     * 1. Load claim/temporary credentials
     * 2. Generate CSR (if needed)
     * 3. Connect to cloud service
     * 4. Request device certificate
     * 5. Register device/thing
     * 6. Store final credentials
     * 
     * @return true if provisioning succeeded
     */
    virtual bool provision() = 0;

    /**
     * Get current provisioning status
     * 
     * @return Current status
     */
    virtual ProvisioningStatus getStatus() const = 0;

    /**
     * Get device ID (thing name, device ID, etc.)
     * 
     * Returns the cloud-assigned device identifier after successful provisioning.
     * 
     * @return Device ID, or empty string if not provisioned
     */
    virtual const char* getDeviceId() const = 0;

    /**
     * Reset provisioning state
     * 
     * Clears stored credentials and resets to NOT_PROVISIONED state.
     * Use this to re-provision a device.
     * 
     * @return true if reset succeeded
     */
    virtual bool resetProvisioning() = 0;
};

} // namespace prov
} // namespace lopcore
