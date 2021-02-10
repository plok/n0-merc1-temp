#include "stdio.h"
#include "misc.h"
#include "string.h"
#include "stdbool.h"
#include "math.h"
#include "nvs.h"

//Calibration parameters
float cal_temp_lo = 0.0;
float cal_temp_hi = 100.0;
float cal_temp_tolerance = 1.0;
int streak_size = 20;
float streak_precision = 0.20;

//Calibration data
int sensor_count = 0;
int sample_count = 0;
float samples[8][60];
float samples_lo_average_precision[8][2];
float samples_hi_average_precision[8][2];
float samples_correction_parameters[8][2];

//Forward Declarations
int CountSensors(struct TempReading tempReading);
void WaitForTemp(float cal_temp, struct TempReading tempReading);
void CalibrateLoHi(float cal_temp, float sample_average_precision[8][2], struct TempReading tempReading);
void FinalizeCalibration(struct TempReading tempReading);
void PrintSampleData(float samples[8][60], int read_size, int write_location);
void PrintAggregationData(float sample_average_precision[8][2]);
void PrintCorrectionDataTable();


enum CalibrationStates CalibrationState = Calibration_idle;

struct CorrectionData correctionDataTable[8] =   {
                                                    { {"31011927ec8fc828"}, 1.001185332, 0.19},
                                                    { {"36011927fd0a8c28"}, 0.997991542, 0.06},
                                                    { {"47011927f400ba28"}, 1.006506345, -0.12},
                                                    { {"ImNotARealSensor"}, 1.007080133, -0.38},
                                                    { {"                "}, 0.0, 0.0},
                                                    { {"ImNotARealSensor"}, 1.0, 0.1},
                                                    { {"                "}, 0.0, 0.0},
                                                    { {"                "}, 0.0, 0.0}
                                                    };

void Calibration_Intinialize(float temp_lo, float temp_hi, int size, float precision, struct TempReading tempReading)
{
    printf("Initializing Calibration.\n");
    
    CalibrationState = Initializing;

    cal_temp_lo = temp_lo;
    cal_temp_hi = temp_hi;
    streak_size = size;
    streak_precision = precision;
    
    sensor_count = CountSensors(tempReading);
    sample_count = 0;
    
    //
    //implement resetting of sample_array
    //
    
    CalibrationState++;
}

void Calibrate(struct TempReading tempReading)
{
    printf("CalibrationState: %d\n", CalibrationState);
    
    switch(CalibrationState)
    {
        case Calibration_idle:
        break;

        case Initializing:
        break;

        case Wait_temp_lo:
        WaitForTemp(cal_temp_lo, tempReading);
        break;

        case Calibrating_temp_lo:
        CalibrateLoHi(cal_temp_lo, samples_lo_average_precision, tempReading);
        break;

        case Wait_temp_hi:
        WaitForTemp(cal_temp_hi, tempReading);
        break;;

        case Calibrating_temp_hi:
        CalibrateLoHi(cal_temp_hi, samples_hi_average_precision, tempReading);
        break;

        case Finalize:
        FinalizeCalibration(tempReading);
        break;

    }
}

void WaitForTemp(float cal_temp, struct TempReading tempReading)
{
    printf("Waiting for calibration temprature %3.2f±%1.2f°C \n", cal_temp, cal_temp_tolerance);
    
    for (int i = 0; i < 8; ++i)
    {
        if (strcmp(tempReading.ucMessageID[i], "") != 0 )
        {
             printf("%3.2f\t", tempReading.ucReading[i]);
            
        }
    }
    printf("\n");


    bool next_state = true;

    for(int i=0; i< sensor_count; i++ )
    {
        if(fabs(tempReading.ucReading[i] - cal_temp) > cal_temp_tolerance)
        {
            next_state = false;
            i=sensor_count;
        }
    }
    
    if(next_state)
    {
        CalibrationState++;
    }
}

void CalibrateLoHi(float cal_temp, float sample_average_precision[8][2], struct TempReading tempReading)
{
    printf("Calibrating - Waiting for precision requirement.\n");

    int write_location = (sample_count % streak_size) ; // Overwrite results if sample_count exceeds streak_size.

    //Check if measurement is still withn tolerance
    for(int i=0; i< sensor_count; i++ )
    {
        if(fabs(tempReading.ucReading[i] - cal_temp) > cal_temp_tolerance)
        {
            CalibrationState--;
            sample_count = 0;
            return;
        }
    }

    //Add new temperature values to sample array
    for(int i = 0; i < sensor_count; i++ ) 
    {
        samples[i][write_location] = tempReading.ucReading[i];
    }

    sample_count++; //increase sample count

    //Determine amount of samples in set.
    int read_size;
    if(sample_count < streak_size){read_size = sample_count;} //limit sample read amount to streak_size
    else{read_size = streak_size;}


    //Print Sample data
    PrintSampleData(samples, read_size, write_location);

    //Sample set aggregate calculation
    for(int i=0; i< sensor_count; i++ ) //loop through sample sets
    {
        float sample_sum=0;
        float hi=0;
        float lo=0;
        
        for(int j=0; j < read_size; j++) //calculate sample set aggregates
        {
            float current_sample = samples[i][j]; //Calculate sample set average
            sample_sum += current_sample;
            
            if(j == 0) //Calculate date set bandwidth
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

    //Print aggergation data
    PrintAggregationData(sample_average_precision);

    //
    //Implement communication aggregate data to frontend
    //


    // Check if calibration requirements are met
    if(sample_count >= streak_size) 
    {
        bool next_state = true;

        for(int i=0; i < sensor_count;i++)
        {
            if(sample_average_precision[i][1] > streak_precision )
            { 
                next_state = false;
                i = sensor_count;
            }
        }
        if(next_state)
        {
            printf("Calibration of %3.2f°C completed.\n\n", cal_temp);

            CalibrationState++;
            sample_count = 0;
        }
    }
}

void FinalizeCalibration(struct TempReading tempReading)
{   
    printf("Original CorrectionDataTable:\n");
    PrintCorrectionDataTable();

    //Calculate correction parameters from calibration data.
    for(int i = 0; i < sensor_count; i++)
    {
        float temp_hi = samples_hi_average_precision[i][0];
        float temp_lo = samples_lo_average_precision[i][0];

        float rc = (temp_hi-temp_lo)/(cal_temp_hi-cal_temp_lo);
        float y_crossing = temp_lo - (cal_temp_lo * rc);

        float a = 1/rc;
        float b = -y_crossing;

        //Save results in correction parameters
        samples_correction_parameters[i][0] = a;
        samples_correction_parameters[i][1] = b;
    }

    //Update CorrectionDataTable with new parameters
    for(int i = 0; i < sensor_count; i++)
    {
        //Find matching sensor id in CorrectionDataTable and overwrite parameters.
        bool sensor_match = false;

        for(int j = 0; j < 8; j++)
        {
            if(strcmp(tempReading.ucMessageID[i], correctionDataTable[j].device) == 0)
            {
                correctionDataTable[j].a = samples_correction_parameters[i][0];
                correctionDataTable[j].b = samples_correction_parameters[i][1];
                
                sensor_match = true;
                j=8;
            }
        }

        //If no match is found in CorrectionDataTable, find first empty space and write parameters.
        if(!sensor_match)
        {
            for(int j = 0; j < 8; j++)
            {
                if(strcmp(correctionDataTable[j].device, "                ") == 0)
                {
                    memcpy(correctionDataTable[j].device, tempReading.ucMessageID[i], sizeof(tempReading.ucMessageID[i]));

                    correctionDataTable[j].a = samples_correction_parameters[i][0];
                    correctionDataTable[j].b = samples_correction_parameters[i][1];
                    
                    j=8;
                }
            }
        }
    }
    printf("Modified CorrectionDataTable:\n");
    PrintCorrectionDataTable();

    CalibrationState = Calibration_idle;
}

void TempResultCorrect (struct TempReading *tempReading)
{
    for (int i = 0; i < 8; ++i)
    {
        bool blCorrected = 0;

        for (int j = 0; j < 8; ++j)
        {
            if ((strcmp((*tempReading).ucMessageID[i], "") !=0 && strcmp((*tempReading).ucMessageID[i], correctionDataTable[j].device) == 0 ))
            {
                (*tempReading).ucCorrected[i] = ((*tempReading).ucReading[i] + correctionDataTable[j].b) * correctionDataTable[j].a;

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

int CountSensors(struct TempReading tempReading)
{
    int sensor_count = 0;

    for(int i = 0 ; i < 8; i++)
    {
        if (strcmp(tempReading.ucMessageID[i], "") != 0 )
        {
            sensor_count++;  
        }
    }
    return sensor_count;
}

void PrintSampleData(float samples[8][60], int read_size, int write_location )
{
       printf("\nSamplecount: %d \nWrite location: %d\n", sample_count, write_location);

    for (int i = 0; i < read_size; ++i)
    {
        for (int j = 0; j < sensor_count ; j++)
        {
            printf("%3.2f\t",samples[j][i]);
        }
        printf("\n");
    }
    printf("\n");
}

void PrintAggregationData(float sample_average_precision[8][2])
{
        //Print Aggregation data
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < sensor_count ; j++)
        {
            printf("%3.2f\t",sample_average_precision[j][i]);              
        }
        printf("\n");
    }
    printf("\n");
}

void PrintCorrectionDataTable()
{
    for(int i = 0; i < 8; i++)
    {
        printf("%s\t%3.2f\t%3.2f\n", correctionDataTable[i].device, correctionDataTable[i].a, correctionDataTable[i].b);
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