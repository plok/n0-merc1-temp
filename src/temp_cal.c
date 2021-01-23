#include "stdio.h"
#include "misc.h"
#include "string.h"
#include "stdbool.h"



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

//struct CorrectionData correctionDataTable[8];


void TempResultCorrect (struct TempReading *tempReading)
{
    for (int i = 0; i < 8; ++i)
    {

        if (strcmp((*tempReading).ucMessageID[i], "") == 0 )
        {
            //printf("Sensor%did       :                  Reading: %3.2f   Corrected: %3.2f \n",i, (*tempReading).ucReading[i], (*tempReading).ucCorrected[i] );
        }
        else 
        {
            //printf("Sensor %d id     :%s  Reading: %3.2f  Corrected: %3.2f \n", i, (*tempReading).ucMessageID[i], (*tempReading).ucReading[i], (*tempReading).ucCorrected[i]  );
        }

        
    }

    for (int i = 0; i < 8; ++i)
    {
        //printf("CorrectionID%d   :%s  a:%3.2f b:%3.2f \n",i,correctionDataTable[i].device, correctionDataTable[i].a,correctionDataTable[i].b );
    }

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
    
    //printf("\n%d Readings Corrected\n\n",matchcount);

    
    
};

void SetCalibrationTable (struct CorrectionData correctionDataTable)
{
     //EEPROM.set(100, correctionDataTable);
}

void GetCalibrationTable (struct CorrectionData *correctionDataTable)
{
    //EEPROM.get(100, *correctionDataTable);

}