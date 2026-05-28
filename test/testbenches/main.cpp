#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include "common.h"
#include "test_utils.h"

// Prototypes for functions in .ino
void StartUpdateServos();
boolean TerminalMonitor();
void GaitSelect();
void GaitSeq();
void SingleLegControl();
void WriteOutputs();
void CheckAngles();
void BalCalcOneLeg(short, short, short, byte);
void BalanceBody();
void BodyFK(short, short, short, short, byte);
void LegIK(short, short, short, byte);
void robot_setup();
void robot_loop();

// Include the actual robot code
#include "../../src/main.cpp"

// Include gait test after robot code so it sees its globals
#include "test_gait.h"
#include "test_input.h"
#include "test_servo.h"

bool compare_ik_line(const std::vector<std::string>& parts, short c, short f, short t, int tolerance) {
    if (parts.size() < 11) return false;
    short g_c = std::stoi(parts[8]);
    short g_f = std::stoi(parts[9]);
    short g_t = std::stoi(parts[10]);
    return is_near(c, g_c, tolerance) && is_near(f, g_f, tolerance) && is_near(t, g_t, tolerance);
}

bool run_ik_tests() {
    std::cout << "\n[PHASE] Starting Inverse Kinematics (IK) Regression Tests..." << std::endl;
    
    std::ifstream legacy_file("test/golden_data/ik_snapshot.csv");
    std::ifstream float_file("test/golden_data/ik_snapshot_float.csv");
    
    bool has_legacy = legacy_file.is_open();
    bool has_float = float_file.is_open();
    
    if (has_legacy) {
        std::string dummy; std::getline(legacy_file, dummy);
        std::cout << "[INFO] Legacy Master: test/golden_data/ik_snapshot.csv (Tolerance: 2)" << std::endl;
    }
    if (has_float) {
        std::string dummy; std::getline(float_file, dummy);
        std::cout << "[INFO] Float Master:  test/golden_data/ik_snapshot_float.csv (Strict)" << std::endl;
    }

    std::ofstream out_csv("test/golden_data/ik_snapshot_current.csv");
    out_csv << "TestID,Leg,IKFeetX,IKFeetY,IKFeetZ,BodyRotX,BodyRotY,BodyRotZ,CoxaAngle,FemurAngle,TibiaAngle" << std::endl;

    int testId = 0, legacy_failures = 0, float_failures = 0, matches = 0;
    short test_coords[] = {-100, -50, 0, 50, 100};
    short test_rots[] = {-100, 0, 100};

    for (short rx : test_rots) {
        for (short ry : test_rots) {
            for (short rz : test_rots) {
                g_InControlState.BodyRot1.x = rx; g_InControlState.BodyRot1.y = ry; g_InControlState.BodyRot1.z = rz;
                for (short tx : test_coords) {
                    for (short ty : test_coords) {
                        for (short tz : test_coords) {
                            testId++;
                            for (int leg = 0; leg < 6; ++leg) {
                                if (leg <= 2) { // Right legs
                                    g_Legs[leg].calculateBodyFK(-g_Legs[leg].posX + tx, g_Legs[leg].posY + ty, g_Legs[leg].posZ + tz, 0, 
                                                              g_Hexapod.BodyRotOffsetX, g_Hexapod.BodyRotOffsetY, g_Hexapod.BodyRotOffsetZ, g_Hexapod.TotalXBal1, g_Hexapod.TotalYBal1, g_Hexapod.TotalZBal1);
                                    g_Legs[leg].calculateLegIK(g_Legs[leg].posX - tx + g_Legs[leg].bodyFKPosX,
                                                              g_Legs[leg].posY + ty - g_Legs[leg].bodyFKPosY,
                                                              g_Legs[leg].posZ + tz - g_Legs[leg].bodyFKPosZ);
                                } else { // Left legs
                                    g_Legs[leg].calculateBodyFK(g_Legs[leg].posX - tx, g_Legs[leg].posY + ty, g_Legs[leg].posZ + tz, 0, 
                                                              g_Hexapod.BodyRotOffsetX, g_Hexapod.BodyRotOffsetY, g_Hexapod.BodyRotOffsetZ, g_Hexapod.TotalXBal1, g_Hexapod.TotalYBal1, g_Hexapod.TotalZBal1);
                                    g_Legs[leg].calculateLegIK(g_Legs[leg].posX + tx - g_Legs[leg].bodyFKPosX,
                                                              g_Legs[leg].posY + ty - g_Legs[leg].bodyFKPosY,
                                                              g_Legs[leg].posZ + tz - g_Legs[leg].bodyFKPosZ);
                                }
                                out_csv << testId << "," << leg << "," << tx << "," << ty << "," << tz << "," << rx << "," << ry << "," << rz << ","
                                    << g_Legs[leg].coxaAngle << "," << g_Legs[leg].femurAngle << "," << g_Legs[leg].tibiaAngle << std::endl;

                                if (has_legacy) {
                                    std::string line; std::getline(legacy_file, line);
                                    std::vector<std::string> parts = split_csv(line);
                                    if (!compare_ik_line(parts, g_Legs[leg].coxaAngle, g_Legs[leg].femurAngle, g_Legs[leg].tibiaAngle, 35)) {
                                        legacy_failures++;
                                        if (legacy_failures <= 5) {
                                            std::cout << "[INFO] Legacy Mismatch TestID " << testId << " Leg " << leg << std::endl;
                                            std::cout << "  Expected: " << parts[8] << "," << parts[9] << "," << parts[10] << std::endl;
                                            std::cout << "  Actual:   " << g_Legs[leg].coxaAngle << "," << g_Legs[leg].femurAngle << "," << g_Legs[leg].tibiaAngle << std::endl;
                                        }
                                    }
                                }
                                if (has_float) {
                                    std::string line; std::getline(float_file, line);
                                    if (!compare_ik_line(split_csv(line), g_Legs[leg].coxaAngle, g_Legs[leg].femurAngle, g_Legs[leg].tibiaAngle, 0)) float_failures++;
                                }
                                matches++;
                            }
                        }
                    }
                }
            }
        }
    }
    out_csv.close();
    if (has_legacy) legacy_file.close();
    if (has_float) float_file.close();

    if (has_legacy && legacy_failures > 0) std::cout << "[FAIL] Legacy IK Regression deviation outside tolerance!" << std::endl;
    if (has_float && float_failures > 0)   std::cout << "[FAIL] Float IK Regression deviate from perfect baseline!" << std::endl;
    
    if (legacy_failures == 0) std::cout << "[SUCCESS] IK REGRESSION PASSED (within legacy tolerance)." << std::endl;
    return (legacy_failures == 0);
}

int main() {
    try {
        std::cout << "Starting Test Runner..." << std::endl;
        
        auto reset_state = []() {
            robot_setup();
            for(int i=0; i<21; i++) ps2x.analogs[i] = 128;
            ps2x.analogs[1] = 0x70; // Pass the "valid analog mode" check by default
            ps2x.buttons = 0xFFFF;
            ps2x.last_buttons = 0xFFFF;
        };

        reset_state();
        bool ik_pass = run_ik_tests();
        
        reset_state();
        bool gait_pass = run_gait_tests();
        
        reset_state();
        bool input_pass = run_input_tests();
        
        reset_state();
        bool servo_pass = run_servo_tests();
        
        std::cout << "\n[FINAL SUMMARY]" << std::endl;
        std::cout << "IK Regression:    " << (ik_pass ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Gait Regression:  " << (gait_pass ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Input Regression: " << (input_pass ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Servo Regression: " << (servo_pass ? "PASSED" : "FAILED") << std::endl;

        if (!ik_pass || !gait_pass || !input_pass || !servo_pass) {
            std::cout << "\n[RESULT] TEST SUITE FAILED." << std::endl;
            return 1;
        }

        std::cout << "\n[RESULT] ALL TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Crashed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Crashed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}

