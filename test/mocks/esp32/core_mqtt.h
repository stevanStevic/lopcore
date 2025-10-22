/**
 * @file core_mqtt.h
 * @brief Mock coreMQTT API for host testing
 *
 * This provides minimal mocks of the AWS IoT coreMQTT library for testing.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "transport_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// MQTT return codes
typedef enum MQTTStatus
{
    MQTTSuccess = 0,
    MQTTBadParameter,
    MQTTNoMemory,
    MQTTSendFailed,
    MQTTRecvFailed,
    MQTTBadResponse,
    MQTTServerRefused,
    MQTTNoDataAvailable,
    MQTTIllegalState,
    MQTTStateCollision,
    MQTTKeepAliveTimeout,
    MQTTNeedMoreBytes
} MQTTStatus_t;

// MQTT QoS levels
typedef enum MQTTQoS
{
    MQTTQoS0 = 0,
    MQTTQoS1 = 1,
    MQTTQoS2 = 2
} MQTTQoS_t;

// MQTT packet types
typedef enum MQTTPacketType
{
    MQTT_PACKET_TYPE_CONNECT = 0x10,
    MQTT_PACKET_TYPE_CONNACK = 0x20,
    MQTT_PACKET_TYPE_PUBLISH = 0x30,
    MQTT_PACKET_TYPE_PUBACK = 0x40,
    MQTT_PACKET_TYPE_PUBREC = 0x50,
    MQTT_PACKET_TYPE_PUBREL = 0x60,
    MQTT_PACKET_TYPE_PUBCOMP = 0x70,
    MQTT_PACKET_TYPE_SUBSCRIBE = 0x82,
    MQTT_PACKET_TYPE_SUBACK = 0x90,
    MQTT_PACKET_TYPE_UNSUBSCRIBE = 0xA2,
    MQTT_PACKET_TYPE_UNSUBACK = 0xB0,
    MQTT_PACKET_TYPE_PINGREQ = 0xC0,
    MQTT_PACKET_TYPE_PINGRESP = 0xD0,
    MQTT_PACKET_TYPE_DISCONNECT = 0xE0
} MQTTPacketType_t;

// MQTT packet ID constants
#define MQTT_PACKET_ID_INVALID 0

// Time function type
typedef uint32_t (*MQTTGetCurrentTimeFunc_t)(void);

// Event callback type
typedef void (*MQTTEventCallback_t)(struct MQTTContext *pContext,
                                    struct MQTTPacketInfo *pPacketInfo,
                                    struct MQTTDeserializedInfo *pDeserializedInfo);

// MQTT publish info
typedef struct MQTTPublishInfo
{
    MQTTQoS_t qos;
    bool retain;
    bool dup;
    const char *pTopicName;
    uint16_t topicNameLength;
    const void *pPayload;
    size_t payloadLength;
} MQTTPublishInfo_t;

// MQTT subscription info
typedef struct MQTTSubscribeInfo
{
    MQTTQoS_t qos; // Note: Should be qosLevel in real coreMQTT, but using qos for compatibility
    const char *pTopicFilter;
    uint16_t topicFilterLength;
} MQTTSubscribeInfo_t;

// MQTT packet info
typedef struct MQTTPacketInfo
{
    MQTTPacketType_t type;
    const uint8_t *pRemainingData;
    size_t remainingLength;
} MQTTPacketInfo_t;

// MQTT connection info
typedef struct MQTTConnectInfo
{
    bool cleanSession;
    uint16_t keepAliveSeconds;
    const char *pClientIdentifier;
    uint16_t clientIdentifierLength;
    const char *pUserName;
    uint16_t userNameLength;
    const char *pPassword;
    uint16_t passwordLength;
} MQTTConnectInfo_t;

// MQTT publish state (for QoS tracking)
typedef enum MQTTPublishState
{
    MQTTStateNull = 0,
    MQTTPublishSend,
    MQTTPubAckSend,
    MQTTPubAckPending,
    MQTTPubRecSend,
    MQTTPubRecPending,
    MQTTPubRelSend,
    MQTTPubRelPending,
    MQTTPubCompSend,
    MQTTPubCompPending,
    MQTTPublishDone
} MQTTPublishState_t;

// MQTT PubAck info
typedef struct MQTTPubAckInfo
{
    uint16_t packetId;
    MQTTPublishState_t publishState;
} MQTTPubAckInfo_t;

// MQTT deserialized info
typedef struct MQTTDeserializedInfo
{
    MQTTPublishInfo_t *pPublishInfo;
    MQTTPacketInfo_t packetInfo;
    uint16_t packetIdentifier;
    uint8_t deserializationResult;
} MQTTDeserializedInfo_t;

// MQTT connection status
typedef enum MQTTConnectionStatus
{
    MQTTNotConnected,
    MQTTConnected,
    MQTTDisconnectPending
} MQTTConnectionStatus_t;

// MQTT fixed buffer
typedef struct MQTTFixedBuffer
{
    uint8_t *pBuffer;
    size_t size;
} MQTTFixedBuffer_t;

// Forward declaration
struct MQTTContext;

// MQTT context
typedef struct MQTTContext
{
    const TransportInterface_t *pNetworkInterface;
    MQTTFixedBuffer_t networkBuffer;
    MQTTGetCurrentTimeFunc_t getTime;
    MQTTEventCallback_t appCallback;
    uint16_t nextPacketId;
    MQTTConnectionStatus_t connectStatus;
} MQTTContext_t;

// Mock functions

/**
 * @brief Initialize MQTT context (mock)
 */
inline MQTTStatus_t MQTT_Init(MQTTContext_t *pContext,
                              const TransportInterface_t *pTransportInterface,
                              MQTTGetCurrentTimeFunc_t getTimeFunction,
                              MQTTEventCallback_t userCallback,
                              const MQTTFixedBuffer_t *pNetworkBuffer)
{
    if (pContext != nullptr && pTransportInterface != nullptr && pNetworkBuffer != nullptr)
    {
        pContext->pNetworkInterface = pTransportInterface;
        pContext->getTime = getTimeFunction;
        pContext->connectStatus = MQTTNotConnected;
        return MQTTSuccess;
    }
    return MQTTBadParameter;
}

inline MQTTStatus_t MQTT_InitStatefulQoS(MQTTContext_t *pContext,
                                         MQTTPubAckInfo_t *pOutgoingPublishRecords,
                                         size_t outgoingPublishCount,
                                         MQTTPubAckInfo_t *pIncomingPublishRecords,
                                         size_t incomingPublishCount)
{
    // Mock implementation - just return success
    (void) pContext;
    (void) pOutgoingPublishRecords;
    (void) outgoingPublishCount;
    (void) pIncomingPublishRecords;
    (void) incomingPublishCount;
    return MQTTSuccess;
}

/**
 * @brief Connect to MQTT broker (mock - always succeeds)
 */
inline MQTTStatus_t MQTT_Connect(MQTTContext_t *pContext,
                                 const MQTTConnectInfo_t *pConnectInfo,
                                 const MQTTPublishInfo_t *pWillInfo,
                                 uint32_t timeoutMs,
                                 bool *pSessionPresent)
{
    (void) pConnectInfo;
    (void) pWillInfo;
    (void) timeoutMs;

    if (!pContext)
    {
        return MQTTBadParameter;
    }

    pContext->connectStatus = MQTTConnected;

    if (pSessionPresent)
    {
        *pSessionPresent = false;
    }

    return MQTTSuccess;
}

/**
 * @brief Subscribe to topics (mock - always succeeds)
 */
inline MQTTStatus_t MQTT_Subscribe(MQTTContext_t *pContext,
                                   const MQTTSubscribeInfo_t *pSubscriptionList,
                                   size_t subscriptionCount,
                                   uint16_t packetId)
{
    // Mock implementation - just return success
    (void) pContext;
    (void) pSubscriptionList;
    (void) subscriptionCount;
    (void) packetId;
    return MQTTSuccess;
}

inline MQTTStatus_t MQTT_Unsubscribe(MQTTContext_t *pContext,
                                     const MQTTSubscribeInfo_t *pSubscriptionList,
                                     size_t subscriptionCount,
                                     uint16_t packetId)
{
    // Mock implementation - just return success
    (void) pContext;
    (void) pSubscriptionList;
    (void) subscriptionCount;
    (void) packetId;
    return MQTTSuccess;
}

/**
 * @brief Publish message (mock - always succeeds)
 */
inline MQTTStatus_t MQTT_Publish(MQTTContext_t *pContext,
                                 const MQTTPublishInfo_t *pPublishInfo,
                                 uint16_t packetId)
{
    (void) pPublishInfo;
    (void) packetId;

    if (!pContext)
    {
        return MQTTBadParameter;
    }

    return MQTTSuccess;
}

/**
 * @brief Process loop (mock - no-op)
 */
inline MQTTStatus_t MQTT_ProcessLoop(MQTTContext_t *pContext)
{
    // Mock implementation - just return success
    (void) pContext;
    return MQTTSuccess;
}

/**
 * @brief Disconnect from broker (mock)
 */
inline MQTTStatus_t MQTT_Disconnect(MQTTContext_t *pContext)
{
    if (!pContext)
    {
        return MQTTBadParameter;
    }

    pContext->connectStatus = MQTTNotConnected;
    return MQTTSuccess;
}

/**
 * @brief Get packet ID (mock)
 */
inline uint16_t MQTT_GetPacketId(MQTTContext_t *pContext)
{
    if (!pContext)
    {
        return 0;
    }

    return pContext->nextPacketId++;
}

/**
 * @brief Get status string (mock)
 */
inline const char *MQTT_Status_strerror(MQTTStatus_t status)
{
    static const char *statusStrings[] = {"MQTTSuccess",        "MQTTBadParameter",    "MQTTNoMemory",
                                          "MQTTSendFailed",     "MQTTRecvFailed",      "MQTTBadResponse",
                                          "MQTTServerRefused",  "MQTTNoDataAvailable", "MQTTIllegalState",
                                          "MQTTStateCollision", "MQTTKeepAliveTimeout"};

    if (status <= MQTTKeepAliveTimeout)
    {
        return statusStrings[status];
    }
    return "Unknown";
}

#ifdef __cplusplus
}
#endif
