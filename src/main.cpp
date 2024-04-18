//include "machine.h"
#include "main.h"

volatile State state;
volatile Mode mode;
bool (*errorCheck)(const iCANflex& Car); 
bool BSE_APPS_violation = false;

State sendToError(bool (*erFunc)(const iCANflex& Car)) {
   errorCheck = erFunc; 
   return ERROR;
}

static void SEND_SYS_CHECK_FRAMES(){ // TODO:
    Serial.println("SENDING SYS CHECK FRAMES");
    for(int i = 0; i < 5; i++){
        Serial.print(millis());
        Serial.println(SYS_CHECK_CAN_FRAME[i]);
    }
} 

// string maps for testing purposes
// -------------------------------------------------------------------------------
static unordered_map<State, string> state_to_string = {
    {ECU_FLASH, "ECU_FLASH"},
    {GLV_ON, "GLV_ON"},
    {TS_PRECHARGE, "TS_PRECHARGE"},
    {PRECHARGING, "PRECHARGING"},
    {PRECHARGE_COMPLETE, "PRECHARGE_COMPLETE"},
    {DRIVE_NULL, "DRIVE_NULL"},
    {DRIVE_TORQUE, "DRIVE_TORQUE"},
    {DRIVE_REGEN, "DRIVE_REGEN"},
    {ERROR, "ERROR"}
};

static unordered_map<Mode, string> mode_to_string = {
    {TESTING, "TESTING"},
    {LAUNCH, "LAUNCH"},
    {ENDURANCE, "ENDURANCE"},
    {AUTOX, "AUTOX"},
    {SKIDPAD, "SKIDPAD"},
    {ACC, "ACC"},
    {PIT, "PIT"}
};  

// -------------------------------------------------------------------------------

void loop(){

    Car->canSimulation(false);

    // SEND_SYS_CHECK_FRAMES();
    
    delay(500);

    // STATE MACHINE OPERATION

}



//GLV STARTUP
void setup() {
    Car = new iCANflex();
    Serial.begin(9600);
    Serial.println("Waiting for Serial Port to connect");
    while(!Serial) Serial.println("Waiting for Serial Port to connect");
    Serial.println("Connected to Serial Port 9600");

    Car->begin();
    Car->canSimulation(true);

    // set state  
    // state = GLV_ON;
}



