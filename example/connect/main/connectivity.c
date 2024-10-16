/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "juice/juice.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif_sntp.h"

#define BUFFER_SIZE 1024

static juice_agent_t *agent1;
static EventGroupHandle_t event_group = NULL;

#define DESC_SIZE 256
#define MAX_CANDS 4

static char s_remote_desc[DESC_SIZE];
static int s_remote_cand = 0;
static char s_remote_cands[MAX_CANDS][DESC_SIZE];

static void on_state_changed1(juice_agent_t *agent, juice_state_t state, void *user_ptr);
static void on_candidate1(juice_agent_t *agent, const char *sdp, void *user_ptr);
static void on_gathering_done1(juice_agent_t *agent, void *user_ptr);
static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);

static const char *TAG = "mqtt_example";

#if defined(CLIENT1)
#define OUR_CLIENT "1"
#define THEIR_CLIENT "2"
#define OUR_PORT 12345
#elif defined(CLIENT2)
#define OUR_CLIENT "2"
#define THEIR_CLIENT "1"
#define OUR_PORT 12346
#endif

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    static int ready_already = 0;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic123789/desc" THEIR_CLIENT, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, "/topic123789/cand" THEIR_CLIENT, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, "/topic123789/done" THEIR_CLIENT, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, "/topic123789/ready" THEIR_CLIENT, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if (memcmp(event->topic, "/topic123789/desc" THEIR_CLIENT, event->topic_len) == 0) {
                memcpy(s_remote_desc, event->data, event->data_len);
                s_remote_desc[event->data_len] = '\0';
//                juice_set_remote_description(agent1, event->data);
            }
            if (memcmp(event->topic, "/topic123789/cand" THEIR_CLIENT, event->topic_len) == 0) {
                memcpy(s_remote_cands[s_remote_cand], event->data, event->data_len);
                s_remote_cands[s_remote_cand][event->data_len] = '\0';
                s_remote_cand++;
//                event->data[event->data_len] = '\0';
                // store it in global vars;
                // juice_add_remote_candidate(agent1, event->data);
            }
            if (memcmp(event->topic, "/topic123789/done" THEIR_CLIENT, event->topic_len) == 0) {
                xEventGroupSetBits(event_group, 2);
            }
            if (memcmp(event->topic, "/topic123789/ready" THEIR_CLIENT, event->topic_len) == 0) {
                ESP_LOGE(TAG, "Their is ready!");
                if (!ready_already) {
                    msg_id = esp_mqtt_client_publish(client, "/topic123789/ready" OUR_CLIENT, "our client is ready", 0, 0, 0);
                    ESP_LOGI(TAG, "published msg_id %d", msg_id);
                    ready_already = 1;
                }
                xEventGroupSetBits(event_group, 1);
            }

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_mqtt_client_handle_t client = NULL;
#include <time.h>

static void setup_sntp(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("time.windows.com");
    esp_netif_sntp_init(&config);
    int retry = 0;
    const int retry_count = 10;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) != ESP_OK && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    if (retry == retry_count) {
        abort();
    }
    time(&now);
    setenv("TZ", "CET-1", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "current date/time is: %s", strftime_buf);


}

int test_connectivity() {
    event_group = xEventGroupCreate();

    esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
            .task.stack_size = 16384,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    setup_sntp();

    ESP_LOGW(TAG, "Waiting for ready signal...");
//     while (!xEventGroupWaitBits(event_group, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000))) {
//         int msg_id = esp_mqtt_client_publish(client, "/topic123789/ready" OUR_CLIENT, "our client is ready", 0, 0, 0);
//         ESP_LOGI(TAG, "published msg_id %d", msg_id);
//     }

    struct tm timeinfo = {};
    int last_sec = 0;
    do {
         time_t now;
         time(&now);
         localtime_r(&now, &timeinfo);
         if (last_sec != timeinfo.tm_sec) {
             ESP_LOGE(TAG, "seconds %d", timeinfo.tm_sec);
             last_sec = timeinfo.tm_sec;
         }
        vTaskDelay(pdMS_TO_TICKS(20));
     } while (timeinfo.tm_hour != 15 || timeinfo.tm_min != 40 || timeinfo.tm_sec != 3);
    ESP_LOGW(TAG, "Their client is ready, start");

    juice_set_log_level(JUICE_LOG_LEVEL_VERBOSE);

    // Agent 1: Create agent
    juice_config_t config1;
    memset(&config1, 0, sizeof(config1));

    // STUN server example
    config1.stun_server_host = "stun.l.google.com";
    config1.stun_server_port = 19302;

    config1.cb_state_changed = on_state_changed1;
    config1.cb_candidate = on_candidate1;
    config1.cb_gathering_done = on_gathering_done1;
    config1.cb_recv = on_recv1;
    config1.user_ptr = NULL;

    vTaskDelay(pdMS_TO_TICKS(1000));
    agent1 = juice_create(&config1);

    // Agent 1: Generate local description
    static char sdp1[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(agent1, sdp1, JUICE_MAX_SDP_STRING_LEN);
    printf("Local description 1:\n%s\n", sdp1);
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/desc" OUR_CLIENT, sdp1, 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Agent 1: Gather candidates (and send them to agent 2)
    juice_gather_candidates(agent1);
    xEventGroupWaitBits(event_group, 2, pdTRUE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(5000));

    // wait for sync:
//    add_remote_cands(..);

    // Check states
    juice_state_t state1 = juice_get_state(agent1);
    printf("STATE: %d\n", state1);
    bool success = (state1 == JUICE_STATE_COMPLETED);

    // Retrieve candidates
    char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    if (success &=
                (juice_get_selected_candidates(agent1, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote,
                                               JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0)) {
        printf("Local candidate  1: %s\n", local);
        printf("Remote candidate 1: %s\n", remote);
        if ((!strstr(local, "typ host") && !strstr(local, "typ prflx")) ||
            (!strstr(remote, "typ host") && !strstr(remote, "typ prflx")))
            success = false; // local connection should be possible
    }

    // Retrieve addresses
    char localAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    char remoteAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    if (success &= (juice_get_selected_addresses(agent1, localAddr, JUICE_MAX_ADDRESS_STRING_LEN,
                                                 remoteAddr, JUICE_MAX_ADDRESS_STRING_LEN) == 0)) {
        printf("Local address  1: %s\n", localAddr);
        printf("Remote address 1: %s\n", remoteAddr);
    }

    // Agent 1: destroy
    juice_destroy(agent1);

    if (success) {
        printf("Success\n");
        return 0;
    } else {
        printf("Failure\n");
        return -1;
    }
    while (1)
{
    juice_send(agent1, "message",6);
    vTaskDelay(pdMS_TO_TICKS(1000));


}


 /*

0) init sntp
1) get description   -- cat to string
2) gather candidates -- cat to string
3) wait for the gathering done callback
4) send the string (desc + candidates) over mqtt
5) --- wait for sync --
6)

 */


}

// Agent 1: on state changed
static void on_state_changed1(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    printf("State 1: %s\n", juice_state_to_string(state));

    if (state == JUICE_STATE_CONNECTED) {
        // Agent 1: on connected, send a message
        const char *message = "Hello from 1" OUR_CLIENT;
        juice_send(agent, message, strlen(message));
    }
}

static void on_candidate1(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ESP_LOGI("[1]", "Candidate 1: >>%s<<\n", sdp);
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/cand" OUR_CLIENT, sdp, 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);
}

static void on_gathering_done1(juice_agent_t *agent, void *user_ptr) {
    ESP_LOGI("[1]", "Gathering done 1\n");
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/done" OUR_CLIENT, "gathering done", 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);
}

static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    char buffer[BUFFER_SIZE];
    if (size > BUFFER_SIZE - 1)
        size = BUFFER_SIZE - 1;
    memcpy(buffer, data, size);
    buffer[size] = '\0';
    printf("Received 1: %s\n", buffer);
}
