
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

enum SystemStates 
{
    System_idle,
    Calibrating,
    Session_idle,
    Session_heating,
    Session_maintaining
};

enum CalibrationStates
{
    Calibration_idle,
    Initializing,
    Wait_temp_lo,
    Calibrating_temp_lo,
    Wait_temp_hi,
    Calibrating_temp_hi,
    Finalize
};




#endif


