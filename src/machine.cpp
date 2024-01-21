#include <Arduino.h>
#include <imxrt.h>
#include "machine.h"

bool reject_on = true;

State off(iCANflex& Car, const vector<int>& switches) {
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);    
   
    reject_on = switches[1];
    if (switches[0] && !reject_on) return ON;
    if (!switches[0] && !switches[1]) reject_on = false;
    return OFF;
}  

// ON is when PRECHARGING BEGINS
State on(iCANflex& Car, const vector<int>& switches) { 
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);
    
    // switch 1 turned off
    if(!switches[0]){
        return OFF;
    }
    // Stay here if any startup error is detected or switch 2 is not yet on
    else if (!switches[1] || ECU_Startup_Rejection(Car)) {
        return ON;
    }
    // if switch 2 is on and no startup errors, run more systems checks and go to drive ready
    // here, switch 2 is implicitly on and ECU_Startup_Rejection is implicitly false, no need to recheck everything
    else /*if (switches[1] && !ECU_Startup_Rejection(Car))*/ {
        if(Critical_Systems_Fault(Car)) return ERROR;
        Warning_Systems_Fault(Car);
        // start powertrain cooling
        // WAIT FOR PRECHARGE COMPLETE SIGNAL FROM ACU!!!!!!
        // play RTD sound
        return DRIVE_READY;
    } 
    return ON;
}



 // PRECHARGING MUST BE COMPLETE BEFORE ENTERING THIS STATE
State drive_ready(iCANflex& Car, const vector<int>& switches, bool& BSE_APPS_violation) {
    Car.DTI.setDriveEnable(1);
    Car.DTI.setRCurrent(0);
    //start cooling system and all that 

    // switch 1 turned off 
    if(!switches[0]) { return OFF;}
    // switch 2 turned off while 1 is on
    else if(!switches[1]) { return ON;}

    float throttle = (Car.PEDALS.getAPPS1() + Car.PEDALS.getAPPS2())/2.0;
    float brake = (Car.PEDALS.getBrakePressureF() + Car.PEDALS.getBrakePressureR())/2.0;
   
    if(BSE_APPS_violation) {
        if(throttle < 0.05) {
            // violation exit condition, reset violation and return to DRIVE_READY
            BSE_APPS_violation = false;
            return DRIVE_READY;
        }
        // else loop back into DRIVE_READY state with Violation still true
    }

    // only if no violation, and throttle is pressed, go to DRIVE
    if(!BSE_APPS_violation && throttle > 0.05) {
        return DRIVE;
    }

    return DRIVE_READY;
}

float requested_torque(iCANflex& Car, float throttle, int rpm) {
    // z = np.clip((x - (1-x)*(x + b)*((y/5500.0)**p)*k )*100, 0, 100)
    float tq_percent = (throttle-(1-throttle)*(throttle+TORQUE_PROFILE_B)*pow(rpm/REV_LIMIT, TORQUE_PROFILE_P)*TORQUE_PROFILE_K);
    return tq_percent*MAX_MOTOR_CURRENT;
}

State drive(iCANflex& Car, const vector<int>& switches, bool& BSE_APPS_violation) {
    if(!switches[0]) return OFF;
    if(!switches[1]) return ON;

    float throttle = (Car.PEDALS.getAPPS1() + Car.PEDALS.getAPPS2())/2;
    float brake = (Car.PEDALS.getBrakePressureF() + Car.PEDALS.getBrakePressureR())/2;
    
    // APPS GRADIENT REDUNDANCY??

    // set violation condtion, and return to DRIVE_READY, cutting motor power. 
    if(brake > 0.05 && throttle > 0.25) {
        BSE_APPS_violation = true;
        return DRIVE_READY;
    }


    Car.DTI.setDriveEnable(1);
    Car.DTI.setRCurrent(requested_torque(Car, throttle, Car.DTI.getERPM()/10.0));
    
    return DRIVE;
}

State error(iCANflex& Car, const vector<int>& switches, State prevState, volatile bool (*errorCheck)(iCANflex& c)) {
    Car.DTI.setDriveEnable(0);
    Car.DTI.setRCurrent(0);
    
    if(!switches[0]) return OFF;
    //if(!switches[1]) return ON;

    if(errorCheck(Car)) {
        return ERROR;
    }
    else {
        float throttle = (Car.PEDALS.getAPPS1() + Car.PEDALS.getAPPS2())/2;
        if(throttle < 0.05) {
            return prevState;
        }
        return ERROR;
    }
}
