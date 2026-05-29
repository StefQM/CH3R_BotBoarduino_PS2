#ifndef BOTLIGHT_H_MOCK
#define BOTLIGHT_H_MOCK

#include <stdint.h>

#define NEO_BRIGHTNESS 100
enum RingPattern { RING_OFF=0, RING_COLOR=1, RING_COLOR_FADING=2, RING_CIRCLEWIPE=3, RING_RAINBOW=4, RING_BREATH=5, RING_COUNT=6 };

class BotLightController {
public:
    void init() {}
    void update() {}
    void processSerialInput(char c) {}
    void setRingPattern(uint32_t pattern, uint32_t color = 0xFFFFFF, uint8_t speed = 10, uint8_t brightness = NEO_BRIGHTNESS) {}
    void setLegPattern(uint8_t index, uint32_t pattern, uint32_t color = 0xFFFFFF, uint8_t speed = 10, uint8_t brightness = NEO_BRIGHTNESS) {}
    void nextPattern() {}
};

extern BotLightController g_BotLight;

#endif // BOTLIGHT_H_MOCK
