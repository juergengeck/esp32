#pragma once

#include <SSD1306Wire.h>
#include <Wire.h>
#include <vector>
#include "display_constants.h"

namespace one {
namespace display {

class DisplayManager {
public:
    static DisplayManager& getInstance() {
        static DisplayManager instance;
        return instance;
    }

    bool initialize(int sda = 21, int scl = 22);  // Default ESP32 I2C pins
    void clear();
    void update();
    
    // Status display methods
    void showBootScreen(const char* version);
    void showStatus(const char* status);
    void showError(const char* error);
    void showProgress(const char* operation, int progress);
    
    // Info display methods
    void showNetworkInfo(const char* ssid, const char* ip);
    void showStorageInfo(size_t used, size_t total);
    void showInstanceInfo(const char* name, const char* owner);

    // Display test methods
    void runDisplayTests();  // Run all display tests
    void showTestPattern();  // Show geometric test pattern
    void showPixelTest();    // Test individual pixels
    void showTextTest();     // Test different fonts and alignments
    void showAnimationTest(); // Simple animation demo
    bool detectDisplay();    // Scan I2C bus and detect display
    
    // New display test methods
    void showScrollingText(const char* text);  // Scroll text across screen
    void showContrastTest();                   // Fade display in and out
    void showInverseTest();                    // Invert display colors
    void showWaveAnimation();                  // Animated sine wave
    void showSpirograph();                     // Draw spirograph pattern
    void showStarfield();                      // Moving star field animation
    void showBouncePattern();                  // Multiple bouncing objects
    void showDisplayInfo();                    // Show display capabilities

    // Add dramatic display effects
    void showMatrixEffect();         // Matrix-style falling characters
    void showFireEffect();           // Animated fire effect
    void showPlasmaEffect();         // Animated plasma waves
    void showParticleExplosion();    // Particle explosion animation
    void showVortex();               // Spinning vortex animation
    void showRainEffect();           // Matrix-style rain drops
    void showSparkles();             // Random sparkles across screen
    void showHeartbeat();            // Pulsing heartbeat animation
    void showScanline();             // Moving scanline effect
    void showStaticEffect();         // TV static noise effect

private:
    DisplayManager() : display_(0x3c, SDA, SCL) {}  // 0x3c is default I2C address
    ~DisplayManager() = default;
    
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    void drawHeader(const char* title);
    void drawProgressBar(int progress);
    void drawCheckerboard();
    void drawDiagonalLines();
    void drawCirclePattern();
    
    // Helper methods for new animations
    void drawSineWave(int offset, int amplitude = 20);
    void drawStar(int x, int y, int brightness);
    void drawSpirographPattern(int R, int r, int p);

    // Helper methods for effects
    void drawParticle(int x, int y, int brightness);
    void drawFire(int x, int y, int heat);
    void drawPlasma(int x, int y, int phase);
    void updateParticles();
    void updateMatrix();
    
    // Effect state variables
    std::vector<int> particleX, particleY, particleVX, particleVY;
    std::vector<uint8_t> matrixColumns;
    uint8_t plasmaPhase = 0;
    uint8_t* fireBase = nullptr;  // Will be allocated in cpp
    int scanlinePos = 0;

    SSD1306Wire display_;
    bool initialized_ = false;
};

} // namespace display
} // namespace one 