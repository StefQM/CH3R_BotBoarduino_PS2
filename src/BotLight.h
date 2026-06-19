#ifndef BOTLIGHT_H
#define BOTLIGHT_H

#include <cstdint>
#include <cstddef>

#ifndef MOCK_HARDWARE
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#endif

#include "Hex_Cfg.h"

// --- NeoPixel Constants ---
#define NEO_BRIGHTNESS  100
#define NEO_LEGSCOUNT   6
#define NEO_RINGCOUNT   60
#define NEO_TOTALCOUNT (NEO_LEGSCOUNT+NEO_RINGCOUNT)

// --- MCP23017 Constants ---
#define MCP_ADDR        0x20
#define BUTTONS_COUNT   8

// --- Pattern Enums ---
enum LegPattern { LEG_OFF=0, LEG_COLOR=1, LEG_COLOR_FADING=2, LEG_COLOR_TOUCH=3, LEG_COLOR_TOUCH_FADING=4, LEG_COLOR_FROM_RING=5, LEG_CONTACT=6, LEG_CONTACT_FADING=7 };
enum RingPattern { RING_OFF=0, RING_COLOR=1, RING_COLOR_FADING=2, RING_METEORTAIL=3, RING_RAINBOW=4, RING_BREATH=5, RING_COUNT=6 };

// --- Animation State Structs ---
struct LegState { 
    uint32_t pattern; 
    uint32_t color; 
    uint8_t targetBrightness; 
    float currentBrightness; 
    uint8_t speed; 
    float currentFade; 
    uint8_t attribute;
    uint32_t originalColor;
};

struct RingState { 
    uint32_t pattern; 
    uint32_t color; 
    uint8_t targetBrightness; 
    float currentBrightness; 
    uint8_t speed; 
    uint16_t frameCounter; 
    float currentFade; 
    uint16_t cycleLimit;
    uint16_t cyclesDone;
    uint32_t nextPatternOnComplete;
    uint8_t attribute;
    uint32_t originalColor;
};

struct Cmd {
    uint8_t type; uint8_t index; uint32_t action; uint32_t color; uint32_t brightness; uint32_t speed; uint32_t attribute;
    bool has_color; bool has_brightness; bool has_speed; bool has_attribute;
};

class BotLightController {
public:
    BotLightController();
    void init();
    void update();
    void processSerialInput(char c);
    
    void setRingPattern(uint32_t pattern, uint32_t color = 0xFFFFFF, uint8_t speed = 10, uint8_t brightness = NEO_BRIGHTNESS);
    void setRingPatternWithLimit(uint32_t pattern, uint16_t cycles, uint32_t nextPattern, uint32_t color = 0xFFFFFF, uint8_t speed = 10, uint8_t brightness = NEO_BRIGHTNESS);
    void setLegPattern(uint8_t index, uint32_t pattern, uint32_t color = 0xFFFFFF, uint8_t speed = 10, uint8_t brightness = NEO_BRIGHTNESS);
    void nextPattern();
    
    void setRingColor(uint32_t color) { ring.color = color; }
    void setRingSpeed(uint8_t speed) { ring.speed = speed; }
    void setRingAttribute(uint8_t attribute) { ring.attribute = attribute; }
    void setLegColor(uint8_t index, uint32_t color);
    void setLegSpeed(uint8_t index, uint8_t speed);
    void setLegAttribute(uint8_t index, uint8_t attribute);
    
    uint32_t getRingPattern() const { return ring.pattern; }
    uint32_t getRingColor() const { return ring.color; }
    uint8_t getRingSpeed() const { return ring.speed; }
    uint8_t getRingAttribute() const { return ring.attribute; }
    uint8_t getRingBrightness() const { return ring.targetBrightness; }
    
    uint32_t getLegPattern(uint8_t index) const { return (index < NEO_LEGSCOUNT) ? legs[index].pattern : 0; }
    uint32_t getLegColor(uint8_t index) const { return (index < NEO_LEGSCOUNT) ? legs[index].color : 0; }
    uint8_t getLegSpeed(uint8_t index) const { return (index < NEO_LEGSCOUNT) ? legs[index].speed : 0; }
    uint8_t getLegAttribute(uint8_t index) const { return (index < NEO_LEGSCOUNT) ? legs[index].attribute : 0; }
    
    void setDirection(bool reversed) { isReversed = reversed; }
    void setLegOffset(int16_t offset) { legOffset = offset; }

private:
    bool isInitialized;
    bool isReversed;
    int16_t legOffset;
#ifndef MOCK_HARDWARE
private:
    Adafruit_NeoPixel neoStrip;
    Adafruit_MCP23X17 mcp;
#endif

private:
    LegState legs[NEO_LEGSCOUNT];
    RingState ring;

    bool useVirtualLegSensors;
    uint8_t virtualButtonState[BUTTONS_COUNT];

    uint8_t buttonState[BUTTONS_COUNT];
    uint8_t buttonStatePrev[BUTTONS_COUNT];
    uint8_t lastSeenButtonState[BUTTONS_COUNT];
    uint8_t buttonRaw[BUTTONS_COUNT];

    unsigned long lastButtonReadTime;
    unsigned long lastLedUpdateTime;
    float rainbowHue;
    float circleWipeHead;

    char serialCommandBuffer[128];
    size_t serialCommandIndex;

    void getButtonState(uint8_t* stateArray);
    float updateBrightness(float current, uint8_t target, float step = 2.0f);
    void setPixelColorCorrected(uint16_t n, uint32_t c);
    void blendPixel(uint16_t n, uint32_t c);
    void setRingColor(uint32_t rgbColor, float brightness);
    uint32_t colorWheel(uint8_t WheelPos);
    
    bool parseCommand(const char *cmdStr, Cmd *cmd);
    void processCmd(const Cmd& cmd);
    
    void ringRainbow(unsigned long elapsedTime);
    void ringColorFading(unsigned long elapsedTime);
    void ringCircleWipe(unsigned long elapsedTime);
    void ringBreath(unsigned long elapsedTime);
    void legColorFading(uint8_t legIndex, unsigned long elapsedTime);
};

extern BotLightController g_BotLight;

#endif // BOTLIGHT_H
