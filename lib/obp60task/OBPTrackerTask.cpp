// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3
#include <WiFi.h>
#include <PubSubClient.h>
#include "OBPTrackerTask.h"

/*

Regatta Hero: use PubSubClient preferred over MQTTClient. Less
functionality but smaller and easier.

JSON-data to send to server:
    Payload = {
        "passcode": "[code]",
        "orgid": "[orgname]",
        "raceid": "[raceid]",
        "gps": {
            "lat": 47.823938240041436,
            "lon": 8.1386857025205,
            "speed": 5.5,
            "odo": 1000,
            "age": 1000,
            "bat": 0.7,
            "timestamp": "2011-10-05T14:48:00.000Z"
        },
        "boat": {
            "boatid": "c32d8b45-92fe-44f6-8b61-42c2107dfe45",
            "sailno": "GER 11",
             "team": "OBP60team",
             "boatclass": "One off",
             "handicap": 114.0,
             "club": "BSC",
             "boatname": "Delfin"
        }
    }

mqttClient.state()

*/

void mqttCallback(char *topic, byte *payload, unsigned int length) {

    if (String(topic) == "hero/race") {
    }

}

void trackerTask(void *param) {
    TrackerData *data = (TrackerData *)param;

    // TCP client connection is needed
    GwApi::Status status;
    data->api->getStatus(status);
    if (status.wifiClientConnected) {
        data->logger->logDebug(GwLog::ERROR, "No WiFi connection. Cannot start tracker task.");
        vTaskDelete(NULL);
    }

    WiFiClient wificlient;
    PubSubClient mqtt(wificlient);

    String broker = "mqtt.regattahero.com";
    String mqttUsername = "obp60";
    String mqttPassword = "23qwecv";
    // String mqttClientID = generateClientID();

    mqtt.setServer(broker.c_str(), 1883);
    mqtt.setBufferSize(2048); // Erhöht die maximale Payload-Größe auf 512 Bytes   *** TODO WTF?
    mqtt.setCallback(mqttCallback);
    // mqtt.connect(mqttClientID.c_str(), mqttUsername.c_str(), mqttPassword.c_str())

    String subRaceStatus = "regattahero/racestatus/[orgname]/#";
    // mqtt.subscribe(subRaceStatus.c_str());

    String subOrgStatus = "regattahero/orgstatus/[orgname]/#";
    // mqtt.subscribe(subOrgStatus.c_str());

    while (true) {    
        delay(1000); // Loop time one second
    }
    vTaskDelete(NULL);
}

void createTrackerTask(TrackerData *param) {
    TaskHandle_t xHandle = NULL;
    if (xTaskCreate(trackerTask, "tracker", configMINIMAL_STACK_SIZE + 2048, param, configMAX_PRIORITIES-1, &xHandle)) {
        param->logger->logDebug(GwLog::ERROR, "Failed to create tracker task!");
    }
}
#endif
