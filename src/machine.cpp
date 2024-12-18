#include <Arduino.h>
#include <imxrt.h>
#include "machine.h"


/*

GLV_ON STATE

THIS STATE IS ACTIVE WHEN THE MICROCONTROLLER IS POWERED DUE TO THE 
GLV MASTER SWITCH BEING TURNED. THIS STATE IS RESPONSIBLE FOR THE IDLE 
STATE OF THE VEHICLE DYNAMICS MODULE. 

THIS STATE IS RESPONSIBLE FOR THE FOLLOWING:
    - SETTING THE DRIVE ENABLE TO 0
    - SETTING THE MOTOR CURRENT TO 0
    - WAITING FOR THE TS ACTIVE SWITCH TO BE PRESSED
    - RECONFIGURING THE TUNE PARAMETERS THROUGH THE CAN DEVICE
*/



State glv_on(iCANflex& Car) {
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);    

    // wait for the TS ACTIVE button to be pressed

    return GLV_ON;
}  


/*

PRECHARGING PROCESS


*/

State ts_precharge(iCANflex& Car) { 
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);
    // run a system check
    // begin prechargin
    return PRECHARGING;
}

State precharging(iCANflex& Car){
    // wait for precharge complete signal
    return PRECHARGING;
}

State precharge_complete(iCANflex& Car){
    return PRECHARGE_COMPLETE;
    // wait for RTD signal
}

/*

RTD_0TQ STATE 

PRECHARGING MUST BE COMPLETE BEFORE THIS STATE IS ENTERED. THIS STATE IS RESPONSIBLE FOR
THE VEHICLE DYNAMICS IN IDLE SITUATIONS WHERE EITHER NO TORQUE IS REQUESTED BY THE DRIVER OR 
THERE IS A VIOLATION THAT LIMITS TORQUE REQUESTS.

THIS STATE IS RESPONSIBLE FOR THE FOLLOWING:
    - SETTING THE DRIVE ENABLE TO 0
    - SETTING THE MOTOR CURRENT TO 0
    - ENSURING THE APPS AND BSE ARE NOT INTERFERING


*/



State rtd_0tq(iCANflex& Car, bool& BSE_APPS_violation) {
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);

    float throttle = (Car.PEDALS.getAPPS1() + Car.PEDALS.getAPPS2())/2.0;
    // float brake = (Car.PEDALS.getBrakePressureF() + Car.PEDALS.getBrakePressureR())/2.0;
    
    // only if no violation, and throttle is pressed, go to DRIVE
    if(!BSE_APPS_violation && throttle > 0.05) return DRIVE_TORQUE;

    if(BSE_APPS_violation) {
        // SEND CAN WARNING TO DASH
        if(throttle < 0.05) {
            // violation exit condition, reset violation and return to DRIVE_READY
            BSE_APPS_violation = false;
            return RTD_0TQ;
        }  
    }
    // else loop back into RTD state with Violation still true
    return RTD_0TQ;
}


/*
DRIVE_TORQUE STATE

THIS STATE IS RESPONSIBLE FOR THE VEHICLE DYNAMICS WHEN THE DRIVER IS REQUESTING TORQUE FROM THE MOTOR.
THE TORQUE IS CALCULATED THROUGH THE STANDARD EQUATION DEFINED BELOW. 
Z = X-(1-X)(X+B)(Y^P)K  0 <= Z <= 1 (CLIPPED)
X IS THROTTLE 0 TO 1
Y IS RPM LOAD 0 TO 1
B IS OFFSET 0 TO 1 
K IS MULTIPLIER 0 TO 1
P IS STEEPNESS 0 TO 5

THE CONSTANTS B, K, AND P ARE DEFINED THROUGHT THE ECU MAP IN THE SD CARD OR THE REFLASH OVER CAN.
THIS VALUE OF Z IS APPLIED TO THE MAX CURRENT SET AND WILL BE THE DRIVER REQUESTED TORQUE. 
THIS IS FOR A GENERALLY SMOOTHER TORQUE PROFILE AND DRIVABILITY.


THE DRIVE_TORQUE STATE IS ALSO RESPONSIBLE FOR CHECKING THE APPS AND BSE FOR VIOLATIONS AS WELL AS 
THE GRADIENTS OF THE TWO APPS SIGNALS TO MAKE SURE THAT THEY ARE NOT COMPROMISED. 
*/


float requested_torque(iCANflex& Car, float throttle, int rpm) {
    // z = np.clip((x - (1-x)*(x + b)*((y/5500.0)**p)*k )*100, 0, 100)
    float tq_percent = (throttle-(1-throttle)*(throttle+TORQUE_PROFILE_B)*pow(rpm/REV_LIMIT, TORQUE_PROFILE_P)*TORQUE_PROFILE_K);
    if(tq_percent > 1) tq_percent = 1;
    if(tq_percent < 0) tq_percent = 0;
    return tq_percent*MAX_MOTOR_CURRENT;
}

State drive_torque(iCANflex& Car, bool& BSE_APPS_violation) {
    float a1 = Car.PEDALS.getAPPS1();
    float a2 = Car.PEDALS.getAPPS2();
    float throttle = Car.PEDALS.getThrottle();
    float brake = (Car.PEDALS.getBrakePressureF() + Car.PEDALS.getBrakePressureR())/2;
    
    // APPS GRADIENT VIOLATION
    if(abs(a1 - (2*a2)) > 0.1){
        // send an error message on the dash
        return RTD_0TQ;
    } 
    // APPS BSE VIOLATION
    if((brake > 0.05 && a1 > 0.25) /*|| SystemsCheck::BSPD_fault(Car)*/) {
        BSE_APPS_violation = true;
        return RTD_0TQ;
    }
    Car.DTI.setDriveEnable(1);
    Car.DTI.setRCurrent(requested_torque(Car, throttle, Car.DTI.getERPM()/10.0));
    
    return DRIVE_TORQUE;
}



State regen_torque(iCANflex& Car){
    
}

/*
ERROR STATE

THIS STATE WILL HANDLE ERRORS THAT OCCUR DURING THE OPERATION OF THE VEHICLE.
THIS STATE WILL BE ENTERED WHENEVER A CRITICAL SYSTEMS FAILURE OCCURS OR WHEN THE
DRIVER REQUESTS TO STOP THE VEHICLE.

THE VEHICLE REMAINS IN THIS STATE UNTIL THE VIOLATION IS RESOLVED 

*/


State error(iCANflex& Car, bool (*errorCheck)(const iCANflex& c)) {
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);

    if(errorCheck(Car))  return ERROR;
    else {
        active_faults.erase(errorCheck);
        return ERROR_RESOLVED;
    }
    
}
