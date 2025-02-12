#include "display_manager.h"
#include <esp_log.h>
#include <Wire.h>

namespace one {
namespace display {

static const char* TAG = "DisplayManager";

bool DisplayManager::initialize(int sda, int scl) {
    if (initialized_) {
        return true;
    }

    Wire.begin(sda, scl);
    
    if (!display_.init()) {
        ESP_LOGE(TAG, "Display initialization failed!");
        return false;
    }

    display_.flipScreenVertically();
    display_.setFont(ArialMT_Plain_10);
    display_.setTextAlignment(TEXT_ALIGN_LEFT);
    
    initialized_ = true;
    ESP_LOGI(TAG, "Display initialized successfully");
    return true;
}

void DisplayManager::clear() {
    if (!initialized_) return;
    display_.clear();
    display_.display();
}

void DisplayManager::update() {
    if (!initialized_) return;
    display_.display();
}

void DisplayManager::drawHeader(const char* title) {
    display_.setFont(ArialMT_Plain_10);
    display_.drawString(0, 0, title);
    display_.drawHorizontalLine(0, 13, 128);
}

void DisplayManager::drawProgressBar(int progress) {
    display_.drawProgressBar(0, 32, 120, 8, progress);
}

void DisplayManager::showBootScreen(const char* version) {
    if (!initialized_) return;
    
    display_.clear();
    display_.setFont(ArialMT_Plain_16);
    display_.setTextAlignment(TEXT_ALIGN_CENTER);
    display_.drawString(64, 0, "ONE Node");
    
    display_.setFont(ArialMT_Plain_10);
    display_.drawString(64, 20, version);
    
    display_.setTextAlignment(TEXT_ALIGN_LEFT);
    display_.drawString(0, 40, "Initializing...");
    
    display_.display();
}

void DisplayManager::showStatus(const char* status) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Status");
    display_.drawString(0, 16, status);
    display_.display();
}

void DisplayManager::showError(const char* error) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Error");
    
    // Split error message into multiple lines if needed
    String errorMsg(error);
    int y = 16;
    while (errorMsg.length() > 0 && y < 54) {
        String line = errorMsg.substring(0, 20);
        int lastSpace = line.lastIndexOf(' ');
        
        if (lastSpace > 0 && errorMsg.length() > 20) {
            line = errorMsg.substring(0, lastSpace);
            errorMsg = errorMsg.substring(lastSpace + 1);
        } else {
            errorMsg = errorMsg.substring(line.length());
        }
        
        display_.drawString(0, y, line);
        y += 12;
    }
    
    display_.display();
}

void DisplayManager::showProgress(const char* operation, int progress) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Progress");
    display_.drawString(0, 16, operation);
    drawProgressBar(progress);
    display_.display();
}

void DisplayManager::showNetworkInfo(const char* ssid, const char* ip) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Network");
    
    display_.drawString(0, 16, "SSID: ");
    display_.drawString(35, 16, ssid);
    
    display_.drawString(0, 28, "IP: ");
    display_.drawString(20, 28, ip);
    
    display_.display();
}

void DisplayManager::showStorageInfo(size_t used, size_t total) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Storage");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Used: %d KB", used / 1024);
    display_.drawString(0, 16, buf);
    
    snprintf(buf, sizeof(buf), "Total: %d KB", total / 1024);
    display_.drawString(0, 28, buf);
    
    int percentage = (used * 100) / total;
    drawProgressBar(percentage);
    
    display_.display();
}

void DisplayManager::showInstanceInfo(const char* name, const char* owner) {
    if (!initialized_) return;
    
    display_.clear();
    drawHeader("Instance");
    
    display_.drawString(0, 16, "Name: ");
    display_.drawString(35, 16, name);
    
    display_.drawString(0, 28, "Owner: ");
    display_.drawString(35, 28, owner);
    
    display_.display();
}

bool DisplayManager::detectDisplay() {
    Wire.begin();
    
    ESP_LOGI(TAG, "Scanning I2C bus...");
    bool found = false;
    
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            ESP_LOGI(TAG, "I2C device found at address 0x%02X", address);
            if (address == 0x3C || address == 0x3D) {  // Common SSD1306 addresses
                ESP_LOGI(TAG, "SSD1306 OLED display detected!");
                found = true;
            }
        }
    }
    
    return found;
}

void DisplayManager::runDisplayTests() {
    if (!initialized_) return;
    
    // Run through all tests with delays between
    showTestPattern();
    delay(2000);
    
    showPixelTest();
    delay(2000);
    
    showTextTest();
    delay(2000);
    
    showAnimationTest();
}

void DisplayManager::showTestPattern() {
    if (!initialized_) return;
    
    display_.clear();
    
    // Draw border
    display_.drawRect(0, 0, 128, 64);
    
    // Draw crosshairs
    display_.drawLine(0, 32, 128, 32);    // Horizontal
    display_.drawLine(64, 0, 64, 64);     // Vertical
    
    // Draw circles
    display_.drawCircle(64, 32, 31);
    display_.drawCircle(64, 32, 20);
    display_.drawCircle(64, 32, 10);
    
    // Draw some diagonal lines
    display_.drawLine(0, 0, 128, 64);
    display_.drawLine(0, 64, 128, 0);
    
    display_.display();
}

void DisplayManager::showPixelTest() {
    if (!initialized_) return;
    
    display_.clear();
    
    // Checkerboard pattern
    for (int y = 0; y < 64; y += 2) {
        for (int x = 0; x < 128; x += 2) {
            display_.setPixel(x + (y % 4 == 0 ? 0 : 1), y);
        }
    }
    
    display_.display();
}

void DisplayManager::showTextTest() {
    if (!initialized_) return;
    
    display_.clear();
    
    // Test different fonts
    display_.setFont(ArialMT_Plain_10);
    display_.drawString(0, 0, "Arial 10px");
    
    display_.setFont(ArialMT_Plain_16);
    display_.drawString(0, 16, "Arial 16px");
    
    display_.setFont(ArialMT_Plain_24);
    display_.drawString(0, 32, "24px");
    
    // Test alignments
    display_.setFont(ArialMT_Plain_10);
    display_.setTextAlignment(TEXT_ALIGN_LEFT);
    display_.drawString(0, 54, "Left");
    
    display_.setTextAlignment(TEXT_ALIGN_CENTER);
    display_.drawString(64, 54, "Center");
    
    display_.setTextAlignment(TEXT_ALIGN_RIGHT);
    display_.drawString(128, 54, "Right");
    
    display_.display();
}

void DisplayManager::showAnimationTest() {
    if (!initialized_) return;
    
    // Simple bouncing ball animation
    int x = 64, y = 32;
    int dx = 2, dy = 1;
    int radius = 5;
    
    for (int i = 0; i < 100; i++) {  // Run animation for 100 frames
        display_.clear();
        
        // Update ball position
        x += dx;
        y += dy;
        
        // Bounce off edges
        if (x <= radius || x >= 128-radius) dx = -dx;
        if (y <= radius || y >= 64-radius) dy = -dy;
        
        // Draw the ball
        display_.fillCircle(x, y, radius);
        
        display_.display();
        delay(50);  // 20 fps
    }
}

void DisplayManager::drawCheckerboard() {
    for (int y = 0; y < 64; y += 8) {
        for (int x = 0; x < 128; x += 8) {
            if ((x + y) % 16 == 0) {
                display_.fillRect(x, y, 8, 8);
            }
        }
    }
}

void DisplayManager::drawDiagonalLines() {
    for (int i = 0; i < 128 + 64; i += 8) {
        display_.drawLine(0, i, i, 0);
        display_.drawLine(0, i-64, i, 64);
    }
}

void DisplayManager::drawCirclePattern() {
    for (int r = 4; r <= 32; r += 4) {
        display_.drawCircle(64, 32, r);
    }
}

void DisplayManager::showScrollingText(const char* text) {
    if (!initialized_) return;
    
    display_.clear();
    display_.setFont(ArialMT_Plain_16);
    display_.setTextAlignment(TEXT_ALIGN_LEFT);
    
    int textWidth = display_.getStringWidth(text);
    
    // Scroll text from right to left
    for (int x = 128; x > -textWidth; x -= 2) {
        display_.clear();
        display_.drawString(x, 24, text);
        display_.display();
        delay(20);
    }
}

void DisplayManager::showContrastTest() {
    if (!initialized_) return;
    
    display_.clear();
    display_.setFont(ArialMT_Plain_16);
    display_.setTextAlignment(TEXT_ALIGN_CENTER);
    display_.drawString(64, 24, "Contrast Test");
    
    // Fade out
    for (int contrast = 255; contrast >= 0; contrast -= 5) {
        display_.setContrast(contrast);
        delay(20);
    }
    
    // Fade in
    for (int contrast = 0; contrast <= 255; contrast += 5) {
        display_.setContrast(contrast);
        delay(20);
    }
    
    display_.setContrast(255);  // Reset to full contrast
}

void DisplayManager::showInverseTest() {
    if (!initialized_) return;
    
    for (int i = 0; i < 4; i++) {
        display_.clear();
        display_.setFont(ArialMT_Plain_16);
        display_.setTextAlignment(TEXT_ALIGN_CENTER);
        display_.drawString(64, 24, "Inverse Test");
        display_.normalDisplay();
        display_.display();
        delay(1000);
        
        display_.invertDisplay();
        delay(1000);
    }
    
    display_.normalDisplay();
}

void DisplayManager::showWaveAnimation() {
    if (!initialized_) return;
    
    for (int offset = 0; offset < 360; offset += 5) {
        display_.clear();
        drawSineWave(offset);
        drawSineWave(offset + 90, 15);  // Second wave with different amplitude
        display_.display();
        delay(50);
    }
}

void DisplayManager::drawSineWave(int offset, int amplitude) {
    const float frequency = 2.0;  // Number of waves across screen
    int lastY = -1;
    
    for (int x = 0; x < 128; x++) {
        float angle = (x * frequency * 2 * PI / 128.0) + (offset * PI / 180.0);
        int y = 32 + (sin(angle) * amplitude);
        
        if (lastY != -1) {
            display_.drawLine(x-1, lastY, x, y);
        }
        lastY = y;
    }
}

void DisplayManager::showSpirograph() {
    if (!initialized_) return;
    
    display_.clear();
    
    // R is the radius of the fixed circle
    // r is the radius of the moving circle
    // p is the offset of the drawing point
    drawSpirographPattern(30, 15, 10);
    display_.display();
}

void DisplayManager::drawSpirographPattern(int R, int r, int p) {
    float t = 0;
    int lastX = -1, lastY = -1;
    
    while (t < 2 * PI * r) {
        int x = 64 + ((R-r) * cos(t) + p * cos(((R-r)/r) * t));
        int y = 32 + ((R-r) * sin(t) - p * sin(((R-r)/r) * t));
        
        if (lastX != -1 && lastY != -1) {
            display_.drawLine(lastX, lastY, x, y);
        }
        
        lastX = x;
        lastY = y;
        display_.display();  // Update more frequently for animation effect
        t += 0.1;
    }
}

void DisplayManager::showStarfield() {
    if (!initialized_) return;
    
    const int NUM_STARS = 50;
    struct Star {
        float x, y, z;
    } stars[NUM_STARS];
    
    // Initialize stars with random positions
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = random(-100, 100);
        stars[i].y = random(-100, 100);
        stars[i].z = random(1, 100);
    }
    
    // Animate stars
    for (int frame = 0; frame < 200; frame++) {
        display_.clear();
        
        for (int i = 0; i < NUM_STARS; i++) {
            // Project 3D position to 2D screen
            int x = (stars[i].x * 100) / stars[i].z + 64;
            int y = (stars[i].y * 100) / stars[i].z + 32;
            
            // Calculate brightness based on z position
            int brightness = map(stars[i].z, 1, 100, 3, 1);
            
            if (x >= 0 && x < 128 && y >= 0 && y < 64) {
                drawStar(x, y, brightness);
            }
            
            // Move star closer
            stars[i].z -= 1;
            
            // Reset star if it's too close
            if (stars[i].z < 1) {
                stars[i].x = random(-100, 100);
                stars[i].y = random(-100, 100);
                stars[i].z = 100;
            }
        }
        
        display_.display();
        delay(20);
    }
}

void DisplayManager::drawStar(int x, int y, int brightness) {
    for (int i = 0; i < brightness; i++) {
        display_.setPixel(x, y);
        if (i > 0) {
            display_.setPixel(x+1, y);
            display_.setPixel(x-1, y);
            display_.setPixel(x, y+1);
            display_.setPixel(x, y-1);
        }
    }
}

void DisplayManager::showBouncePattern() {
    if (!initialized_) return;
    
    const int NUM_BALLS = 5;
    struct Ball {
        int x, y, dx, dy, radius;
    } balls[NUM_BALLS];
    
    // Initialize balls with random positions and velocities
    for (int i = 0; i < NUM_BALLS; i++) {
        balls[i].x = random(10, 118);
        balls[i].y = random(10, 54);
        balls[i].dx = random(2, 5) * (random(2) ? 1 : -1);
        balls[i].dy = random(2, 5) * (random(2) ? 1 : -1);
        balls[i].radius = random(2, 6);
    }
    
    // Animate for 200 frames
    for (int frame = 0; frame < 200; frame++) {
        display_.clear();
        
        for (int i = 0; i < NUM_BALLS; i++) {
            // Update position
            balls[i].x += balls[i].dx;
            balls[i].y += balls[i].dy;
            
            // Bounce off edges
            if (balls[i].x <= balls[i].radius || balls[i].x >= 128-balls[i].radius) {
                balls[i].dx = -balls[i].dx;
            }
            if (balls[i].y <= balls[i].radius || balls[i].y >= 64-balls[i].radius) {
                balls[i].dy = -balls[i].dy;
            }
            
            // Draw ball
            display_.fillCircle(balls[i].x, balls[i].y, balls[i].radius);
        }
        
        display_.display();
        delay(20);
    }
}

void DisplayManager::showDisplayInfo() {
    if (!initialized_) return;
    
    display_.clear();
    display_.setFont(ArialMT_Plain_10);
    display_.setTextAlignment(TEXT_ALIGN_LEFT);
    
    display_.drawString(0, 0, "Display Info:");
    display_.drawString(0, 12, "Type: SSD1306");
    display_.drawString(0, 22, "Resolution: 128x64");
    display_.drawString(0, 32, "I2C Addr: 0x3C");
    display_.drawString(0, 42, "Monochrome OLED");
    display_.drawString(0, 52, "Buffer: 1024 bytes");
    
    display_.display();
}

void DisplayManager::showMatrixEffect() {
    display_.clear();
    static uint8_t drops[DISPLAY_WIDTH] = {0};
    static char chars[DISPLAY_WIDTH];
    
    // Update and draw drops
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        if (drops[x] == 0 && random(20) == 0) {
            drops[x] = 1;
            chars[x] = random(33, 126); // Random ASCII character
        }
        if (drops[x] > 0) {
            display_.setColor(WHITE);
            display_.drawString(x, (drops[x]-1)*8, String(chars[x]));
            if (++drops[x] >= DISPLAY_HEIGHT/8) drops[x] = 0;
        }
    }
    display_.display();
    delay(50);
}

void DisplayManager::showFireEffect() {
    display_.clear();
    static uint8_t heat[DISPLAY_WIDTH * DISPLAY_HEIGHT] = {0};
    
    // Add heat at bottom
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        heat[x + (DISPLAY_HEIGHT-1)*DISPLAY_WIDTH] = random(160, 255);
    }
    
    // Propagate fire upwards
    for (int y = 0; y < DISPLAY_HEIGHT-1; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int idx = x + y * DISPLAY_WIDTH;
            heat[idx] = (heat[idx+DISPLAY_WIDTH] + 
                        heat[(x+1)%DISPLAY_WIDTH + (y+1)*DISPLAY_WIDTH] + 
                        heat[(x-1+DISPLAY_WIDTH)%DISPLAY_WIDTH + (y+1)*DISPLAY_WIDTH]) / 3;
            if (heat[idx] > 0) heat[idx]--;
            
            // Draw pixel with intensity based on heat
            if (heat[idx] > 0) {
                display_.setColor(WHITE);
                display_.setPixel(x, y);
            }
        }
    }
    display_.display();
    delay(30);
}

void DisplayManager::showParticleExplosion() {
    const int NUM_PARTICLES = 50;
    static bool initialized = false;
    
    if (!initialized) {
        particleX.resize(NUM_PARTICLES);
        particleY.resize(NUM_PARTICLES);
        particleVX.resize(NUM_PARTICLES);
        particleVY.resize(NUM_PARTICLES);
        
        // Initialize particles at center
        for (int i = 0; i < NUM_PARTICLES; i++) {
            particleX[i] = DISPLAY_WIDTH/2;
            particleY[i] = DISPLAY_HEIGHT/2;
            float angle = random(360) * 3.14159 / 180.0;
            float speed = random(1, 5);
            particleVX[i] = cos(angle) * speed;
            particleVY[i] = sin(angle) * speed;
        }
        initialized = true;
    }
    
    display_.clear();
    
    // Update and draw particles
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particleX[i] += particleVX[i];
        particleY[i] += particleVY[i];
        particleVY[i] += 0.1; // Gravity
        
        if (particleX[i] >= 0 && particleX[i] < DISPLAY_WIDTH &&
            particleY[i] >= 0 && particleY[i] < DISPLAY_HEIGHT) {
            display_.setColor(WHITE);
            display_.setPixel(particleX[i], particleY[i]);
        }
    }
    
    display_.display();
    delay(20);
}

void DisplayManager::showVortex() {
    static float angle = 0;
    display_.clear();
    
    const int centerX = DISPLAY_WIDTH/2;
    const int centerY = DISPLAY_HEIGHT/2;
    const int maxRadius = min(DISPLAY_WIDTH, DISPLAY_HEIGHT)/2;
    
    for (float r = 0; r < maxRadius; r += 0.5) {
        float x = centerX + cos(angle + r/5) * r;
        float y = centerY + sin(angle + r/5) * r;
        if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
            display_.setColor(WHITE);
            display_.setPixel(x, y);
        }
    }
    
    angle += 0.1;
    display_.display();
    delay(20);
}

void DisplayManager::showHeartbeat() {
    static float phase = 0;
    static const int heartPoints[][2] = {
        {0,-2}, {-1,-1}, {-2,0}, {-1,1}, {0,2}, {1,1}, {2,0}, {1,-1}
    };
    
    display_.clear();
    float scale = 2 + sin(phase) * 1.5; // Pulsing effect
    
    for (auto& point : heartPoints) {
        int x = DISPLAY_WIDTH/2 + point[0] * scale;
        int y = DISPLAY_HEIGHT/2 + point[1] * scale;
        if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
            display_.setColor(WHITE);
            display_.fillCircle(x, y, 2);
        }
    }
    
    phase += 0.2;
    display_.display();
    delay(30);
}

void DisplayManager::showStaticEffect() {
    display_.clear();
    
    // Fill screen with random noise
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (random(2) == 0) {
                display_.setColor(WHITE);
                display_.setPixel(x, y);
            }
        }
    }
    
    display_.display();
    delay(50);
}

} // namespace display
} // namespace one 