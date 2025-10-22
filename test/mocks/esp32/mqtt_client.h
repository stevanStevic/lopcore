/**
 * @file mqtt_client.h
 * @brief Mock ESP-MQTT client API for host testing
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Mock ESP MQTT types
typedef struct esp_mqtt_client *esp_mqtt_client_handle_t;

typedef enum
{
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_ERROR,
    MQTT_EVENT_BEFORE_CONNECT,
} esp_mqtt_event_id_t;

typedef struct
{
    esp_mqtt_event_id_t event_id;
    esp_mqtt_client_handle_t client;
    void *user_context;
    char *data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    char *topic;
    int topic_len;
    int msg_id;
    int session_present;
    int error_handle;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

typedef esp_err_t (*mqtt_event_callback_t)(esp_mqtt_event_handle_t event);

typedef struct
{
    mqtt_event_callback_t event_handle;
    const char *host;
    const char *uri;
    uint32_t port;
    const char *client_id;
    const char *username;
    const char *password;
    const char *lwt_topic;
    const char *lwt_msg;
    int lwt_qos;
    int lwt_retain;
    int lwt_msg_len;
    int disable_clean_session;
    int keepalive;
    bool disable_auto_reconnect;
    void *user_context;
    int task_prio;
    int task_stack;
    int buffer_size;
    const char *cert_pem;
    size_t cert_len;
    const char *client_cert_pem;
    size_t client_cert_len;
    const char *client_key_pem;
    size_t client_key_len;
    bool use_secure_element;
    void *ds_data;
    int network_timeout_ms;
    bool skip_cert_common_name_check;
    const char **alpn_protos;
    int reconnect_timeout_ms;
} esp_mqtt_client_config_t;

// Mock functions
inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config)
{
    (void) config;
    return reinterpret_cast<esp_mqtt_client_handle_t>(0x3000);
}

inline esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{
    (void) client;
    return ESP_OK;
}

inline esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client)
{
    (void) client;
    return ESP_OK;
}

inline esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client)
{
    (void) client;
    return ESP_OK;
}

inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{
    (void) client;
    (void) topic;
    (void) qos;
    return 1; // Mock message ID
}

inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t client,
                                   const char *topic,
                                   const char *data,
                                   int len,
                                   int qos,
                                   int retain)
{
    (void) client;
    (void) topic;
    (void) data;
    (void) len;
    (void) qos;
    (void) retain;
    return 1; // Mock message ID
}

inline esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                                esp_mqtt_event_id_t event,
                                                mqtt_event_callback_t event_handler,
                                                void *event_handler_arg)
{
    (void) client;
    (void) event;
    (void) event_handler;
    (void) event_handler_arg;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
