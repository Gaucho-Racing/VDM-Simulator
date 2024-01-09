#include "main.h"

//BSE and APPS check for input at startup ONLY
static volatile bool ECU_Startup_Rejection(iCANflex& Car) {
    if (Car.PEDALS.getAPPS1() > 0.05 || 
        Car.PEDALS.getAPPS2() > 0.05 ||
        Car.PEDALS.getBrakePressureF() <= 0.05 || 
        Car.PEDALS.getBrakePressureR() <= 0.05) {
        Serial.println("ECU REJECTED STARTUP");
        // send error code to dash
        return true;
    }
    Serial.println("ECU STARTUP PASS");
    return false;
}

static volatile bool Critical_Systems_Fault(iCANflex& Car) {
    return false; //implement later
    Serial.println("CRITICAL SYSTEMS FAULT");
}

static volatile void Warning_Systems_Fault(iCANflex& Car) {
    Serial.println("NON CRITICAL ERROR CODES");
}