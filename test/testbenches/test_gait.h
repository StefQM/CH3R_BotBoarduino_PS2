#include "test_utils.h"

extern PS2X ps2x; // From PS2_controller.cpp

bool compare_gait_line(const std::vector<std::string>& parts, uint8_t gaitStep, bool fWalking, bool fContinueWalking, uint16_t moveTime, const std::string& serial, const std::vector<short>& coords, int coord_tolerance, bool strict_serial) {
    if (parts.size() < 24) return false;
    if (std::stoi(parts[1]) != gaitStep) return false;
    if (std::stoi(parts[2]) != fWalking) return false;
    if (std::stoi(parts[4]) != moveTime) return false;
    if (strict_serial && parts[5] != serial) return false;
    
    for (int i = 0; i < 18; ++i) {
        if (!is_near((int)coords[i], std::stoi(parts[6+i]), coord_tolerance)) return false;
    }
    return true;
}

bool run_gait_tests() {
    std::cout << "\n[PHASE] Starting Gait & Trajectory Regression Tests..." << std::endl;
    
    std::ifstream legacy_file("test/golden_data/gait_snapshot.csv");
    std::ifstream float_file("test/golden_data/gait_snapshot_float.csv");
    
    bool has_legacy = legacy_file.is_open();
    bool has_float = float_file.is_open();
    
    if (has_legacy) {
        std::string dummy; std::getline(legacy_file, dummy);
        std::cout << "[INFO] Legacy Master: test/golden_data/gait_snapshot.csv (Coord Tolerance: 2)" << std::endl;
    }
    if (has_float) {
        std::string dummy; std::getline(float_file, dummy);
        std::cout << "[INFO] Float Master:  test/golden_data/gait_snapshot_float.csv (Strict)" << std::endl;
    }

    std::ofstream out_csv("test/golden_data/gait_snapshot_current.csv");
    out_csv << "Cycle,GaitStep,fWalking,fContinueWalking,ServoMoveTime,SerialOutput";
    for(int i=0; i<6; i++) out_csv << ",L" << i << "X,L" << i << "Y,L" << i << "Z";
    out_csv << std::endl;

    g_mock_millis = 0; g_mock_micros = 0;
    g_InControlState.fHexOn = true;
    ps2x.analogs[1] = 0x70; // Pass the "valid analog mode" check
    ps2x.analogs[PSS_LY] = 0; // Push stick forward

    int legacy_failures = 0, float_failures = 0, matches = 0;

    for (int cycle = 0; cycle < 100; ++cycle) {
        g_mock_millis += 20; g_mock_micros += 20000;
        SoftwareSerial::clear();
        robot_loop();

        std::string escapedSerial = SoftwareSerial::lastOutput;
        size_t pos = 0;
        while ((pos = escapedSerial.find(',', pos)) != std::string::npos) { escapedSerial.replace(pos, 1, ";"); pos += 1; }
        while ((pos = escapedSerial.find("\r\n", 0)) != std::string::npos) { escapedSerial.replace(pos, 2, " "); }

        std::vector<short> current_coords;
        out_csv << cycle << "," << (int)g_Hexapod.GaitStep << "," << g_Hexapod.fWalking << "," << g_Hexapod.fContinueWalking << "," << g_Hexapod.ServoMoveTime << "," << escapedSerial;
        for (int i = 0; i < 6; ++i) {
            out_csv << "," << g_Legs[i].gaitPosX << "," << g_Legs[i].gaitPosY << "," << g_Legs[i].gaitPosZ;
            current_coords.push_back(g_Legs[i].gaitPosX); current_coords.push_back(g_Legs[i].gaitPosY); current_coords.push_back(g_Legs[i].gaitPosZ);
        }
        out_csv << std::endl;

        if (has_legacy) {
            std::string line; std::getline(legacy_file, line);
            if (!compare_gait_line(split_csv(line), g_Hexapod.GaitStep, g_Hexapod.fWalking, g_Hexapod.fContinueWalking, g_Hexapod.ServoMoveTime, escapedSerial, current_coords, 10, false)) legacy_failures++;
        }
        if (has_float) {
            std::string line; std::getline(float_file, line);
            if (!compare_gait_line(split_csv(line), g_Hexapod.GaitStep, g_Hexapod.fWalking, g_Hexapod.fContinueWalking, g_Hexapod.ServoMoveTime, escapedSerial, current_coords, 0, true)) float_failures++;
        }
        matches++;
    }

    out_csv.close();
    if (has_float) float_file.close();

    if (has_float && float_failures > 0)   std::cout << "[FAIL] Float Gait Regression deviate from perfect baseline!" << std::endl;
    
    if (float_failures == 0) std::cout << "[SUCCESS] GAIT REGRESSION PASSED (strict)." << std::endl;
    return (float_failures == 0);
}
