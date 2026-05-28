//=============================================================================
//Project Lynxmotion Phoenix
//Description: Phoenix software
//Software version: V2.0
//Date: 29-10-2009
//Programmer: Jeroen Janssen [aka Xan]
//         Kurt Eckhardt(KurtE) converted to C and Arduino
//
// This version of the Phoenix code was ported over to the Arduino Environment
// and is specifically configured for the Lynxmotion BotBoarduino 
//=============================================================================
// Header Files
//=============================================================================

#define DEFINE_HEX_GLOBALS
#if ARDUINO>99
#include <Arduino.h>
#else
#endif
#include <PS2X_lib.h>
#include <pins_arduino.h>
#include <SoftwareSerial.h>
#include "Hex_globals.h"
#define BalanceDivFactor 6    

//--------------------------------------------------------------------
//[TABLES]
#include "Hex_Tables.h"

//--------------------------------------------------------------------
//[GLOABAL]
INCONTROLSTATE   g_InControlState;
Leg              g_Legs[6];
ServoDriver      g_ServoDriver;
bool          g_fDebugOutput = true;

// Prototypes for methods in Hexapod.cpp
void GaitSelect() { g_Hexapod.GaitSelect(); }
void GaitSeq() { g_Hexapod.GaitSeq(); }
void Gait(uint8_t leg) { g_Hexapod.Gait(leg); }
void BalanceBody() { g_Hexapod.BalanceBody(); }
void CheckAngles() { g_Hexapod.CheckAngles(); }
bool CheckVoltage() { return g_Hexapod.CheckVoltage(); }
void BalCalcOneLeg(short x, short z, short y, uint8_t leg) { g_Hexapod.BalCalcOneLeg(x, z, y, leg); }

// Prototypes for local functions
void WriteOutputs();
void SingleLegControl();
void StartUpdateServos();
void UpdateIK();
void SoundNoTimer(uint8_t _pin, unsigned long duration, unsigned int frequency);
#ifdef OPT_TERMINAL_MONITOR
bool TerminalMonitor(void);
#endif
void AdjustLegPositionsToBodyHeight(void);

void setup(){
    g_fDebugOutput = false;
#ifdef DBGSerial    
    DBGSerial.begin(57600);
#endif
    g_ServoDriver.Init();
    if (g_InputController.FIsDiagnosticModeRequested()) g_ServoDriver.SSCForwarder();
    
    // Boot Beep
    pinMode(SOUND_PIN, OUTPUT);
    MSound(3, 60, 2000, 80, 2250, 100, 2500);
    
    delay(10);
    
    for (uint8_t LegIndex= 0; LegIndex <= 5; LegIndex++ ) {
        g_Legs[LegIndex].init(LegIndex);
    }
    
    g_InControlState.SelectedLeg = 255;
    g_Hexapod.PrevSelectedLeg = 255;
    g_InControlState.BodyPos.x = g_InControlState.BodyPos.y = g_InControlState.BodyPos.z = 0;
    g_InControlState.BodyRot1.x = g_InControlState.BodyRot1.y = g_InControlState.BodyRot1.z = 0;
    g_InControlState.TravelLength.x = g_InControlState.TravelLength.y = g_InControlState.TravelLength.z = 0;
    g_InControlState.SLLeg.x = g_InControlState.SLLeg.y = g_InControlState.SLLeg.z = 0;
    g_InControlState.fSLHold = false;
    g_InControlState.BalanceMode = false;
    g_InControlState.InputTimeDelay = 0;
    
    g_Hexapod.BodyRotOffsetX = g_Hexapod.BodyRotOffsetY = g_Hexapod.BodyRotOffsetZ = 0;
    
    g_InControlState.GaitType = 1;
    g_InControlState.BalanceMode = 0;
    g_InControlState.LegLiftHeight = 50;
    g_InControlState.ForceGaitStepCnt = 0;
    g_Hexapod.GaitStep = 1;
    GaitSelect();
    
    g_InputController.Init();
    g_Hexapod.ServoMoveTime = 150;
    g_InControlState.fHexOn = 0;
    g_InControlState.fPrev_HexOn = 0;
    g_Hexapod.fLowVoltageShutdown = false;
}

void loop(void)
{
    static unsigned long lTransitionStartTime = 0;
    g_Hexapod.lLogicTimeStart = micros();
    g_Hexapod.lTimerStart = millis(); 

    if (!g_Hexapod.fLowVoltageShutdown) g_InputController.ControlInput();

    if (CheckVoltage()) {
        if (g_InControlState.fPrev_HexOn || (g_Hexapod.AllDown == 0)) {
            Serial.println("[LOG] Low Voltage Shutdown Triggered");
            g_InControlState.fHexOn = false;
            UpdateIK();
            g_Hexapod.ServoMoveTime = 600;
            StartUpdateServos();
            g_ServoDriver.CommitServoDriver(g_Hexapod.ServoMoveTime);
            MSound(3, 100, 2500, 80, 2250, 60, 2000);
            delay(600);
        }
        g_ServoDriver.FreeServos();
        g_Hexapod.Eyes = 0;
        g_InControlState.fHexOn = false;
        g_InControlState.fPrev_HexOn = false;
    } else {
        // [STARTUP TRANSITION]
        if (g_InControlState.fHexOn && !g_InControlState.fPrev_HexOn) {
            Serial.println("[LOG] Engaging motors at height 0");
            
            // 1. Snap to sitting position to engage motors without violent movement
            short targetHeight = g_InControlState.BodyPos.y;
            g_InControlState.BodyPos.y = 0;
            UpdateIK(); 
            g_Hexapod.ServoMoveTime = 200; // Snap
            StartUpdateServos();
            g_ServoDriver.CommitServoDriver(g_Hexapod.ServoMoveTime);
            
            MSound(3, 60, 2000, 80, 2250, 100, 2500); // Beep during snap
            delay(200);

            // 2. Smoothly stand up
            Serial.println("[LOG] Standing Up (600ms move)");
            g_InControlState.BodyPos.y = targetHeight;
            UpdateIK();
            g_Hexapod.ServoMoveTime = 600;
            StartUpdateServos();
            g_ServoDriver.CommitServoDriver(g_Hexapod.ServoMoveTime);
            delay(600); // Block to ensure move completes
            
            lTransitionStartTime = millis();
            g_Hexapod.Eyes = 1;
            g_InControlState.fPrev_HexOn = true;
            return; 
        }

        // [SHUTDOWN TRANSITION]
        if (!g_InControlState.fHexOn && g_InControlState.fPrev_HexOn) {
            Serial.println("[LOG] Shutting Down (600ms move)");
            MSound(3, 100, 2500, 80, 2250, 60, 2000); // Beep first
            UpdateIK(); 
            g_Hexapod.ServoMoveTime = 600;
            StartUpdateServos();
            g_ServoDriver.CommitServoDriver(g_Hexapod.ServoMoveTime);
            delay(600); // Block to ensure move completes
            g_ServoDriver.FreeServos();
            g_Hexapod.Eyes = 0;
            g_InControlState.fPrev_HexOn = false;
            return; 
        }

        WriteOutputs();

#ifdef OPT_GPPLAYER
        g_ServoDriver.GPPlayer();
#endif

        if (g_InControlState.fHexOn) {
            SingleLegControl();
            GaitSeq();
            UpdateIK();
                    
            if ((abs(g_InControlState.TravelLength.x)>cTravelDeadZone) || (abs(g_InControlState.TravelLength.z)>cTravelDeadZone) || (abs(g_InControlState.TravelLength.y*2)>cTravelDeadZone)) {         
                g_Hexapod.ServoMoveTime = g_Hexapod.NomGaitSpeed + (g_InControlState.InputTimeDelay*2) + g_InControlState.SpeedControl;
                if (g_InControlState.BalanceMode) g_Hexapod.ServoMoveTime += 100;
            } else {
                g_Hexapod.ServoMoveTime = 200 + g_InControlState.SpeedControl;
            }
            
            StartUpdateServos();
            
            unsigned long lSerialStart = micros();
            g_ServoDriver.CommitServoDriver(g_Hexapod.ServoMoveTime);
            unsigned long lSerialFrameTime = micros() - lSerialStart;
            if (lSerialFrameTime > g_Hexapod.lSerialTimeMax) g_Hexapod.lSerialTimeMax = lSerialFrameTime;

            g_Hexapod.fContinueWalking = false;
            for (uint8_t LegIndex = 0; LegIndex <= 5; LegIndex++) {
                if ( (abs(g_Legs[LegIndex].gaitPosX) > 2) || (abs(g_Legs[LegIndex].gaitPosY) > 2) || (abs(g_Legs[LegIndex].gaitPosZ) > 2) || (abs(g_Legs[LegIndex].gaitRotY) > 2) ) {
                    g_Hexapod.fContinueWalking = true;
                    break;
                }
            }

            if (g_Hexapod.fWalking || g_Hexapod.fContinueWalking) {
                g_Hexapod.fWalking = g_Hexapod.fContinueWalking;
                g_Hexapod.lTimerEnd = millis();
                if (g_Hexapod.lTimerEnd > g_Hexapod.lTimerStart) g_Hexapod.CycleTime = g_Hexapod.lTimerEnd-g_Hexapod.lTimerStart;
                else g_Hexapod.CycleTime = 0xffffffffL - g_Hexapod.lTimerEnd + g_Hexapod.lTimerStart + 1;

                unsigned long lFrameLogicEnd = micros();
                unsigned long lFrameLogicTime = lFrameLogicEnd - g_Hexapod.lLogicTimeStart;
                if (lFrameLogicTime > g_Hexapod.lLogicTimeMax) g_Hexapod.lLogicTimeMax = lFrameLogicTime;

                if (g_Hexapod.fShowProfile && (millis() - g_Hexapod.lProfileWindowStart > 1000)) {
                    Serial.print("[PROFILE] Total: ");
                    Serial.print(g_Hexapod.lLogicTimeMax);
                    Serial.print(" us (Serial: ");
                    Serial.print(g_Hexapod.lSerialTimeMax);
                    Serial.print(" us, Math: ");
                    Serial.print(g_Hexapod.lLogicTimeMax - g_Hexapod.lSerialTimeMax);
                    Serial.println(" us)");
                    g_Hexapod.lLogicTimeMax = 0;
                    g_Hexapod.lSerialTimeMax = 0;
                    g_Hexapod.lProfileWindowStart = millis();
                }

                delay(min(max ((int)(g_Hexapod.PrevServoMoveTime - g_Hexapod.CycleTime), 1), (int)g_Hexapod.NomGaitSpeed)); 
            }
        }
    }

#ifdef OPT_TERMINAL_MONITOR  
    TerminalMonitor();
#endif

    delay(20);
    g_Hexapod.PrevServoMoveTime = g_Hexapod.ServoMoveTime;
    g_InControlState.fPrev_HexOn = g_InControlState.fHexOn;
}

void UpdateIK() {
    g_Hexapod.TotalTransX = g_Hexapod.TotalTransZ = g_Hexapod.TotalTransY = 0;
    g_Hexapod.TotalXBal1 = g_Hexapod.TotalYBal1 = g_Hexapod.TotalZBal1 = 0;

    if (g_InControlState.BalanceMode) {
        for (uint8_t LegIndex = 0; LegIndex <= 2; LegIndex++) {
            BalCalcOneLeg (-g_Legs[LegIndex].posX+g_Legs[LegIndex].gaitPosX, 
                        g_Legs[LegIndex].posZ+g_Legs[LegIndex].gaitPosZ, 
                        (g_Legs[LegIndex].posY-(short)cInitPosY[LegIndex])+g_Legs[LegIndex].gaitPosY, LegIndex);
        }
        for (uint8_t LegIndex = 3; LegIndex <= 5; LegIndex++) {
            BalCalcOneLeg(g_Legs[LegIndex].posX+g_Legs[LegIndex].gaitPosX, 
                        g_Legs[LegIndex].posZ+g_Legs[LegIndex].gaitPosZ, 
                        (g_Legs[LegIndex].posY-(short)cInitPosY[LegIndex])+g_Legs[LegIndex].gaitPosY, LegIndex);
        }
        BalanceBody();
    }
            
    g_Hexapod.IKSolution = g_Hexapod.IKSolutionWarning = g_Hexapod.IKSolutionError = 0;
            
    for (uint8_t LegIndex = 0; LegIndex <= 2; LegIndex++) {    
        g_Legs[LegIndex].calculateBodyFK(-g_Legs[LegIndex].posX+g_InControlState.BodyPos.x+g_Legs[LegIndex].gaitPosX - g_Hexapod.TotalTransX,
                g_Legs[LegIndex].posY+g_InControlState.BodyPos.y+g_Legs[LegIndex].gaitPosY - g_Hexapod.TotalTransY,
                g_Legs[LegIndex].posZ+g_InControlState.BodyPos.z+g_Legs[LegIndex].gaitPosZ - g_Hexapod.TotalTransZ,
                g_Legs[LegIndex].gaitRotY, g_Hexapod.BodyRotOffsetX, g_Hexapod.BodyRotOffsetY, g_Hexapod.BodyRotOffsetZ, g_Hexapod.TotalXBal1, g_Hexapod.TotalYBal1, g_Hexapod.TotalZBal1);

        g_Legs[LegIndex].calculateLegIK(g_Legs[LegIndex].posX-g_InControlState.BodyPos.x+g_Legs[LegIndex].bodyFKPosX-(g_Legs[LegIndex].gaitPosX - g_Hexapod.TotalTransX), 
                g_Legs[LegIndex].posY+g_InControlState.BodyPos.y-g_Legs[LegIndex].bodyFKPosY+g_Legs[LegIndex].gaitPosY - g_Hexapod.TotalTransY,
                g_Legs[LegIndex].posZ+g_InControlState.BodyPos.z-g_Legs[LegIndex].bodyFKPosZ+g_Legs[LegIndex].gaitPosZ - g_Hexapod.TotalTransZ);
    }

    for (uint8_t LegIndex = 3; LegIndex <= 5; LegIndex++) {
        g_Legs[LegIndex].calculateBodyFK(g_Legs[LegIndex].posX-g_InControlState.BodyPos.x+g_Legs[LegIndex].gaitPosX - g_Hexapod.TotalTransX,
                g_Legs[LegIndex].posY+g_InControlState.BodyPos.y+g_Legs[LegIndex].gaitPosY - g_Hexapod.TotalTransY,
                g_Legs[LegIndex].posZ+g_InControlState.BodyPos.z+g_Legs[LegIndex].gaitPosZ - g_Hexapod.TotalTransZ,
                g_Legs[LegIndex].gaitRotY, g_Hexapod.BodyRotOffsetX, g_Hexapod.BodyRotOffsetY, g_Hexapod.BodyRotOffsetZ, g_Hexapod.TotalXBal1, g_Hexapod.TotalYBal1, g_Hexapod.TotalZBal1);
        g_Legs[LegIndex].calculateLegIK(g_Legs[LegIndex].posX+g_InControlState.BodyPos.x-g_Legs[LegIndex].bodyFKPosX+g_Legs[LegIndex].gaitPosX - g_Hexapod.TotalTransX,
                g_Legs[LegIndex].posY+g_InControlState.BodyPos.y-g_Legs[LegIndex].bodyFKPosY+g_Legs[LegIndex].gaitPosY - g_Hexapod.TotalTransY,
                g_Legs[LegIndex].posZ+g_InControlState.BodyPos.z-g_Legs[LegIndex].bodyFKPosZ+g_Legs[LegIndex].gaitPosZ - g_Hexapod.TotalTransZ);
    }
    
    CheckAngles();
}

void StartUpdateServos() {        
    g_ServoDriver.BeginServoUpdate();
    for (uint8_t i = 0; i <= 5; i++) {
#ifdef c4DOF
        g_ServoDriver.OutputServoInfoForLeg(i, g_Legs[i].coxaAngle, g_Legs[i].femurAngle, g_Legs[i].tibiaAngle, g_Legs[i].tarsAngle);
#else
        g_ServoDriver.OutputServoInfoForLeg(i, g_Legs[i].coxaAngle, g_Legs[i].femurAngle, g_Legs[i].tibiaAngle);
#endif      
    }
}

void WriteOutputs() {
#ifdef cEyesPin
    digitalWrite(cEyesPin, g_Hexapod.Eyes);
#endif        
}

void SingleLegControl() {
    g_Hexapod.AllDown = true;
    for (uint8_t LegIndex = 0; LegIndex < 6; LegIndex++) if (g_Legs[LegIndex].posY != (short)cInitPosY[LegIndex]) g_Hexapod.AllDown = false;

    if (g_InControlState.SelectedLeg <= 5) {
        if (g_InControlState.SelectedLeg != g_Hexapod.PrevSelectedLeg) {
            if (g_Hexapod.AllDown) {
                g_Legs[g_InControlState.SelectedLeg].posY = (short)cInitPosY[g_InControlState.SelectedLeg]-20;
                g_Hexapod.PrevSelectedLeg = g_InControlState.SelectedLeg;
            } else {
                g_Legs[g_Hexapod.PrevSelectedLeg].posX = (short)cInitPosX[g_Hexapod.PrevSelectedLeg];
                g_Legs[g_Hexapod.PrevSelectedLeg].posY = (short)cInitPosY[g_Hexapod.PrevSelectedLeg];
                g_Legs[g_Hexapod.PrevSelectedLeg].posZ = (short)cInitPosZ[g_Hexapod.PrevSelectedLeg];
            }
        } else if (!g_InControlState.fSLHold) {
            g_Legs[g_InControlState.SelectedLeg].posY += g_InControlState.SLLeg.y;
            g_Legs[g_InControlState.SelectedLeg].posX = (short)cInitPosX[g_InControlState.SelectedLeg]+g_InControlState.SLLeg.x;
            g_Legs[g_InControlState.SelectedLeg].posZ = (short)cInitPosZ[g_InControlState.SelectedLeg]+g_InControlState.SLLeg.z;
        }
    } else {
        if (!g_Hexapod.AllDown) {
            for (uint8_t LegIndex = 0; LegIndex <= 5; LegIndex++) {
                g_Legs[LegIndex].posX = (short)cInitPosX[LegIndex];
                g_Legs[LegIndex].posY = (short)cInitPosY[LegIndex];
                g_Legs[LegIndex].posZ = (short)cInitPosZ[LegIndex];
            }
        }
        g_Hexapod.PrevSelectedLeg = 255;
    }
}

void SoundNoTimer(uint8_t _pin, unsigned long duration,  unsigned int frequency) {
#ifdef __AVR__
    volatile uint8_t *pin_port; uint8_t pin_mask;
    pinMode(_pin, OUTPUT);
    pin_port = portOutputRegister(digitalPinToPort(_pin));
    pin_mask = digitalPinToBitMask(_pin);
    long toggle_count = 2 * frequency * duration / 1000;
    long lusDelayPerHalfCycle = 1000000L/(frequency * 2);
    while (toggle_count--) { *pin_port ^= pin_mask; delayMicroseconds(lusDelayPerHalfCycle); }
    *pin_port &= ~(pin_mask);
#else
    tone(_pin, frequency, duration);
    delay(duration);
    noTone(_pin);
#endif
}

void MSound(uint8_t cNotes, ...) {
    va_list ap; va_start(ap, cNotes);
    while (cNotes > 0) {
        unsigned int duration = va_arg(ap, unsigned int);
        unsigned int frequency = va_arg(ap, unsigned int);
        SoundNoTimer(SOUND_PIN, duration, frequency);
        cNotes--;
    }
    va_end(ap);
}

#ifdef OPT_TERMINAL_MONITOR
bool TerminalMonitor(void) {
    static bool g_fShowDebugPrompt = true;
    uint8_t szCmdLine[5]; int ich; int ch;
    if (g_fShowDebugPrompt) {
        DBGSerial.println("Arduino Phoenix Monitor");
        DBGSerial.println("D - Toggle debug on or off");
        g_fShowDebugPrompt = false;
    }
    if (ich = DBGSerial.available()) {
        ich = 0;
        for (ich=0; ich < sizeof(szCmdLine); ich++) {
            ch = DBGSerial.read(); if ((ch == -1) || ((ch >= 10) && (ch <= 15))) break;
            szCmdLine[ich] = ch;
        }
        szCmdLine[ich] = '\0';
        if (ich == 0) g_fShowDebugPrompt = true;
        else if ((ich == 1) && ((szCmdLine[0] == 'd') || (szCmdLine[0] == 'D'))) {
            g_fDebugOutput = !g_fDebugOutput;
        }
        else if ((ich == 1) && ((szCmdLine[0] == 'p') || (szCmdLine[0] == 'P'))) {
            g_Hexapod.fShowProfile = !g_Hexapod.fShowProfile;
            if (g_Hexapod.fShowProfile) {
                DBGSerial.println("Performance Profiling: ENABLED");
                g_Hexapod.lProfileWindowStart = millis();
                g_Hexapod.lLogicTimeMax = 0;
            } else {
                DBGSerial.println("Performance Profiling: DISABLED");
            }
        }
        return true;
    }
    return false;
}
#endif

short SmoothControl (short CtrlMoveInp, short CtrlMoveOut, uint8_t CtrlDivider) {
    if (g_Hexapod.fWalking) {
        if (CtrlMoveOut < (CtrlMoveInp - 4)) return CtrlMoveOut + abs((CtrlMoveOut - CtrlMoveInp)/CtrlDivider);
        else if (CtrlMoveOut > (CtrlMoveInp + 4)) return CtrlMoveOut - abs((CtrlMoveOut - CtrlMoveInp)/CtrlDivider);
    }
    return CtrlMoveInp;
}

uint8_t g_iLegInitIndex = 0x00;
void AdjustLegPositionsToBodyHeight(void) {
#ifdef CNT_HEX_INITS
    if (g_InControlState.BodyPos.y > (short)g_abHexMaxBodyY[CNT_HEX_INITS-1])
      g_InControlState.BodyPos.y =  (short)g_abHexMaxBodyY[CNT_HEX_INITS-1];
    uint8_t i; word XZLength1;
    for(i = 0; i < CNT_HEX_INITS; i++) {
      if (g_InControlState.BodyPos.y <= (short)g_abHexMaxBodyY[i]) {
        XZLength1 = g_abHexIntXZ[i]; break;
      }
    }
    if (i != g_iLegInitIndex) { 
       g_iLegInitIndex = i;
       for (uint8_t li = 0; li <= 5; li++) {
           float angleRad = (float)((short)cCoxaAngle1[li]) * M_PI / 1800.0f;
           g_Legs[li].posX = (short)(cosf(angleRad) * XZLength1);
           g_Legs[li].posZ = (short)(-sinf(angleRad) * XZLength1);
       }
       g_InControlState.ForceGaitStepCnt = g_Hexapod.StepsInGait;
    }
#endif
}
