// Week 8 - Class 2: Queue communication challenges
// Queue 1 carries button structs; queue 2 carries DHT20 temperatures.
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <Wire.h>
#include "DHT20.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_0 // ESP32-S3 BOOT button (active-low)
#define LED_GPIO GPIO_NUM_48        // LED on GPIO48 (change if your board differs)
#define QUEUE_LENGTH 5
#define TEMPERATURE_QUEUE_LENGTH 5
#define DHT20_SDA_GPIO 11
#define DHT20_SCL_GPIO 12

struct ButtonMessage
{
    uint32_t pressCount;
    uint32_t durationMs;
};

QueueHandle_t pressQueue;
QueueHandle_t temperatureQueue;
DHT20 queueDht20;

void task_monitor_button(void *pvParameters)
{
    static uint32_t pressCount = 0;
    while (1)
    {
        if (gpio_get_level(BOOT_BUTTON_GPIO) == 0)
        {                                  // pressed (active-low)
            const uint32_t pressStartMs = millis();
            vTaskDelay(pdMS_TO_TICKS(50)); // simple debounce
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0)
            {
                // Wait for release so that the press duration can be measured.
                while (gpio_get_level(BOOT_BUTTON_GPIO) == 0)
                    vTaskDelay(pdMS_TO_TICKS(10));

                ButtonMessage message;
                message.pressCount = ++pressCount;
                message.durationMs = millis() - pressStartMs;

                // Send a COPY of the complete struct.
                // Timeout 0 = never wait if the queue is full.
                if (xQueueSend(pressQueue, &message, 0) != pdPASS)
                {
                    Serial.println("Queue full -> press dropped");
                }
                else
                {
                    Serial.printf("Sent: pressCount = %lu, durationMs = %lu\n",
                                  (unsigned long)message.pressCount,
                                  (unsigned long)message.durationMs);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task_blink_led(void *pvParameters)
{
    ButtonMessage message = {0, 0};
    while (1)
    {
        // Blocked until a complete ButtonMessage arrives.
        if (xQueueReceive(pressQueue, &message, portMAX_DELAY) == pdPASS)
        {
            Serial.printf("Received: pressCount = %lu, durationMs = %lu\n",
                          (unsigned long)message.pressCount,
                          (unsigned long)message.durationMs);

            if (message.durationMs > 1000)
            {
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(2000));
                gpio_set_level(LED_GPIO, 0);
            }
            else
            {
                for (uint32_t i = 0; i < message.pressCount; i++)
                {
                    gpio_set_level(LED_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    gpio_set_level(LED_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
            }
        }
    }
}

void task_read_temperature(void *pvParameters)
{
    // Allow the sensor to finish its first startup interval.
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1)
    {
        const int status = queueDht20.read();
        if (status == DHT20_OK)
        {
            const float temperature = queueDht20.getTemperature();

            // Send a copy of the temperature to the second queue.
            if (xQueueSend(temperatureQueue, &temperature,
                           pdMS_TO_TICKS(100)) != pdPASS)
            {
                Serial.println("Temperature queue full -> sample dropped");
            }
        }
        else
        {
            Serial.printf("DHT20 read failed, status = %d\n", status);
        }

        // DHT20 should not be read more than once per second.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void task_print_temperature(void *pvParameters)
{
    float receivedTemperature = 0.0F;

    while (1)
    {
        if (xQueueReceive(temperatureQueue, &receivedTemperature,
                          portMAX_DELAY) == pdPASS)
        {
            Serial.printf("Temperature received from queue: %.2f C\n",
                          receivedTemperature);
        }
    }
}

void setup(void)
{
    Serial.begin(115200);
    pinMode(BOOT_BUTTON_GPIO, INPUT_PULLUP);
    pinMode(LED_GPIO, OUTPUT);

    if (!queueDht20.begin(DHT20_SDA_GPIO, DHT20_SCL_GPIO))
    {
        Serial.println("DHT20 not found on I2C");
    }

    // 5 slots, each holding one complete ButtonMessage.
    pressQueue = xQueueCreate(QUEUE_LENGTH, sizeof(ButtonMessage));
    // A separate queue with 5 slots, each holding one float temperature.
    temperatureQueue = xQueueCreate(TEMPERATURE_QUEUE_LENGTH, sizeof(float));

    if (pressQueue == NULL || temperatureQueue == NULL)
    {
        Serial.println("Queue creation failed (out of heap)");
        return;
    }

    xTaskCreate(task_monitor_button, "MonitorButton", 4096, NULL, 10, NULL);
    xTaskCreate(task_blink_led, "BlinkLED", 4096, NULL, 5, NULL);
    xTaskCreate(task_read_temperature, "ReadTemperature", 4096, NULL, 5, NULL);
    xTaskCreate(task_print_temperature, "PrintTemperature", 4096, NULL, 5, NULL);
}

void loop()
{
    // empty: the FreeRTOS tasks do all the work
}
