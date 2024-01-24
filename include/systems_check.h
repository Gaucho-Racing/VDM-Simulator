#ifndef SYSTEMS_CHECK
#define SYSTEMS_CHECK

#include "machine.h"


class SystemsCheck{
    private:
    static const int CAN_MS_THRESHOLD = 100; // msec

    public:
    static bool rtd_brake_fault(const iCANflex& Car); 

    static bool critical_sys_fault(const iCANflex& Car);
    static bool warn_sys_fault(const iCANflex& Car);


    // CAN RECIEVE FAILURES
    static bool critical_can_failure(const iCANflex& Car); 
    static bool warn_can_failure(const iCANflex& Car);

    // read bspd, ams, and imd pins as analog
    // .5v is shit -  ADC: 155
    // 3v when almost ok - ADC: 930
    // 2.4v is ok - ADC: 744
    // 1v = 310
    static bool AMS_fault(const iCANflex& Car);
    static bool IMD_fault(const iCANflex& Car);
    static bool BSPD_fault(const iCANflex& Car);

    static bool SDC_opened(const iCANflex& Car);

};

#endif
