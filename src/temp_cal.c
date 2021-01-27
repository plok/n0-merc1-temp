#include "stdio.h"
#include "misc.h"
#include "string.h"
#include "stdbool.h"
#include "math.h"

float cal_temp_lo = 0.0;
float cal_temp_hi = 100.0;
float cal_temp_tolerance = 1.5;

int streak_size = 20;
float streak_precision = 0.20;

int sample_count = 0;
float samples[8][60];
float sample_average_precision[8][2];



enum CalibrationStates
{
    idle,
    initializing,
    wait_temp_lo,
    calibrating_temp_lo,
    wait_temp_hi,
    calibrating_temp_hi,
    finalize
};

enum CalibrationStates CalibrationState = idle;

struct CorrectionData correctionDataTable[8] =   {
                                                    { {"31011927ec8fc828"}, 1.001185332, 0.19},
                                                    { {"36011927fd0a8c28"}, 0.997991542, 0.06},
                                                    { {"47011927f400ba28"}, 1.006506345, -0.12},
                                                    { {"b8011927da669228"}, 1.007080133, -0.38},
                                                    { {"                "}, 5.0, 0.5},
                                                    { {"                "}, 6.0, 0.6},
                                                    { {"                "}, 7.0, 0.7},
                                                    { {"                "}, 8.0, 0.8}
                                                    };


void Calibration_Intinialize(float temp_lo, float temp_hi, int size, float precision)
{
    printf("Initializing Calibration Module... \n");
    
    CalibrationState = initializing;

    cal_temp_lo = temp_lo;
    cal_temp_hi = temp_hi;
    streak_size = size;
    streak_precision = precision;
    
    sample_count = 0;
    //
    //implement resetting of sample_array
    //
    
    CalibrationState = wait_temp_lo;
}

void WaitForTemp(float cal_temp, struct TempReading tempReading)
{
    printf("Waiting for calibration temprature %3.2f±%1.2f°C \n", cal_temp, cal_temp_tolerance);
    
    for (int i = 0; i < 8; ++i)
    {
        if (strcmp(tempReading.ucMessageID[i], "") == 0 )
        {
            //printf("Sensor%did       :                  Reading: %3.2f   Corrected: %3.2f \n",i, tempReading.ucReading[i], tempReading.ucCorrected[i] );
            
        }
        else 
        {
            printf("%3.2f\t", tempReading.ucReading[i]);
            //printf("%lld;%d;%s;%3.2f;%3.2f\n", esp_timer_get_time() ,i, tempReading.ucMessageID[i], tempReading.ucReading[i], tempReading.ucCorrected[i] );
        }

    }
    printf("\n");


    bool next_state = true;

    for(int i=0; i<8; i++ )
    {
        if(strcmp(tempReading.ucMessageID[i],"") !=0 && fabs(tempReading.ucReading[i] - cal_temp) > cal_temp_tolerance)
        {
            next_state = false;
            i=8;
        }
    }
    
    if(next_state)
    {
        CalibrationState++;
    }
}

void CalibrateLo(struct TempReading tempReading)
{
    printf("Calibrating - Waiting for precision requirement.\n");

    int write_location = (sample_count % streak_size) ; // Overwrite results if sample_count exceeds streak_size.

    //Add new temperature values to sample array
    for(int i=0; i<8; i++ ) 
    {
        if(strcmp(tempReading.ucMessageID[i],"") !=0)
        {
            samples[i][write_location] = tempReading.ucReading[i];
        }
    }

    sample_count++; //increase sample count

    //Print Sample data
    printf("\nSamplecount: %d \nWrite location: %d\n",sample_count, write_location);

    for (int i = 0; i < streak_size; ++i)
    {
        for (int j = 0; j < 8 ; j++)
        {
            if (strcmp(tempReading.ucMessageID[j], "") != 0 )
            {
                printf("%3.2f\t",samples[j][i]);              
            }
        }
        printf("\n");
    }
    printf("\n");

    //Sample set aggregate calculation
    for(int i=0; i<8; i++ ) //loop through sample sets
    {
        if(strcmp(tempReading.ucMessageID[i],"") !=0) //Disregard no-sensor sets
        {    

            float sample_sum=0;
            float hi=0;
            float lo=0;
            int read_size;

            if(sample_count < streak_size){read_size = sample_count;} //limit sample read amount to streak_size
            else{read_size = streak_size;}

            for(int j=0; j < read_size; j++) //calculate sample set aggregates
            {
                float current_sample = samples[i][j];
                sample_sum += current_sample;
                
                if(j == 0)
                {
                    hi = current_sample;
                    lo = current_sample;
                }
                if(j > 0 && hi < current_sample) 
                {
                    hi = current_sample;
                }
                if(j > 0 && lo > current_sample) 
                {
                    lo = current_sample;
                }

                sample_average_precision[i][0] = sample_sum / read_size;
                sample_average_precision[i][1] = fabs(hi-lo); 
                
            }
        }
    }
    
    //Print Aggregation data
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8 ; j++)
        {
            if (strcmp(tempReading.ucMessageID[j], "") != 0 )
            {
                printf("%3.2f\t",sample_average_precision[j][i]);              
            }
        }
        printf("\n");
    }
    printf("\n");

    //
    //Implement communication aggregate data to front end
    //


    // Check if calibration requirements are met
    if(sample_count >= streak_size) 
    {
        bool next_state = true;

        for(int i=0; i<8;i++)
        {
            if(strcmp(tempReading.ucMessageID[i],"") !=0 && sample_average_precision[i][1] > streak_precision ) //Disregard no-sensor sets ///Implement tolarance requirement.
            { 
                next_state = false;
                i=8;
            }
        }
        if(next_state)
        {
            CalibrationState++;
        }
    }
}

void Calibrate(struct TempReading tempReading)
{
    printf("CalibrationState: %d\n", CalibrationState);
    if(CalibrationState == wait_temp_lo)
    {
        WaitForTemp(cal_temp_lo, tempReading);
    }
    if(CalibrationState == calibrating_temp_lo)
    {
        CalibrateLo(tempReading);
    }
    if(CalibrationState == wait_temp_hi)
    {
        WaitForTemp(cal_temp_hi, tempReading);
    }
        if(CalibrationState == calibrating_temp_hi)
    {
        CalibrateLo(tempReading);
    }
}

void TempResultCorrect (struct TempReading *tempReading)
{
    int matchcount = 0;
        for (int i = 0; i < 8; ++i)
    {
        bool blCorrected = 0;

        for (int j = 0; j < 8; ++j)
        {
            
            if ((strcmp((*tempReading).ucMessageID[i], "") !=0 && strcmp((*tempReading).ucMessageID[i], correctionDataTable[j].device) == 0 ))
            {
                (*tempReading).ucCorrected[i] = ((*tempReading).ucReading[i] + correctionDataTable[j].b) *correctionDataTable[j].a;
                ++matchcount;
                blCorrected = 1;
                j=8;
            }
            if (j==7 && blCorrected == 0 )
            {
                (*tempReading).ucCorrected[i] = (*tempReading).ucReading[i];
            }
        }
    }
}

void SetCalibrationTable (struct CorrectionData correctionDataTable)
{
     //EEPROM.set(100, correctionDataTable);
}

void GetCalibrationTable (struct CorrectionData *correctionDataTable)
{
    //EEPROM.get(100, *correctionDataTable);

}