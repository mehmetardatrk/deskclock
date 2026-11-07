/*
  Arduino Uno + SSD1306 OLED (I2C, 4-pin) + DS3231 RTC
  Rotating gear animation on the left, day / time / date on the right.

  Connections (OLED and RTC share the same I2C bus):
    5V  -> VCC (OLED and RTC)
    GND -> GND (OLED and RTC)
    A4  -> SDA (OLED and RTC)
    A5  -> SCL (OLED and RTC)

  Libraries (install via Library Manager):
    - Adafruit SSD1306
    - Adafruit GFX Library
    - RTClib (Adafruit)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ---------------- Settings ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64      // Set to 32 if using a 128x32 display
#define OLED_RESET   -1
#define OLED_ADDR    0x3C     // Try 0x3D if it does not work

#define SET_TIME_NOW    0     // 1: Set RTC to compilation time (upload once, then set to 0 and upload again)
#define USE_BITMAP_ANIM 0     // 1: Use custom animation from frames.h
#define FRAME_DELAY_MS  60    // Animation speed (smaller = faster)

// Center coordinates of the animation area
#define ANIM_CX 20
#define ANIM_CY 32

#if USE_BITMAP_ANIM
  #include "frames.h"
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;               // If using DS1307: RTC_DS1307 rtc;

// Day names (ASCII characters only, 0 = Sunday)
const char* const dayNames[] = {
  "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
  "THURSDAY", "FRIDAY", "SATURDAY"
};

unsigned long lastFrameTime = 0;
uint8_t frameIndex = 0;
float angle = 0;

// ---------------- Rotating gear (procedural animation) ----------------
void drawGear(int cx, int cy, float angle) {
  const int teeth = 8;
  const int innerRadius = 12;
  const int outerRadius = 17;

  display.drawCircle(cx, cy, innerRadius, SSD1306_WHITE);
  display.fillCircle(cx, cy, 3, SSD1306_WHITE);

  for (int i = 0; i < teeth; i++) {
    float a = angle + i * 2.0 * PI / teeth;

    int x1 = cx + cos(a - 0.20) * innerRadius;
    int y1 = cy + sin(a - 0.20) * innerRadius;
    int x2 = cx + cos(a + 0.20) * innerRadius;
    int y2 = cy + sin(a + 0.20) * innerRadius;
    int x3 = cx + cos(a) * outerRadius;
    int y3 = cy + sin(a) * outerRadius;

    display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_WHITE);

    if (i % 2 == 0) {
      display.drawLine(cx, cy,
                       cx + cos(a) * (innerRadius - 2),
                       cy + sin(a) * (innerRadius - 2),
                       SSD1306_WHITE);
    }
  }
}

// ---------------- Bitmap animation (frames.h) ----------------
#if USE_BITMAP_ANIM
void drawBitmapAnim(uint8_t frame) {
  const uint8_t* ptr = (const uint8_t*)pgm_read_ptr(&frames[frame]);

  display.drawBitmap(ANIM_CX - FRAME_W / 2, ANIM_CY - FRAME_H / 2,
                     ptr, FRAME_W, FRAME_H, SSD1306_WHITE);
}
#endif

// ---------------- Helper: print two digits ----------------
void printTwoDigits(int value) {
  if (value < 10) display.print('0');
  display.print(value);
}

// ---------------- Clock and date ----------------
void drawClock(const DateTime& now) {
  // Day name
  display.setTextSize(1);
  display.setCursor(46, 4);
  display.print(dayNames[now.dayOfTheWeek()]);

  // Hour:Minute (large), colon blinks every second
  display.setTextSize(2);
  display.setCursor(46, 20);
  printTwoDigits(now.hour());
  display.print((now.second() % 2 == 0) ? ':' : ' ');
  printTwoDigits(now.minute());

  // Seconds (small)
  display.setTextSize(1);
  display.setCursor(108, 27);
  printTwoDigits(now.second());

  // Date
  display.setCursor(46, 46);
  printTwoDigits(now.day());
  display.print('.');
  printTwoDigits(now.month());
  display.print('.');
  display.print(now.year());
}

// ---------------- Error screen ----------------
void showError(const __FlashStringHelper* line1,
               const __FlashStringHelper* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.display();
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED not found! Check the wiring and OLED_ADDR value."));
    while (true);
  }

  display.setTextColor(SSD1306_WHITE);

  if (!rtc.begin()) {
    Serial.println(F("RTC not found! Check the wiring."));
    showError(F("RTC not found!"), F("Check the wiring"));
    while (true);
  }

#if SET_TIME_NOW
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#else
  if (rtc.lostPower()) {
    // If the battery is missing or depleted, set the clock to the compilation time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
#endif
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastFrameTime < FRAME_DELAY_MS) return;
  lastFrameTime = currentTime;

  DateTime now = rtc.now();

  display.clearDisplay();

#if USE_BITMAP_ANIM
  drawBitmapAnim(frameIndex);
  frameIndex = (frameIndex + 1) % FRAME_COUNT;
#else
  drawGear(ANIM_CX, ANIM_CY, angle);

  angle += 0.10;
  if (angle > 2 * PI) angle -= 2 * PI;
#endif

  // Thin vertical line between the animation and the clock
  display.drawFastVLine(41, 4, SCREEN_HEIGHT - 8, SSD1306_WHITE);

  drawClock(now);
  display.display();
}
