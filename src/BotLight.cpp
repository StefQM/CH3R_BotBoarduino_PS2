#include "BotLight.h"
#include <ctype.h>

#ifndef MOCK_HARDWARE
#include <Wire.h>
BotLightController g_BotLight;

BotLightController::BotLightController() 
    : neoStrip(NEO_TOTALCOUNT, NEO_PIN, NEO_GRB + NEO_KHZ800) {
    isInitialized = false;
    isReversed = true; // Default to reversed (bottom mounted)
    legOffset = 34;    // USER SETTING - PERSISTENT
    serialCommandIndex = 0;
    rainbowHue = 0.0f;
    circleWipeHead = 0.0f;
}

void BotLightController::init() {
    Serial.println("[BotLight] Initializing...");
    Serial.flush();
    
    // Configure I2C1 pins and start
    Wire.setSDA(SDA_PIN);
    Wire.setSCL(SCL_PIN);
    Wire.begin();
    Serial.println("[BotLight] I2C Started.");
    Serial.flush();

    neoStrip.begin();
    neoStrip.setBrightness(NEO_BRIGHTNESS);
    neoStrip.fill(0); // Explicitly clear all pixels
    neoStrip.show();
    Serial.println("[BotLight] NeoPixel Started.");
    Serial.flush();

    if (!mcp.begin_I2C(MCP_ADDR, &Wire)) {
        Serial.println("[ERROR] MCP23017 not found for BotLight!");
        Serial.flush();
    } else {
        Serial.println("[BotLight] MCP23017 found.");
        Serial.flush();
        for (int i = 0; i < BUTTONS_COUNT; i++) {
            mcp.pinMode(i, INPUT_PULLUP);
            mcp.pinMode(i + 8, OUTPUT); // Dbg LEDs
            buttonState[i] = 1;
            buttonStatePrev[i] = 1;
        }
    }

    isInitialized = true;

    useVirtualLegSensors = false;
    for (int i = 0; i < BUTTONS_COUNT; i++) {
        virtualButtonState[i] = 1; // Default to UP (unpressed)
    }

    ring.pattern = RING_OFF; 
    ring.targetBrightness = NEO_BRIGHTNESS; 
    ring.currentBrightness = (float)NEO_BRIGHTNESS;
    ring.color = 0xFFFFFF; ring.speed = 10;
    ring.frameCounter = 0;
    ring.currentFade = 1.0;
    ring.cycleLimit = 0;
    ring.cyclesDone = 0;
    ring.nextPatternOnComplete = RING_OFF;
    ring.attribute = 85;
    ring.originalColor = 0;
    
    for(int i=0; i<NEO_LEGSCOUNT; i++) { 
        legs[i].pattern = LEG_CONTACT; // Default to interactive mode
        legs[i].targetBrightness = NEO_BRIGHTNESS; 
        legs[i].currentBrightness = (float)NEO_BRIGHTNESS;
        legs[i].color = 0xFFFFFF; legs[i].speed = 10;
        legs[i].currentFade = 1.0;
        legs[i].attribute = 128;
        legs[i].originalColor = 0;
        lastSeenButtonState[i] = 1; // Default to UP
    }

    lastButtonReadTime = millis();
    lastLedUpdateTime = millis();
}

void BotLightController::update() {
    if (!isInitialized) return;
    unsigned long currentTime = millis();

    // Button Reading (20ms interval)
    if (currentTime - lastButtonReadTime >= 20) {
        getButtonState(buttonRaw);
        for (int i = 0; i < BUTTONS_COUNT; i++) {
            if (buttonRaw[i] != buttonState[i]) {
                if (buttonRaw[i] == buttonStatePrev[i]) {
                    buttonState[i] = buttonRaw[i];
                }
            }
            buttonStatePrev[i] = buttonRaw[i];
            mcp.digitalWrite(i + 8, !buttonRaw[i]);
        }
        lastButtonReadTime = currentTime;
    }

    // LED Update (25ms interval)
    if (currentTime - lastLedUpdateTime >= 25) {
        unsigned long elapsed = currentTime - lastLedUpdateTime;
        ring.currentBrightness = updateBrightness(ring.currentBrightness, ring.targetBrightness);
        
        switch(ring.pattern) {
            case RING_OFF: for(int i=NEO_LEGSCOUNT; i<NEO_TOTALCOUNT; i++) neoStrip.setPixelColor(i, 0); break;
            case RING_COLOR: setRingColor(ring.color, ring.currentBrightness); break;
            case RING_COLOR_FADING: ringColorFading(elapsed); break;
            case RING_METEORTAIL: ringCircleWipe(elapsed); break;
            case RING_RAINBOW: ringRainbow(elapsed); break;
            case RING_BREATH: ringBreath(elapsed); break;
        }

        // Auto-transition logic
        if (ring.cycleLimit > 0 && ring.cyclesDone >= ring.cycleLimit) {
            Serial.printf("[BotLight] Cycle limit reached (%u). Transitioning to: %u\n", ring.cycleLimit, ring.nextPatternOnComplete);
            Serial.flush();
            setRingPattern(ring.nextPatternOnComplete, ring.color, ring.speed, ring.targetBrightness);
        }

        for(int i=0; i<NEO_LEGSCOUNT; i++) {
            if (legs[i].pattern != LEG_OFF && legs[i].pattern != LEG_CONTACT && legs[i].pattern != LEG_CONTACT_FADING) {
                legs[i].currentBrightness = updateBrightness(legs[i].currentBrightness, legs[i].targetBrightness);
                bool isPrs = (buttonState[i] == LOW);
                switch(legs[i].pattern) {
                    case LEG_COLOR: {
                        uint8_t r = ((legs[i].color >> 16) & 0xFF) * legs[i].currentBrightness / 255;
                        uint8_t g = ((legs[i].color >> 8)  & 0xFF) * legs[i].currentBrightness / 255;
                        uint8_t b = ((legs[i].color >> 0)  & 0xFF) * legs[i].currentBrightness / 255;
                        setPixelColorCorrected(i, neoStrip.Color(r,g,b)); break;
                    }
                    case LEG_COLOR_FADING: legColorFading(i, elapsed); break;
                    case LEG_COLOR_TOUCH:
                        if (isPrs) {
                            uint8_t r = ((legs[i].color >> 16) & 0xFF) * legs[i].currentBrightness / 255;
                            uint8_t g = ((legs[i].color >> 8)  & 0xFF) * legs[i].currentBrightness / 255;
                            uint8_t b = ((legs[i].color >> 0)  & 0xFF) * legs[i].currentBrightness / 255;
                            setPixelColorCorrected(i, neoStrip.Color(r,g,b));
                        } else setPixelColorCorrected(i, 0); break;
                    case LEG_COLOR_TOUCH_FADING: {
                        if (buttonState[i] != lastSeenButtonState[i]) {
                            if (isPrs) {
                                legs[i].currentFade = 1.0f;
                            }
                            lastSeenButtonState[i] = buttonState[i];
                        }
                        
                        if (legs[i].currentFade > 0.0f) {
                            float normalizedSpeed = legs[i].speed / 255.0f;
                            float fade_rate = 0.2f + (normalizedSpeed * 4.8f);
                            legs[i].currentFade -= fade_rate * (elapsed / 1000.0f);
                            if (legs[i].currentFade < 0.0f) legs[i].currentFade = 0.0f;
                        }
                        
                        if (legs[i].currentFade > 0.0f) {
                            float brightness_multiplier = legs[i].currentFade;
                            uint8_t r = ((legs[i].color >> 16) & 0xFF) * legs[i].currentBrightness * brightness_multiplier / 255;
                            uint8_t g = ((legs[i].color >> 8)  & 0xFF) * legs[i].currentBrightness * brightness_multiplier / 255;
                            uint8_t b = ((legs[i].color >> 0)  & 0xFF) * legs[i].currentBrightness * brightness_multiplier / 255;
                            setPixelColorCorrected(i, neoStrip.Color(r,g,b));
                        } else {
                            setPixelColorCorrected(i, 0);
                        }
                        break;
                    }
                    case LEG_COLOR_FROM_RING: {
                        uint16_t ringPixelIndex = (i * (NEO_RINGCOUNT / NEO_LEGSCOUNT) + legOffset + NEO_RINGCOUNT) % NEO_RINGCOUNT;
                        uint16_t logicalIdx = ringPixelIndex;
                        uint16_t mappedIdx = isReversed ? (NEO_RINGCOUNT - 1 - logicalIdx) : logicalIdx;
                        
                        uint32_t ringColor = neoStrip.getPixelColor(mappedIdx + NEO_LEGSCOUNT);
                        uint8_t ring_r = (ringColor >> 16) & 0xFF;
                        uint8_t ring_g = (ringColor >> 8)  & 0xFF;
                        uint8_t ring_b = ringColor & 0xFF;
                        
                        uint8_t final_r = (ring_r * legs[i].currentBrightness) / 255;
                        uint8_t final_g = (ring_g * legs[i].currentBrightness) / 255;
                        uint8_t final_b = (ring_b * legs[i].currentBrightness) / 255;
                        
                        setPixelColorCorrected(i, neoStrip.Color(final_r, final_g, final_b));
                        break;
                    }
                }
            } else if (legs[i].pattern == LEG_CONTACT || legs[i].pattern == LEG_CONTACT_FADING) {
                // Determine button transitions
                bool isPrs = (buttonState[i] == LOW);
                if (buttonState[i] != lastSeenButtonState[i]) {
                    if (legs[i].pattern == LEG_CONTACT_FADING) {
                        // On release (feet up), start the fade out from 1.0f to 0.0f
                        if (!isPrs) {
                            legs[i].currentFade = 1.0f;
                        }
                    } else {
                        legs[i].currentFade = 0.0f;
                    }
                    lastSeenButtonState[i] = buttonState[i];
                }
                
                if (legs[i].pattern == LEG_CONTACT_FADING) {
                    if (legs[i].currentFade > 0.0f) {
                        float normalizedSpeed = legs[i].speed / 255.0f;
                        float fade_rate = 0.1f + (normalizedSpeed * 5.0f);
                        legs[i].currentFade -= fade_rate * (elapsed / 1000.0f);
                        if (legs[i].currentFade < 0.0f) legs[i].currentFade = 0.0f;
                    }
                } else {
                    // LEG_CONTACT standard growth logic
                    float normalizedSpeed = ring.speed / 255.0f;
                    float growth_rate = 0.5f + (normalizedSpeed * 8.0f); 
                    legs[i].currentFade += growth_rate * (elapsed / 1000.0f);
                    if (legs[i].currentFade > 1.0f) legs[i].currentFade = 1.0f;
                }

                if (!isPrs || (legs[i].pattern == LEG_CONTACT_FADING && legs[i].currentFade > 0.0f)) {
                    float fadeFactor = legs[i].currentFade;
                    
                    // Unpack contact pulse color (legs[i].color)
                    uint8_t pulse_r = (legs[i].color >> 16) & 0xFF;
                    uint8_t pulse_g = (legs[i].color >> 8)  & 0xFF;
                    uint8_t pulse_b = legs[i].color & 0xFF;
                    
                    // 1. Leg LED: fades from legs[i].color down to Black (0)
                    uint8_t leg_r = (pulse_r * fadeFactor);
                    uint8_t leg_g = (pulse_g * fadeFactor);
                    uint8_t leg_b = (pulse_b * fadeFactor);
                    setPixelColorCorrected(i, neoStrip.Color(leg_r, leg_g, leg_b));

                    // 2. Ring Expansion: blends from legs[i].color back to original background pattern!
                    uint16_t centerIdx = (i * (NEO_RINGCOUNT / NEO_LEGSCOUNT) + legOffset + NEO_RINGCOUNT) % NEO_RINGCOUNT;
                    uint16_t halfFillCount = (uint16_t)(fadeFactor * 5.0f);
                    
                    for(int16_t j = -(int16_t)halfFillCount; j <= (int16_t)halfFillCount; j++) {
                        uint16_t ringLogicalIdx = (centerIdx + j + NEO_RINGCOUNT) % NEO_RINGCOUNT;
                        uint16_t mappedIdx = isReversed ? (NEO_RINGCOUNT - 1 - ringLogicalIdx) : ringLogicalIdx;
                        
                        // Read active background color of ring pixel directly from buffer
                        uint32_t bg_color = neoStrip.getPixelColor(mappedIdx + NEO_LEGSCOUNT);
                        uint8_t bg_g = (bg_color >> 16) & 0xFF;
                        uint8_t bg_r = (bg_color >> 8)  & 0xFF;
                        uint8_t bg_b = bg_color & 0xFF;
                        
                        // Blend between contact pulse (legs[i].color) and background (bg_color)
                        uint8_t r = (pulse_r * fadeFactor) + (bg_r * (1.0f - fadeFactor));
                        uint8_t g = (pulse_g * fadeFactor) + (bg_g * (1.0f - fadeFactor));
                        uint8_t b = (pulse_b * fadeFactor) + (bg_b * (1.0f - fadeFactor));
                        
                        // Write blended color back using mapped index
                        neoStrip.setPixelColor(mappedIdx + NEO_LEGSCOUNT, neoStrip.Color(g, r, b)); // GRB strip
                    }
                } else {
                    // Feet down / Idle
                    if (legs[i].pattern == LEG_CONTACT) {
                        setPixelColorCorrected(i, 0);
                    }
                }
            } else if (ring.pattern == RING_OFF || ring.pattern == RING_COLOR) {
                setPixelColorCorrected(i, 0);
            }
        }
        
        neoStrip.show();
        lastLedUpdateTime = currentTime;
    }
}

void BotLightController::processSerialInput(char c) {
    if (!isInitialized) return;
    if (serialCommandIndex == 0) {
        if (c == '#') serialCommandBuffer[serialCommandIndex++] = c;
    } else if (serialCommandIndex < 127) {
        serialCommandBuffer[serialCommandIndex++] = c;
        if (c == '!') {
            serialCommandBuffer[serialCommandIndex] = '\0';
            Cmd parsedCmd;
            if (parseCommand(serialCommandBuffer, &parsedCmd)) {
                processCmd(parsedCmd);
            }
            serialCommandIndex = 0;
        }
    }
}

void BotLightController::setRingPattern(uint32_t pattern, uint32_t color, uint8_t speed, uint8_t brightness) {
    Serial.printf("[BotLight] Set Ring Pattern: %u, Color: 0x%06X, Speed: %u\n", pattern, color, speed);
    Serial.flush();
    ring.pattern = pattern;
    ring.color = color;
    ring.speed = speed;
    ring.targetBrightness = brightness;
    ring.currentFade = 1.0;
    ring.cycleLimit = 0;
    ring.cyclesDone = 0;
    ring.nextPatternOnComplete = RING_OFF;
}

void BotLightController::setRingPatternWithLimit(uint32_t pattern, uint16_t cycles, uint32_t nextPattern, uint32_t color, uint8_t speed, uint8_t brightness) {
    setRingPattern(pattern, color, speed, brightness);
    ring.cycleLimit = cycles;
    ring.nextPatternOnComplete = nextPattern;
}

void BotLightController::setLegPattern(uint8_t index, uint32_t pattern, uint32_t color, uint8_t speed, uint8_t brightness) {
    if (index < NEO_LEGSCOUNT) {
        legs[index].pattern = pattern;
        legs[index].color = color;
        legs[index].speed = speed;
        legs[index].targetBrightness = brightness;
        legs[index].currentFade = 1.0;
    } else if (index == 255) { // All legs
        for (int i = 0; i < NEO_LEGSCOUNT; i++) setLegPattern(i, pattern, color, speed, brightness);
    }
}

void BotLightController::setLegColor(uint8_t index, uint32_t color) {
    if (index < NEO_LEGSCOUNT) {
        legs[index].color = color;
    } else if (index == 255) {
        for (int i = 0; i < NEO_LEGSCOUNT; i++) legs[i].color = color;
    }
}

void BotLightController::setLegSpeed(uint8_t index, uint8_t speed) {
    if (index < NEO_LEGSCOUNT) {
        legs[index].speed = speed;
    } else if (index == 255) {
        for (int i = 0; i < NEO_LEGSCOUNT; i++) legs[i].speed = speed;
    }
}

void BotLightController::setLegAttribute(uint8_t index, uint8_t attribute) {
    if (index < NEO_LEGSCOUNT) {
        legs[index].attribute = attribute;
    } else if (index == 255) {
        for (int i = 0; i < NEO_LEGSCOUNT; i++) legs[i].attribute = attribute;
    }
}

void BotLightController::nextPattern() {
    uint32_t next = (ring.pattern + 1) % RING_COUNT;
    Serial.printf("[BotLight] Cycling to next pattern: %u\n", next);
    Serial.flush();
    
    // Ensure legs are in contact mode if they were OFF
    for(int i=0; i<NEO_LEGSCOUNT; i++) {
        if (legs[i].pattern == LEG_OFF) setLegPattern(i, LEG_CONTACT);
    }
    
    setRingPattern(next, ring.color, ring.speed, ring.targetBrightness);
}

// --- Internal Helpers ---

void BotLightController::getButtonState(uint8_t* stateArray) {
    if (useVirtualLegSensors) {
        for (uint8_t i = 0; i < BUTTONS_COUNT; i++) {
            stateArray[i] = virtualButtonState[i];
        }
    } else {
        for (uint8_t i = 0; i < BUTTONS_COUNT; i++) {
            stateArray[i] = mcp.digitalRead(i);
        }
    }
}

float BotLightController::updateBrightness(float current, uint8_t target, float step) {
    if (abs(current - (float)target) < step) return (float)target;
    if (current < (float)target) return current + step;
    return current - step;
}

void BotLightController::setPixelColorCorrected(uint16_t n, uint32_t c) {
    if (n < NEO_LEGSCOUNT) {
        // LEG FIX: Physical legs are RGB, but strip is initialized as GRB.
        // We must swap R and G bytes to ensure Red shows as Red.
        uint32_t r = (c >> 16) & 0xFF;
        uint32_t g = (c >> 8) & 0xFF;
        uint32_t b = c & 0xFF;
        neoStrip.setPixelColor(n, neoStrip.Color(g, r, b)); 
    } else {
        uint16_t logicalIdx = n - NEO_LEGSCOUNT;
        uint16_t mappedIdx = isReversed ? (NEO_RINGCOUNT - 1 - logicalIdx) : logicalIdx;
        neoStrip.setPixelColor(mappedIdx + NEO_LEGSCOUNT, c);
    }
}

void BotLightController::blendPixel(uint16_t n, uint32_t c) {
    uint32_t existing = neoStrip.getPixelColor(n);
    uint8_t r_e = (existing >> 16) & 0xFF, g_e = (existing >> 8) & 0xFF, b_e = existing & 0xFF;
    uint8_t r_n = (c >> 16) & 0xFF, g_n = (c >> 8) & 0xFF, b_n = c & 0xFF;
    uint8_t r = max(r_e, r_n), g = max(g_e, g_n), b = max(b_e, b_n);
    neoStrip.setPixelColor(n, neoStrip.Color(r,g,b));
}

void BotLightController::setRingColor(uint32_t rgbColor, float brightness) {
    uint8_t r = ((rgbColor >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((rgbColor >> 8)  & 0xFF) * brightness / 255;
    uint8_t b = ((rgbColor >> 0)  & 0xFF) * brightness / 255;
    uint32_t c = neoStrip.Color(r,g,b);
    for(uint16_t i = NEO_LEGSCOUNT; i < NEO_TOTALCOUNT; i++) setPixelColorCorrected(i, c);
}

uint32_t BotLightController::colorWheel(uint8_t WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) return neoStrip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    if(WheelPos < 170) {
        WheelPos -= 85;
        return neoStrip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return neoStrip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void BotLightController::ringRainbow(unsigned long elapsedTime) {
    float speed_factor = 0.1f + (ring.speed / 255.0f) * (10.0f - 0.1f);
    float hue_shift_per_ms = (speed_factor * 255.0f) / (5000.0f);
    rainbowHue += hue_shift_per_ms * elapsedTime;
    if (rainbowHue >= 256.0f) {
        rainbowHue -= 256.0f;
        ring.cyclesDone++;
    }

    for(uint16_t i=NEO_LEGSCOUNT; i<NEO_TOTALCOUNT; i++) {
        uint8_t pixelHue = (uint8_t)(rainbowHue + ((i-NEO_LEGSCOUNT) * 256 / NEO_RINGCOUNT)) % 256;
        uint32_t color = colorWheel(pixelHue);
        uint8_t r = ((color >> 16) & 0xFF) * ring.currentBrightness / 255;
        uint8_t g = ((color >> 8)  & 0xFF) * ring.currentBrightness / 255;
        uint8_t b = ((color >> 0)  & 0xFF) * ring.currentBrightness / 255;
        setPixelColorCorrected(i, neoStrip.Color(r,g,b));
    }

    for(uint16_t i=0; i<NEO_LEGSCOUNT; i++) {
        uint16_t ringLogicalIdx = (i * (NEO_RINGCOUNT / NEO_LEGSCOUNT) + legOffset + NEO_RINGCOUNT) % NEO_RINGCOUNT;
        uint8_t pixelHue = (uint8_t)(rainbowHue + (ringLogicalIdx * 256 / NEO_RINGCOUNT)) % 256;
        uint32_t color = colorWheel(pixelHue);
        uint8_t r = ((color >> 16) & 0xFF) * ring.currentBrightness / 255;
        uint8_t g = ((color >> 8)  & 0xFF) * ring.currentBrightness / 255;
        uint8_t b = ((color >> 0)  & 0xFF) * ring.currentBrightness / 255;
        setPixelColorCorrected(i, neoStrip.Color(r,g,b));
    }
}

void BotLightController::ringColorFading(unsigned long elapsedTime) {
    if (ring.currentFade > 0.0f) {
        float normalizedSpeed = ring.speed / 255.0f;
        float fade_rate = 0.2f + (normalizedSpeed * 4.8f);
        ring.currentFade -= fade_rate * (elapsedTime / 1000.0f);
        if (ring.currentFade < 0.0f) ring.currentFade = 0.0f;
    }
    
    float fadeFactor = ring.currentFade;
    uint8_t req_r = (ring.color >> 16) & 0xFF;
    uint8_t req_g = (ring.color >> 8)  & 0xFF;
    uint8_t req_b = ring.color & 0xFF;
    
    uint8_t r = req_r * fadeFactor;
    uint8_t g = req_g * fadeFactor;
    uint8_t b = req_b * fadeFactor;
    
    r = r * ring.currentBrightness / 255;
    g = g * ring.currentBrightness / 255;
    b = b * ring.currentBrightness / 255;
    
    uint32_t c = neoStrip.Color(r,g,b);
    for(uint16_t i = NEO_LEGSCOUNT; i < NEO_TOTALCOUNT; i++) setPixelColorCorrected(i, c);
}

void BotLightController::ringCircleWipe(unsigned long elapsedTime) {
    float normalizedSpeed = ring.speed / 255.0f;
    float head_move_per_second = 0.5f + (normalizedSpeed * 240.0f); 
    float oldHead = circleWipeHead;
    circleWipeHead += (head_move_per_second * elapsedTime / 1000.0f);
    
    if (circleWipeHead >= NEO_RINGCOUNT) {
        circleWipeHead -= NEO_RINGCOUNT;
        ring.cyclesDone++;
    }

    for(uint16_t i=NEO_LEGSCOUNT; i<NEO_TOTALCOUNT; i++) {
        uint32_t c = neoStrip.getPixelColor(i);
        uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        r = (r * ring.attribute) / 100; g = (g * ring.attribute) / 100; b = (b * ring.attribute) / 100;
        neoStrip.setPixelColor(i, neoStrip.Color(r,g,b));
    }

    for(int i=0; i<NEO_LEGSCOUNT; i++) {
        uint32_t c = neoStrip.getPixelColor(i);
        uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        r = (r * ring.attribute) / 100; g = (g * ring.attribute) / 100; b = (b * ring.attribute) / 100;
        setPixelColorCorrected(i, neoStrip.Color(r,g,b));
    }

    uint8_t r_head = ((ring.color >> 16) & 0xFF) * ring.currentBrightness / 255;
    uint8_t g_head = ((ring.color >> 8)  & 0xFF) * ring.currentBrightness / 255;
    uint8_t b_head = ((ring.color >> 0)  & 0xFF) * ring.currentBrightness / 255;
    uint32_t headColor = neoStrip.Color(r_head, g_head, b_head);

    uint16_t start = (uint16_t)oldHead;
    uint16_t end = (uint16_t)circleWipeHead;
    
    auto litLeg = [&](uint16_t logicalRingIdx) {
        uint16_t adjustedIdx = (logicalRingIdx - legOffset + NEO_RINGCOUNT) % NEO_RINGCOUNT;
        if (adjustedIdx % (NEO_RINGCOUNT / NEO_LEGSCOUNT) == 0) {
             uint8_t legIdx = adjustedIdx / (NEO_RINGCOUNT / NEO_LEGSCOUNT);
             if (legIdx < NEO_LEGSCOUNT) setPixelColorCorrected(legIdx, headColor);
        }
    };

    if (end >= start) {
        for (uint16_t i = start; i <= end; i++) {
            setPixelColorCorrected((i % NEO_RINGCOUNT) + NEO_LEGSCOUNT, headColor);
            litLeg(i % NEO_RINGCOUNT);
        }
    } else {
        for (uint16_t i = start; i < NEO_RINGCOUNT; i++) {
            setPixelColorCorrected(i + NEO_LEGSCOUNT, headColor);
            litLeg(i);
        }
        for (uint16_t i = 0; i <= end; i++) {
            setPixelColorCorrected(i + NEO_LEGSCOUNT, headColor);
            litLeg(i);
        }
    }
}

void BotLightController::ringBreath(unsigned long elapsedTime) {
    if (ring.speed == 0) { setRingColor(ring.color, ring.currentBrightness); return; }
    float normalizedSpeed = min((int)ring.speed, 200) / 255.0f;
    float breath_rate_per_second = 0.05f + (normalizedSpeed * normalizedSpeed) * 5.0f;
    ring.currentFade += (breath_rate_per_second * 2.0f) * (elapsedTime / 1000.0f);
    if (ring.currentFade >= 2.0f) { ring.currentFade -= 2.0f; ring.cyclesDone++; }
    float brightness_multiplier = 1.0f - abs(1.0f - ring.currentFade);
    uint8_t r = ((ring.color >> 16) & 0xFF) * ring.currentBrightness * brightness_multiplier / 255;
    uint8_t g = ((ring.color >> 8)  & 0xFF) * ring.currentBrightness * brightness_multiplier / 255;
    uint8_t b = ((ring.color >> 0)  & 0xFF) * ring.currentBrightness * brightness_multiplier / 255;
    uint32_t c = neoStrip.Color(r,g,b);
    for(uint16_t i = NEO_LEGSCOUNT; i < NEO_TOTALCOUNT; i++) setPixelColorCorrected(i, c);
    for(uint16_t i = 0; i < NEO_LEGSCOUNT; i++) setPixelColorCorrected(i, c);
}

void BotLightController::legColorFading(uint8_t legIndex, unsigned long elapsedTime) {
    if (legs[legIndex].currentFade > 0.0f) {
        float normalizedSpeed = legs[legIndex].speed / 255.0f;
        float fade_rate = 0.2f + (normalizedSpeed * 4.8f);
        legs[legIndex].currentFade -= fade_rate * (elapsedTime / 1000.0f);
        if (legs[legIndex].currentFade < 0.0f) legs[legIndex].currentFade = 0.0f;
    }
    
    float fadeFactor = legs[legIndex].currentFade;
    uint8_t req_r = (legs[legIndex].color >> 16) & 0xFF;
    uint8_t req_g = (legs[legIndex].color >> 8)  & 0xFF;
    uint8_t req_b = legs[legIndex].color & 0xFF;
    
    uint8_t r = req_r * fadeFactor;
    uint8_t g = req_g * fadeFactor;
    uint8_t b = req_b * fadeFactor;
    
    r = r * legs[legIndex].currentBrightness / 255;
    g = g * legs[legIndex].currentBrightness / 255;
    b = b * legs[legIndex].currentBrightness / 255;
    
    setPixelColorCorrected(legIndex, neoStrip.Color(r,g,b));
}

bool BotLightController::parseCommand(const char *cmdStr, Cmd *cmd) {
    if (!cmdStr || *cmdStr != '#') return false;
    cmd->type = 0; cmd->index = 0; cmd->action = 0; cmd->color = 0; cmd->brightness = 0; cmd->speed = 0;
    cmd->has_color = false; cmd->has_brightness = false; cmd->has_speed = false;
    const char *p = cmdStr + 1;
    cmd->type = toupper(*p++);
    if (cmd->type == '?') {
        return (*p == '!');
    }
    if (isdigit(*p)) {
        cmd->index = 0; while (isdigit(*p)) { cmd->index = cmd->index * 10 + (*p - '0'); p++; }
    }
    if (*p != '_') return false; p++;
    cmd->action = 0; while (isdigit(*p)) { cmd->action = cmd->action * 10 + (*p - '0'); p++; }
    while (*p != '\0' && *p != '!') {
        if (*p != '_') return false; p++;
        char key = toupper(*p); if (*(p + 1) != '=') return false; p += 2;
        if (key == 'C') {
            cmd->color = 0;
            while (isxdigit(*p)) {
                cmd->color *= 16; if (isdigit(*p)) cmd->color += (*p - '0'); else cmd->color += (toupper(*p) - 'A' + 10);
                p++;
            }
            cmd->has_color = true;
        } else if (key == 'B') {
            cmd->brightness = 0; while (isdigit(*p)) { cmd->brightness = cmd->brightness * 10 + (*p - '0'); p++; }
            cmd->has_brightness = true;
        } else if (key == 'S') {
            cmd->speed = 0; while (isdigit(*p)) { cmd->speed = cmd->speed * 10 + (*p - '0'); p++; }
            cmd->has_speed = true;
        } else if (key == 'A') {
            cmd->attribute = 0; while (isdigit(*p)) { cmd->attribute = cmd->attribute * 10 + (*p - '0'); p++; }
            cmd->has_attribute = true;
        } else while (*p != '\0' && *p != '_' && *p != '!') p++;
    }
    return (*p == '!');
}

void BotLightController::processCmd(const Cmd& cmd) {
    if (cmd.type == '?') {
        Serial.printf("{\"ring\":{\"p\":%u,\"c\":\"0x%06X\",\"b\":%u,\"t\":%u,\"s\":%u,\"a\":%u},\"legs\":[{\"p\":%u,\"c\":\"0x%06X\",\"b\":%u,\"t\":%u,\"s\":%u,\"a\":%u}],\"btns\":\"%u%u%u%u%u%u%u%u\"}\n", 
            ring.pattern, ring.color, (uint8_t)ring.currentBrightness, ring.targetBrightness, ring.speed, ring.attribute,
            legs[0].pattern, legs[0].color, (uint8_t)legs[0].currentBrightness, legs[0].targetBrightness, legs[0].speed, legs[0].attribute,
            buttonState[0]==LOW, buttonState[1]==LOW, buttonState[2]==LOW, buttonState[3]==LOW, 
            buttonState[4]==LOW, buttonState[5]==LOW, buttonState[6]==LOW, buttonState[7]==LOW);
        return;
    }
    if (cmd.type == 'R') {
        if (cmd.action == RING_COLOR_FADING && ring.pattern != RING_COLOR_FADING) {
            ring.originalColor = (ring.pattern == RING_OFF) ? 0 : ring.color;
        }
        ring.pattern = cmd.action;
        if (cmd.has_color) ring.color = cmd.color;
        if (cmd.has_brightness) ring.targetBrightness = cmd.brightness;
        if (cmd.has_speed) ring.speed = cmd.speed;
        if (cmd.has_attribute) ring.attribute = cmd.attribute;
        ring.currentFade = 1.0;
    } else if (cmd.type == 'L') {
        uint8_t start = (cmd.index == 0) ? 0 : cmd.index - 1;
        uint8_t end = (cmd.index == 0) ? NEO_LEGSCOUNT : cmd.index;
        if (end > NEO_LEGSCOUNT) end = NEO_LEGSCOUNT;
        for (uint8_t i = start; i < end; i++) {
            if (cmd.action == LEG_COLOR_FADING && legs[i].pattern != LEG_COLOR_FADING) {
                legs[i].originalColor = (legs[i].pattern == LEG_OFF) ? 0 : legs[i].color;
            }
            legs[i].pattern = cmd.action;
            if (cmd.has_color) legs[i].color = cmd.color;
            if (cmd.has_brightness) legs[i].targetBrightness = cmd.brightness;
            if (cmd.has_speed) legs[i].speed = cmd.speed;
            if (cmd.has_attribute) legs[i].attribute = cmd.attribute;
            legs[i].currentFade = 1.0;
        }
    } else if (cmd.type == 'V') {
        // Virtual Input handling
        if (cmd.index == 0) {
            useVirtualLegSensors = (cmd.action != 0);
            Serial.printf("[BotLight] Virtual leg sensors mode: %s\n", useVirtualLegSensors ? "ENABLED" : "DISABLED");
            Serial.flush();
        } else if (cmd.index <= BUTTONS_COUNT) {
            // cmd.action: 1 = pressed (LOW), 0 = released (HIGH)
            virtualButtonState[cmd.index - 1] = cmd.action ? LOW : HIGH;
            Serial.printf("[BotLight] Virtual button %d: %s\n", cmd.index, cmd.action ? "PRESSED" : "RELEASED");
            Serial.flush();
        }
    }
}
#endif // MOCK_HARDWARE
