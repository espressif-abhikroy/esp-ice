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

#ifdef _WIN32
#include <windows.h>
static void sleep(unsigned int secs) { Sleep(secs * 1000); }
#else
#include <unistd.h> // for sleep
#endif
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define BUFFER_SIZE 1024

//#define BUFFER_SIZE 4096

static juice_agent_t *agent1;
//static juice_agent_t *agent2;
static EventGroupHandle_t event_group = NULL;

static void on_state_changed1(juice_agent_t *agent, juice_state_t state, void *user_ptr);
static void on_state_changed2(juice_agent_t *agent, juice_state_t state, void *user_ptr);

static void on_candidate1(juice_agent_t *agent, const char *sdp, void *user_ptr);
static void on_candidate2(juice_agent_t *agent, const char *sdp, void *user_ptr);

static void on_gathering_done1(juice_agent_t *agent, void *user_ptr);
static void on_gathering_done2(juice_agent_t *agent, void *user_ptr);

static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);
static void on_recv2(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);
static const char *TAG = "mqtt_example";

//#define CLIENT1
#ifdef CLIENT1
#define OUR_CLIENT "1"
#define THEIR_CLIENT "2"
#define OUR_PORT 12345
#else
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
//            msg_id = esp_mqtt_client_subscribe(client, "#", 0);
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
                event->data[event->data_len] = '\0';
                juice_set_remote_description(agent1, event->data);
            }
            if (memcmp(event->topic, "/topic123789/cand" THEIR_CLIENT, event->topic_len) == 0) {
                event->data[event->data_len] = '\0';
                juice_add_remote_candidate(agent1, event->data);
            }
            if (memcmp(event->topic, "/topic123789/done" THEIR_CLIENT, event->topic_len) == 0) {
                printf("Their is DONE!!!!\n");
                xEventGroupSetBits(event_group, 2);
            }
            if (memcmp(event->topic, "/topic123789/ready" THEIR_CLIENT, event->topic_len) == 0) {
                printf("Their is ready!\n");
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

    ESP_LOGW(TAG, "Waiting for ready signal...");
    while (!xEventGroupWaitBits(event_group, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000))) {
        int msg_id = esp_mqtt_client_publish(client, "/topic123789/ready" OUR_CLIENT, "our client is ready", 0, 0, 0);
        ESP_LOGI(TAG, "published msg_id %d", msg_id);
    }
    ESP_LOGW(TAG, "Their client is ready, start");

    juice_set_log_level(JUICE_LOG_LEVEL_INFO);
//    juice_set_log_level(JUICE_LOG_LEVEL_VERBOSE);

    // Agent 1: Create agent
    juice_config_t config1;
    memset(&config1, 0, sizeof(config1));

    // STUN server example
    config1.stun_server_host = "stun.l.google.com";
    config1.stun_server_port = 19302;
    config1.local_port_range_begin = OUR_PORT;
    config1.local_port_range_end = OUR_PORT;

    config1.cb_state_changed = on_state_changed1;
    config1.cb_candidate = on_candidate1;
    config1.cb_gathering_done = on_gathering_done1;
    config1.cb_recv = on_recv1;
    config1.user_ptr = NULL;

    vTaskDelay(pdMS_TO_TICKS(1000));
    agent1 = juice_create(&config1);

    // Agent 2: Create agent
//    juice_config_t config2;
//    memset(&config2, 0, sizeof(config2));
//
//    // STUN server example
//    config2.stun_server_host = "stun.l.google.com";
//    config2.stun_server_port = 19302;
//
//    // Port range example
//    config2.local_port_range_begin = 60000;
//    config2.local_port_range_end = 61000;
//
//    config2.cb_state_changed = on_state_changed2;
//    config2.cb_candidate = on_candidate2;
//    config2.cb_gathering_done = on_gathering_done2;
//    config2.cb_recv = on_recv2;
//    config2.user_ptr = NULL;
//
//    agent2 = juice_create(&config2);

    // Agent 1: Generate local description
    static char sdp1[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(agent1, sdp1, JUICE_MAX_SDP_STRING_LEN);
    printf("Local description 1:\n%s\n", sdp1);
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/desc" OUR_CLIENT, sdp1, 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Agent 2: Receive description from agent 1
//    juice_set_remote_description(agent2, sdp1);
//
//    // Agent 2: Generate local description
//    static char sdp2[JUICE_MAX_SDP_STRING_LEN];
//    juice_get_local_description(agent2, sdp2, JUICE_MAX_SDP_STRING_LEN);
//    printf("Local description 2:\n%s\n", sdp2);

    // Agent 1: Receive description from agent 2
//    juice_set_remote_description(agent1, sdp2);

    // Agent 1: Gather candidates (and send them to agent 2)
    juice_gather_candidates(agent1);
    xEventGroupWaitBits(event_group, 2, pdTRUE, pdTRUE, portMAX_DELAY);

    usleep(10000000);
//    return 0;
//
//    // Agent 2: Gather candidates (and send them to agent 1)
//    juice_gather_candidates(agent2);
//    usleep(10000000);
//
//    // -- Connection should be finished --
//
//    // Check states
    juice_state_t state1 = juice_get_state(agent1);
//    juice_state_t state2 = juice_get_state(agent2);
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
//    if (success &=
//                (juice_get_selected_candidates(agent2, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote,
//                                               JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0)) {
//        printf("Local candidate  2: %s\n", local);
//        printf("Remote candidate 2: %s\n", remote);
//        if ((!strstr(local, "typ host") && !strstr(local, "typ prflx")) ||
//            (!strstr(remote, "typ host") && !strstr(remote, "typ prflx")))
//            success = false; // local connection should be possible
//    }

    // Retrieve addresses
    char localAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    char remoteAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    if (success &= (juice_get_selected_addresses(agent1, localAddr, JUICE_MAX_ADDRESS_STRING_LEN,
                                                 remoteAddr, JUICE_MAX_ADDRESS_STRING_LEN) == 0)) {
        printf("Local address  1: %s\n", localAddr);
        printf("Remote address 1: %s\n", remoteAddr);
    }
//    if (success &= (juice_get_selected_addresses(agent2, localAddr, JUICE_MAX_ADDRESS_STRING_LEN,
//                                                 remoteAddr, JUICE_MAX_ADDRESS_STRING_LEN) == 0)) {
//        printf("Local address  2: %s\n", localAddr);
//        printf("Remote address 2: %s\n", remoteAddr);
//    }

    // Agent 1: destroy
    juice_destroy(agent1);

    // Agent 2: destroy
//    juice_destroy(agent2);

    if (success) {
        printf("Success\n");
        return 0;
    } else {
        printf("Failure\n");
        return -1;
    }
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

// Agent 2: on state changed
static void on_state_changed2(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    printf("State 2: %s\n", juice_state_to_string(state));
    if (state == JUICE_STATE_CONNECTED) {
        // Agent 2: on connected, send a message
        const char *message = "Hello from 2";
        juice_send(agent, message, strlen(message));
    }
}

// Agent 1: on local candidate gathered
#include "esp_log.h"

static void on_candidate1(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ESP_LOGI("[1]", "Candidate 1: >>%s<<\n", sdp);
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/cand" OUR_CLIENT, sdp, 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);


    // Agent 2: Receive it from agent 1
//    juice_add_remote_candidate(agent2, sdp);
}

// Agent 2: on local candidate gathered
static void on_candidate2(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    ESP_LOGI("[2]", "Candidate 2: >>%s<<\n", sdp);

    // Agent 1: Receive it from agent 2
    juice_add_remote_candidate(agent1, sdp);
}

// Agent 1: on local candidates gathering done
static void on_gathering_done1(juice_agent_t *agent, void *user_ptr) {
//    printf();
    ESP_LOGI("[1]", "Gathering done 1\n");
//    juice_set_remote_gathering_done(agent2); // optional
    int msg_id = esp_mqtt_client_publish(client, "/topic123789/done" OUR_CLIENT, "gathering done", 0, 0, 0);
    ESP_LOGI(TAG, "published msg_id %d", msg_id);

}

// Agent 2: on local candidates gathering done
static void on_gathering_done2(juice_agent_t *agent, void *user_ptr) {
    ESP_LOGI("[2]", "Gathering done 2\n");
    printf("Gathering done 2\n");
    juice_set_remote_gathering_done(agent1); // optional
}

// Agent 1: on message received
static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    char buffer[BUFFER_SIZE];
    if (size > BUFFER_SIZE - 1)
        size = BUFFER_SIZE - 1;
    memcpy(buffer, data, size);
    buffer[size] = '\0';
    printf("Received 1: %s\n", buffer);
}

// Agent 2: on message received
static void on_recv2(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    char buffer[BUFFER_SIZE];
    if (size > BUFFER_SIZE - 1)
        size = BUFFER_SIZE - 1;
    memcpy(buffer, data, size);
    buffer[size] = '\0';
    printf("Received 2: %s\n", buffer);
}
