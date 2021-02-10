#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"
#include "freertos/task.h"
#include "misc.h"
#include "string.h"

#define MAX_DEVICES (8)
#define DS18B20_RESOLUTION (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD (1000)
#define GPIO_DS18B20_0 26

extern enum SystemStates SystemState;
extern enum CalibrationStates CalibrationState;

// TaskHandle_t xHandle = NULL;

void Calibration_Intinialize(float temp_lo, float temp_hi, int size, float precision, struct TempReading TempReading);
void Calibrate(struct TempReading tempReading);
void TempResultCorrect (struct TempReading *tempReading);

void vTaskCode(void *pvParameters)
{

    QueueHandle_t messageQueue = ( QueueHandle_t ) pvParameters;
    TickType_t last_wake_time = xTaskGetTickCount();
    int errors_count[MAX_DEVICES] = {0};
    int sample_count = 0;

    OneWireBus *owb;
    int num_devices = 0;
    DS18B20_Info *devices[MAX_DEVICES] = {0};

    // Create a 1-Wire bus, using the RMT timeslot driver

    owb_rmt_driver_info rmt_driver_info;
    owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(owb, true); // enable CRC check for ROM code

    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Find all connected devices
    printf("Find devices:\n");
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", num_devices, rom_code_s);
        device_rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        owb_search_next(owb, &search_state, &found);
    }
    printf("Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");

    // Create DS18B20 devices on the 1-Wire bus
    for (int i = 0; i < num_devices; ++i)
    {
        DS18B20_Info *ds18b20_info = ds18b20_malloc(); // heap allocation
        devices[i] = ds18b20_info;

        if (num_devices == 1)
        {
            printf("Single device optimisations enabled\n");
            ds18b20_init_solo(ds18b20_info, owb); // only one device on bus
        }
        else
        {
            ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true); // enable CRC check on all reads
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }

    bool parasitic_power = false;
    ds18b20_check_for_parasite_power(owb, &parasitic_power);
    if (parasitic_power)
    {
        printf("Parasitic-powered devices detected");
    }

    // In parasitic-power mode, devices cannot indicate when conversions are complete,
    // so waiting for a temperature conversion must be done by waiting a prescribed duration
    owb_use_parasitic_power(owb, parasitic_power);

    // Read temperatures more efficiently by starting conversions on all devices at the same time

    if (num_devices > 0)
    {
        for (;;)
        {
            last_wake_time = xTaskGetTickCount();

            ds18b20_convert_all(owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            // (using printf before reading may take too long)
            float readings[MAX_DEVICES] = {0.1};
            char deviceIds[MAX_DEVICES][17] = {0x0};

            DS18B20_ERROR errors[MAX_DEVICES] = {0};

            for (int i = 0; i < num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(devices[i], &readings[i]);
            }

            // Print results in a separate loop, after all have been read
            printf("measurement %d\n", ++sample_count);

            for (int i = 0; i < num_devices; ++i)
            {
               // printf("Sensor %d: %3.2f\n", i, readings[i]);
                // "Sensor %f",  readings[i]
            }

            for (int i = 0; i < num_devices; ++i)
            {
                owb_string_from_rom_code((*devices[i]).rom_code, deviceIds[i], sizeof(deviceIds[i]));
                if (errors[i] != DS18B20_OK)
                {
                    ++errors_count[i];
                }
            }

            struct TempReading tempReading = {
                .ucMessageID = {{}},
                .ucReading = {0.0f},
                .ucError = {0}
            };

            memcpy(tempReading.ucReading, readings, sizeof(tempReading.ucReading) );
            memcpy(tempReading.ucError, errors, sizeof(tempReading.ucError) );
            memcpy(tempReading.ucMessageID, deviceIds, sizeof(tempReading.ucMessageID) );
                       
            TempResultCorrect(&tempReading);

            if(sample_count == 3)
            {
                SystemState = Calibrating;
                printf("SystemState to Calibration\n");
                Calibration_Intinialize(22, 23, 20, 0.13, tempReading);
            }
            
            if(SystemState == System_idle)
            {
                for(int i = 0; i < 8; i++)
                {
                    if(strcmp(tempReading.ucMessageID[i],"") != 0)
                    {
                        printf("%d\t%d\t%3.2f\t%3.2f\n", xTaskGetTickCount(), i, tempReading.ucReading[i],tempReading.ucCorrected[i]);
                    }
                }
                printf("\n");
            }
            
            if(SystemState == Calibrating)
            {
                if(CalibrationState != Calibration_idle)
                {
                    Calibrate(tempReading);
                }
                else
                {
                    SystemState = System_idle;
                    printf("SystemState to Idle\n");
                }
            }
           

            xQueueSend(messageQueue, &tempReading, 20);
            vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }
    else
    {
        printf("\nNo DS18B20 devices detected!\n");
    }
}

void temp_svc_start(QueueHandle_t xMessageBuffer)
{

    xTaskCreatePinnedToCore(vTaskCode, "c", 4096, (void *) xMessageBuffer, 1, NULL, 0);

}