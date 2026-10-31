// Tema 02 - ESP32-C3: taskuri FreeRTOS si comunicare MQTT.
// Se folosesc fara modificari config.h, connection.h si connection.cpp.

#include "config.h"
#include "connection.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

struct AlarmEvent {
    float temperature;
    float threshold;
};

struct CommandEvent {
    char text[COMMAND_MAX_LEN];
};

QueueHandle_t alarmQueue = nullptr;
QueueHandle_t commandQueue = nullptr;

volatile uint32_t periodMs = PERIOD_MS_INITIAL;
volatile float threshold = THRESHOLD_INITIAL;

void periodicTask(void* parameter);
void sporadicTask(void* parameter);
void aperiodicTask(void* parameter);

float read_temperature(void) {
    return temperatureRead();
}

// TODO 1 - periodic: telemetrie si notificarea taskului sporadic.
void periodicTask(void* parameter) {
    (void)parameter;
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        const float temp = read_temperature();
        const float currentThreshold = threshold;
        char payload[128];
        snprintf(payload, sizeof(payload), "%s;%lld;%.2f",
                 DEVICE_ID, (long long)epoch_ms(), temp);
        if (!mqtt_publish("telemetry", payload)) {
            Serial.println("[periodic] telemetry publish failed");
        }
        Serial.printf("[periodic] temperature = %.2f C\n", temp);

        if (temp > currentThreshold) {
            AlarmEvent event = {temp, currentThreshold};
            if (xQueueSend(alarmQueue, &event, 0) != pdPASS) {
                Serial.println("[periodic] alarm queue full");
            }
        }

        const uint32_t currentPeriod = periodMs;
        TickType_t periodTicks = pdMS_TO_TICKS(currentPeriod);
        if (periodTicks == 0) periodTicks = 1;
        xTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}

// TODO 2 - sporadic: cel mult o alarma publicata la 5 secunde.
void sporadicTask(void* parameter) {
    (void)parameter;
    AlarmEvent event;
    uint32_t lastAlarmMs = 0;
    bool hasPublishedAlarm = false;

    for (;;) {
        if (xQueueReceive(alarmQueue, &event, portMAX_DELAY) == pdTRUE) {
            const uint32_t now = millis();
            if (hasPublishedAlarm &&
                (uint32_t)(now - lastAlarmMs) < MIN_INTERARRIVAL_MS) {
                Serial.println("[sporadic] alarm rejected (minimum interval)");
                continue;
            }

            char payload[128];
            snprintf(payload, sizeof(payload), "%s;%lld;%.2f;%.2f",
                     DEVICE_ID, (long long)epoch_ms(),
                     event.temperature, event.threshold);
            if (mqtt_publish("alarm", payload)) {
                lastAlarmMs = now;
                hasPublishedAlarm = true;
                Serial.printf("[sporadic] alarm sent: %.2f > %.2f\n",
                              event.temperature, event.threshold);
            } else {
                Serial.println("[sporadic] alarm publish failed");
            }
        }
    }
}

// TODO 3 - aperiodic: procesarea comenzilor din coada.
void aperiodicTask(void* parameter) {
    (void)parameter;
    CommandEvent event;

    for (;;) {
        if (xQueueReceive(commandQueue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        char response[96] = {0};
        char extra;
        unsigned long interval;
        float newThreshold;

        // %c detecteaza caractere suplimentare dupa parametru.
        if (sscanf(event.text, "SET_INTERVAL %lu %c", &interval, &extra) == 1) {
            if (interval >= 1 && interval <= UINT32_MAX &&
                pdMS_TO_TICKS((uint32_t)interval) > 0) {
                periodMs = (uint32_t)interval;
                snprintf(response, sizeof(response), "OK SET_INTERVAL %lu", interval);
            } else {
                snprintf(response, sizeof(response), "ERROR invalid interval");
            }
        } else if (sscanf(event.text, "SET_THRESHOLD %f %c", &newThreshold, &extra) == 1) {
            if (isfinite(newThreshold)) {
                threshold = newThreshold;
                snprintf(response, sizeof(response), "OK SET_THRESHOLD %.2f", newThreshold);
            } else {
                snprintf(response, sizeof(response), "ERROR invalid threshold");
            }
        } else if (strcmp(event.text, "RESET") == 0) {
            periodMs = PERIOD_MS_INITIAL;
            threshold = THRESHOLD_INITIAL;
            snprintf(response, sizeof(response), "OK RESET");
        } else if (strcmp(event.text, "STATUS") == 0) {
            snprintf(response, sizeof(response), "OK STATUS %lu %.2f",
                     (unsigned long)periodMs, (double)threshold);
        } else {
            snprintf(response, sizeof(response), "ERROR unknown or invalid command");
        }

        char payload[160];
        snprintf(payload, sizeof(payload), "%s;%lld;%s",
                 DEVICE_ID, (long long)epoch_ms(), response);
        if (!mqtt_publish("status", payload)) {
            Serial.println("[aperiodic] status publish failed");
        }
        Serial.printf("[aperiodic] %s\n", response);
    }
}

// Callback MQTT: copiaza textul, nu pastreaza pointerul primit.
void command_received(const char* text) {
    if (text == nullptr || commandQueue == nullptr) return;
    Serial.printf("[mqtt] comanda primita: %s\n", text);
    CommandEvent event = {};
    snprintf(event.text, sizeof(event.text), "%s", text);
    if (xQueueSend(commandQueue, &event, 0) != pdPASS) {
        Serial.println("[mqtt] command queue full");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(200);
    Serial.println();
    Serial.printf("=== tema 02: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    wifi_connect();
    time_sync();

    // TODO 0 (a): cozile se creeaza inainte de mqtt_start().
    alarmQueue = xQueueCreate(10, sizeof(AlarmEvent));
    commandQueue = xQueueCreate(10, sizeof(CommandEvent));
    if (alarmQueue == nullptr || commandQueue == nullptr) {
        Serial.println("ERROR: queue creation failed");
        while (true) delay(1000);
    }

    mqtt_start();

    // TODO 0 (b): taskuri cu prioritati peste loopTask si stiva >= 4096 B.
    BaseType_t p = xTaskCreate(periodicTask, "Periodic", 4096, nullptr, 3, nullptr);
    BaseType_t s = xTaskCreate(sporadicTask, "Sporadic", 4096, nullptr, 4, nullptr);
    BaseType_t a = xTaskCreate(aperiodicTask, "Aperiodic", 4096, nullptr, 2, nullptr);
    if (p != pdPASS || s != pdPASS || a != pdPASS) {
        Serial.println("ERROR: task creation failed");
    }
}

void loop() {
    delay(1000);
}
