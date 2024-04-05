#include <Arduino.h>
#include "FlexCAN_T4.h"
#include "Nodes.h"
#include "fake_can.h"
#include "SD.h"
#include <cstdlib>

using namespace std;


unsigned long lastRecieveTime=millis();

unsigned long stopwatch=0;
bool canSend = false;


iCANflex::iCANflex(){


}

bool iCANflex::begin() {   //Coordinate the magic CAN pixies to dance together 
    Serial.begin(115200);
    // hasSD=SD.begin(BUILTIN_SDCARD);
    //  if(hasSD){
    //     String filename = "test0.csv";
    //     int filenum = 0;
    //     char a[500];
    //     filename.toCharArray(a,500);
    //     while(SD.exists(a)){
    //         filenum++;
    //         filename = "test"+(String)filenum+".csv";
    //         filename.toCharArray(a,500);
    //     }
    //     logFile = SD.open(a, O_CREAT | O_WRITE);
    //     stuffz.hasSD2 = hasSD;
    //     stuffz.logFile = logFile;
    // }
    can_primary.begin();
    can_primary.setBaudRate(1000000);
    msg.flags.extended = 1;
    can_data.begin();
    can_data.setBaudRate(1000000);
    timeRaw = "";

    return true;
}

void parseCsvLine(int* arr, string line) {
    for (int i = 0; i < numNodes; i++) {
        if (i < numNodes - 1) {
            arr[i] = stoi(line.substr(0, line.find(',')));
        } else {
            arr[i] = stoi(line);
        }
        
        line = (line.substr(line.find(',') + 1, line.length() - line.find(',') + 1));
    }
}

void iCANflex::canSimulation(bool setup) {

    // fix later to read from csv

    // APPS1:  ID=0xC8,   bytes 0-1
    // APPS2:  ID=0xC8,   bytes 2-3
    // brakes: ID=0xC8,   bytes 4-7 (front brakes 4-5, back brakes 6-7)
    // erpm:   ID=0x2016, bytes 0-3
    

    if (setup) {/*
        Serial.println("Initializing SD Card...");
        if(!SD.begin(BUILTIN_SDCARD)){
            Serial.println("CRITICAL FAULT: PLEASE INSERT SD CARD CONTAINING ECU FLASH TUNE");
        }
        else{
            Serial.println("SD INITIALIZATION SUCCESSFUL");
            File ecu_tune;
            ecu_tune = SD.open("vdmsim.csv");
            if(ecu_tune){
                Serial.print("Reading ECU FLASH....");
                String tune;
                while(ecu_tune.available()){
                    Serial.print(".");
                    tune += (char)ecu_tune.read();
                }
                ecu_tune.close();
                Serial.println("");

                simiss << (tune.c_str()); // const so put into FLASH MEMORY
                // read in torque profiles, regen profiles, and traction profiles
                Serial.println("ECU FLASH COMPLETE. GR24 TUNE DOWNLOADED.");

            }
            else {
                Serial.println("VDM SIMULATION FAILED");
            }
        }*/
        simiss << "0,0,0,0,0,0\n0,200,100,0,0,0\n1,300,200,1,1,1\n0,50,50,0,0,1000\n20,25,25,0,0,1500\n30,50,50,0,0,2000\n40,0,0,50,50,1000\n50,0,0,70,70,2000";
        getline(simiss, wasteVals, ',');
        getline(simiss, randomFlags);
        getline(simiss, wasteVals, ',');
        getline(simiss, randomLower);
        getline(simiss, wasteVals, ',');
        getline(simiss, randomHigher);

        parseCsvLine(rFlags, randomFlags);
        parseCsvLine(rLower, randomLower);
        parseCsvLine(rHigher, randomHigher);
    }

    if (!simiss.eof()) {// FIX: END AT END OF CSV
        if (timeRaw == "") {
            getline(simiss, timeRaw, ',');
        }
        if (millis() > stoi(timeRaw.substr(0, timeRaw.find(','))) * 1000) {
            byte buf[8];

            // read csv values on current row into
            string currLine;
            int nodeVals[numNodes];
            
            getline(simiss, currLine);
            parseCsvLine(nodeVals, currLine);
            
            for (int i = 0; i < numNodes; i++) {
                if (rFlags[i] == 1) {
                    nodeVals[i] = (rand() % (rHigher[i] + 1 - rLower[i])) + rLower[i];
                }
            }

            // PEDALS: (byte 0-1: APPS1, byte 2-3: APPS2, byte 4-5: Front Brakes, byte 6-7: Rear Brakes)
            buf[0] = (nodeVals[0]     ) & 0xFF;
            buf[1] = (nodeVals[0] >> 8) & 0xFF;
            buf[2] = (nodeVals[1]     ) & 0xFF;
            buf[3] = (nodeVals[1] >> 8) & 0xFF;        
            buf[4] = (nodeVals[2]     ) & 0xFF;
            buf[5] = (nodeVals[2] >> 8) & 0xFF;
            buf[6] = (nodeVals[3]     ) & 0xFF;
            buf[7] = (nodeVals[3] >> 8) & 0xFF;
            PEDALS.ID = 200;
            PEDALS.dataOut[0] = buf[0];
            PEDALS.dataOut[1] = buf[1];
            PEDALS.dataOut[2] = buf[2];
            PEDALS.dataOut[3] = buf[3];
            PEDALS.dataOut[4] = buf[4];
            PEDALS.dataOut[5] = buf[5];
            PEDALS.dataOut[6] = buf[6];
            PEDALS.dataOut[7] = buf[7];
            PEDALS.send();

            // DTI: (byte 0-3: ERPM, 4-7: junk)
            buf[0] = (nodeVals[4]      ) & 0xFF;
            buf[1] = (nodeVals[4] >> 8 ) & 0xFF;
            buf[2] = (nodeVals[4] >> 16) & 0xFF;
            buf[3] = (nodeVals[4] >> 24) & 0xFF;
            DTI.ID = 8214;
            DTI.send(0, nodeVals[4], 8);

            Serial.println(nodeVals[0]);
            Serial.println(nodeVals[1]);
            Serial.println(nodeVals[2]);
            Serial.println(nodeVals[3]);
            Serial.println(nodeVals[4]);
            
            timeRaw = "";
        }
        
    }
}

bool iCANflex::readData(){    //Read data from the inverter or the BMS
    // lastRecieveTime = millis();
    // if(!can_primary.read(msg)){ // fix this line I beg you
    // //Serial.println("no");
    //     return 0;
    // }
    // if(!msg.flags.extended){
    //     Serial.println("WHAT IS THIS SHIT???? I WANT AN EXTENDED FRAME!!!! COME ON DUDE");
    //     return false;
    // }
    // // stuffz.CAN_CSV(msg);
    // if((msg.id & 0xFF) == DTI.ID){
    //     DTI.receive(msg.id, msg.buf);
    // }
    // else if(msg.id == BMS.ID){
    //     BMS.receive(msg.id, msg.buf);
    //     Serial.print("BMS "); Serial.println(msg.id);
    // }
    // else if(msg.id == fans.ID){
    //     fans.receive(msg.id, msg.buf);
    // }
    // else if(msg.id == pedals.ID){
    //     pedals.receive(msg.id, msg.buf);
    // }
    // else if(msg.id == 105){
    //     Serial.println(micros()-stopwatch);
    // }
    // else{
    //     Serial.print("Unknown CAN Id "); Serial.println(msg.id);
    // }
    // return true;
}

bool iCANflex::readData(INT32U *tim){//Read function that gives the time since the last read
    *tim = millis() - lastRecieveTime;
    return readData();
}
void iCANflex::rawData(INT32U *id, INT8U *buffer){*id=msg.id; buffer=msg.buf;}  //Gives back the raw Id and data array



void iCANflex::send(long OutId, long data, int dataLength){    //Sends 8 bytes with that Id and that data shifted left all the way
    byte stuff[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for(int i=0; i<dataLength; i++)
        stuff[i] = (data >> (i*8));
    for (int i = 0; i < 8; i++)
        msg.buf[i] = stuff[i];
    msg.id = OutId;
    if(canSend){can_primary.write(msg);} //por favor mi amor
    else{Serial.println("YOU TRIED TO SEND????? - note this will prob appear when recieving from the bms actually nevermind ill fix that now");}
}


void iCANflex::send(long OutId, byte stuff[]){    //Sends 8 bytes with that Id and that data shifted left all the way
    for (int i = 0; i < 8; i++)
        msg.buf[i] = stuff[i];
    msg.id = OutId;
    if(canSend){can_primary.write(msg);} //por favor mi amor
    else{Serial.println("YOU TRIED TO SEND????? - note this will prob appear when recieving from the bms actually nevermind ill fix that now");}
}

void iCANflex::sendDashError(byte error){
    msg.id = 2021;
    msg.buf[0] = error;
    msg.buf[1]=0;msg.buf[2]=0;msg.buf[3]=0;msg.buf[4]=0;msg.buf[5]=0;msg.buf[6]=0;msg.buf[7]=0;
    can_primary.write(msg);
}
// void I_no_can_speak_flex::ping(byte id){
//     msg.id = id;
//     stopwatch=micros();
//     can1.write(msg);
// }
void iCANflex::ping(byte id){
    msg.id = id;
    stopwatch=micros();
    can_data.write(msg);
}