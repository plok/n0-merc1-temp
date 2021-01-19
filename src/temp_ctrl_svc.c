#include "freertos/freertos.h"

#include "freertos/task.h"
#include "bleprph.h"
#include "driver/gpio.h"
#define RELAY_PIN (gpio_num_t) 2

TaskHandle_t xHandle = NULL;

float heatSetting = 0;
int loopTime = 4000;


void vHeatingLoop(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    QueueHandle_t messageQueue = ( QueueHandle_t ) pvParameters;
    float readMessage;

    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);

    for(;;) {
        if( xQueueReceive( messageQueue,
                            &( readMessage ),
                            0 ))
        {
            heatSetting = readMessage;
            printf("new percentage: %f\n", heatSetting);
        }

        double timeToHeat = heatSetting * loopTime;
        if (timeToHeat > 0) {
            gpio_set_level(RELAY_PIN, 1);
            printf("Time on: %f\n", timeToHeat);

            vTaskDelayUntil(&last_wake_time, timeToHeat / portTICK_PERIOD_MS);
        }      
        gpio_set_level(RELAY_PIN, 0);


        double timeToRest = loopTime - timeToHeat;
                    printf("Time off: %f\n", timeToRest);
        if (timeToRest > 0) {
            vTaskDelayUntil(&last_wake_time, timeToRest / portTICK_PERIOD_MS);
        }
    }

}


void temp_ctrl_svc_start(QueueHandle_t xMessageBuffer)
{

    // static uint8_t ucParameteroPass;
    xTaskCreatePinnedToCore(vHeatingLoop, "heatingloop", 4096, (void *) xMessageBuffer, 1, NULL, 0);

}