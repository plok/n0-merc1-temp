#ifndef MISC
#define MISC

struct TempReading
{
    char ucMessageID [8][17];
    float ucReading[ 8 ];
    int ucError[ 8 ];
    float ucCorrected[8];
}; 

struct CorrectionData
{
    char device[17];
    float a;
    float b;
};


#endif


