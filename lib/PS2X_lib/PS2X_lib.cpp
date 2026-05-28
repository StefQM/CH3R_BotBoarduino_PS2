#include "PS2X_lib.h"
#include <SPI.h>

// PS2 Protocol is strictly Mode 3, LSBFIRST. 
// Speed: 250kHz is the safest maximum for wireless receivers.
SPISettings ps2Settings(250000, LSBFIRST, SPI_MODE3);

uint8_t PS2X::config_gamepad(uint8_t clk, uint8_t cmd, uint8_t att, uint8_t dat, bool pressures, bool rumble) {
    _clk = clk; _cmd = cmd; _att = att; _dat = dat;
    
    pinMode(_att, OUTPUT);
    digitalWrite(_att, HIGH);
    
    // Initialize SPI1
    SPI.begin();
    
    Serial.println("[PS2] Hardware SPI Initialized.");
    delay(500);
    
    for (int retry = 0; retry < 10; retry++) {
        // 1. Enter Config Mode
        SPI.beginTransaction(ps2Settings);
        digitalWrite(_att, LOW);
        delayMicroseconds(20);
        _shiftinout(0x01);
        _shiftinout(0x43); 
        _shiftinout(0x00);
        _shiftinout(0x01);
        _shiftinout(0x00);
        digitalWrite(_att, HIGH);
        SPI.endTransaction();
        delay(20);
        
        // 2. Set to Analog Mode (Lock it)
        SPI.beginTransaction(ps2Settings);
        digitalWrite(_att, LOW);
        delayMicroseconds(20);
        _shiftinout(0x01);
        _shiftinout(0x44);
        _shiftinout(0x00);
        _shiftinout(0x01); // 0x01 = Analog
        _shiftinout(0x03); // 0x03 = Lock
        _shiftinout(0x00);
        _shiftinout(0x00);
        _shiftinout(0x00);
        _shiftinout(0x00);
        digitalWrite(_att, HIGH);
        SPI.endTransaction();
        delay(20);

        // 3. Exit Config Mode
        SPI.beginTransaction(ps2Settings);
        digitalWrite(_att, LOW);
        delayMicroseconds(20);
        _shiftinout(0x01);
        _shiftinout(0x43);
        _shiftinout(0x00);
        _shiftinout(0x00);
        _shiftinout(0x5A);
        _shiftinout(0x5A);
        _shiftinout(0x5A);
        _shiftinout(0x5A);
        _shiftinout(0x5A);
        digitalWrite(_att, HIGH);
        SPI.endTransaction();
        delay(50);
        
        read_gamepad();
        
        if ((_analog[1] & 0xF0) == 0x70) {
            Serial.print("[PS2] HW SPI SUCCESS on attempt ");
            Serial.println(retry + 1);
            return 0;
        }
        
        Serial.print("[PS2] HW SPI Attempt ");
        Serial.print(retry + 1);
        Serial.print(" failed. ID: 0x");
        Serial.println(_analog[1], HEX);
        delay(100);
    }

    return 1;
}

uint8_t PS2X::_shiftinout(uint8_t byte) {
    uint8_t res = SPI.transfer(byte);
    delayMicroseconds(20); // Critical: PS2 receivers need time to process each byte
    return res;
}

bool PS2X::read_gamepad(bool motor1, uint8_t motor2) {
    _last_buttons = _buttons;
    
    SPI.beginTransaction(ps2Settings);
    digitalWrite(_att, LOW);
    delayMicroseconds(20);
    
    _shiftinout(0x01); // Start
    uint8_t id = _shiftinout(0x42); // Read data
    _shiftinout(0x00); // Dummy
    
    uint8_t b1 = _shiftinout(0x00);
    uint8_t b2 = _shiftinout(0x00);
    _buttons = (b2 << 8) | b1;
    
    _analog[PSS_RX] = _shiftinout(0x00);
    _analog[PSS_RY] = _shiftinout(0x00);
    _analog[PSS_LX] = _shiftinout(0x00);
    _analog[PSS_LY] = _shiftinout(0x00);
    
    _analog[1] = id; 
    
    digitalWrite(_att, HIGH);
    SPI.endTransaction();
    
    return true;
}

bool PS2X::Button(uint16_t button) {
    return ~(_buttons) & button;
}

bool PS2X::ButtonPressed(uint16_t button) {
    return NewButtonState(button) && Button(button);
}

bool PS2X::ButtonReleased(uint16_t button) {
    return NewButtonState(button) && !Button(button);
}

bool PS2X::NewButtonState() {
    return (_buttons ^ _last_buttons) != 0;
}

bool PS2X::NewButtonState(uint16_t button) {
    return (_buttons ^ _last_buttons) & button;
}

uint8_t PS2X::Analog(uint8_t button) {
    return _analog[button];
}

void PS2X::reconfig_gamepad() {
    config_gamepad(_clk, _cmd, _att, _dat);
}
