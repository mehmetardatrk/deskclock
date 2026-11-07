# Arduino Uno · OLED Clock and Animation (with RTC)

A desktop clock built using an Arduino Uno, a 4-pin I2C OLED display (SSD1306, 128x64), and an RTC module (DS3231). A rotating gear animation is displayed on the left side of the screen, while the time, day, and date are displayed on the right. If desired, the animation can be replaced with a custom frame-based (bitmap) animation.

## Components

| Quantity | Component |
|---|---|
| 1 | Arduino Uno |
| 1 | 0.96" OLED display, 4-pin I2C, SSD1306, 128x64 |
| 1 | RTC module DS3231 (DS1307 is also supported; see note below) |
| 1 | CR2032 battery (for the RTC module to keep time when the power is off) |
| 4-8 | Jumper wires (if using a breadboard) |

## Connections

The OLED and RTC share the same I2C bus. Their SDA and SCL pins are connected to the same Arduino pins. Since the devices have different addresses, there is no conflict (OLED `0x3C`, DS3231 `0x68`).

| Arduino Uno | OLED | RTC (DS3231) |
|---|---|---|
| 5V | VCC | VCC |
| GND | GND | GND |
| A4 (SDA) | SDA | SDA |
| A5 (SCL) | SCL | SCL |

Notes:
- On Uno R3 boards, SDA and SCL pins are also available next to AREF. They share the same connections as A4/A5, so either set can be used.
- Some OLED modules require 3.3V. Most operate between 3.3V and 5V, but check the markings on the module if you are unsure.
- If you use a breadboard, connecting 5V and GND to the power rails makes supplying power to both modules easier.

## Libraries

Install the following libraries through **Sketch > Include Library > Manage Libraries** in the Arduino IDE:

1. **Adafruit SSD1306**
2. **Adafruit GFX Library** (the IDE will ask you to install this along with SSD1306; confirm the installation)
3. **RTClib** (Adafruit)

## Code

```cpp
/*
  Arduino Uno + SSD1306 OLED (I2C) + DS3231 RTC
  Rotating gear animation on the left, time / day / date on the right.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ---------------- Settings ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64      // Set to 32 if you are using a 128x32 display
#define OLED_RESET   -1
#define OLED_ADDR    0x3C     // Try 0x3D if it does not work

#define USE_BITMAP_ANIM 0     // Set to 1 to use your custom animation in frames.h
#define SET_TIME_NOW    0     // Set to 1 and upload to adjust the RTC to the compilation time (then set it back to 0!)
#define FRAME_DELAY_MS  60    // Animation speed (smaller = faster)

// Center of the animation area (left side)
#define ANIM_CX 20
#define ANIM_CY 32

#if USE_BITMAP_ANIM
  #include "frames.h"
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;                       // If using DS1307: RTC_DS1307 rtc;

// No Turkish characters, so ASCII abbreviations are used (0 = Sunday)
const char* gunler[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};

unsigned long sonKare = 0;
uint8_t kareNo = 0;
float aci = 0;

// ---------------- Procedural animation: rotating gear ----------------
void drawGear(int cx, int cy, float angle) {
  const int dis = 8;
  const int rIc = 12;
  const int rDis = 17;

  display.drawCircle(cx, cy, rIc, SSD1306_WHITE);
  display.fillCircle(cx, cy, 3, SSD1306_WHITE);

  for (int i = 0; i < dis; i++) {
    float a = angle + i * 2.0 * PI / dis;
    int x1 = cx + cos(a - 0.20) * rIc;
    int y1 = cy + sin(a - 0.20) * rIc;
    int x2 = cx + cos(a + 0.20) * rIc;
    int y2 = cy + sin(a + 0.20) * rIc;
    int x3 = cx + cos(a) * rDis;
    int y3 = cy + sin(a) * rDis;
    display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_WHITE);

    // Inner spokes (make the rotation more visible)
    if (i % 2 == 0) {
      display.drawLine(cx, cy, cx + cos(a) * (rIc - 2), cy + sin(a) * (rIc - 2), SSD1306_WHITE);
    }
  }
}

// ---------------- Bitmap animation ----------------
#if USE_BITMAP_ANIM
void drawBitmapAnim(uint8_t frame) {
  const uint8_t* ptr = (const uint8_t*)pgm_read_ptr(&frames[frame]);
  display.drawBitmap(ANIM_CX - FRAME_W / 2, ANIM_CY - FRAME_H / 2, ptr, FRAME_W, FRAME_H, SSD1306_WHITE);
}
#endif

// ---------------- Helper: print two digits ----------------
void print2(int v) {
  if (v < 10) display.print('0');
  display.print(v);
}

// ---------------- Clock and date ----------------
void drawClock(const DateTime& now) {
  // Day name
  display.setTextSize(1);
  display.setCursor(46, 4);
  display.print(gunler[now.dayOfTheWeek()]);

  // Hour:Minute (large)
  display.setTextSize(2);
  display.setCursor(46, 20);
  print2(now.hour());
  // The colon blinks once per second
  display.print((now.second() % 2 == 0) ? ':' : ' ');
  print2(now.minute());

  // Seconds (small)
  display.setTextSize(1);
  display.setCursor(108, 27);
  print2(now.second());

  // Date
  display.setCursor(46, 46);
  print2(now.day());
  display.print('.');
  print2(now.month());
  display.print('.');
  display.print(now.year());
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED not found! Check the connections and address."));
    while (true);
  }

  if (!rtc.begin()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RTC not found!"));
    display.println(F("Check the connections"));
    display.display();
    while (true);
  }

#if SET_TIME_NOW
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#else
  if (rtc.lostPower()) {
    // If the battery is not installed or is depleted, initialize the clock using the compilation time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
#endif

  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  unsigned long simdi = millis();
  if (simdi - sonKare < FRAME_DELAY_MS) return;
  sonKare = simdi;

  DateTime now = rtc.now();

  display.clearDisplay();

#if USE_BITMAP_ANIM
  drawBitmapAnim(kareNo);
  kareNo = (kareNo + 1) % FRAME_COUNT;
#else
  drawGear(ANIM_CX, ANIM_CY, aci);
  aci += 0.10;
  if (aci > 2 * PI) aci -= 2 * PI;
#endif

  // Thin line between the animation and the clock
  display.drawFastVLine(41, 4, SCREEN_HEIGHT - 8, SSD1306_WHITE);

  drawClock(now);
  display.display();
}
```

## Setting the Time

The RTC may display an incorrect time during the initial setup.

1. Set `#define SET_TIME_NOW 1` in the code and upload it. The RTC will be adjusted to the computer's compilation time.
2. After confirming that the time is correct, set `#define SET_TIME_NOW 0` and upload the code again. Otherwise, the clock will revert to the compilation time every time the Arduino is reset.

Since there may be a difference of a few seconds between compilation and uploading, the clock may run slightly behind. For a more precise adjustment, you can quickly reset the board immediately after uploading is completed, or manually enter the time as follows:

`rtc.adjust(DateTime(2026, 11, 6, 14, 30, 0));`

## Adding Your Own Animation

Since the display is 1-bit (black and white), the animation consists of several small black-and-white images (frames) displayed sequentially. It works just like a flipbook.

### 1) Prepare the Frames

- Recommended dimensions: **32x32 pixels** (fits in the area on the left side of the display; you can increase it to approximately 40x56 at most).
- Each frame should be a separate image: `frame0.png`, `frame1.png`, `frame2.png` ... A total of 4 to 12 frames is ideal.
- Draw in black and white. [Piskel](https://www.piskelapp.com/) is very suitable for pixel art. Create a 32x32 canvas, draw the frames, and export each one separately as a PNG. Frames from a GIF can also be used.

### 2) Convert the Frames into Code

1. Open the [image2cpp](https://javl.github.io/image2cpp/) website.
2. Upload all frame PNG files at the same time.
3. Use the following settings:
   - **Canvas size:** 32 x 32 (or your custom dimensions)
   - **Background color:** Black
   - **Invert image colors:** Enable this if the output appears inverted
   - **Code output format:** Arduino code
   - **Draw mode:** Horizontal – 1 bit per pixel
4. Click **Generate code**. An array will be generated for each frame.

### 3) Create the `frames.h` File

In the Arduino IDE, open a new tab next to the `.ino` file (top-right arrow > New Tab), name it `frames.h`, and arrange the image2cpp output in the following format:

```cpp
#ifndef FRAMES_H
#define FRAMES_H

#include <avr/pgmspace.h>

#define FRAME_W 32
#define FRAME_H 32
#define FRAME_COUNT 4          // Number of frames

const unsigned char frame0[] PROGMEM = {
  // 128 bytes generated by image2cpp (for 32x32)
};
const unsigned char frame1[] PROGMEM = {
  // ...
};
const unsigned char frame2[] PROGMEM = {
  // ...
};
const unsigned char frame3[] PROGMEM = {
  // ...
};

const unsigned char* const frames[] PROGMEM = {
  frame0, frame1, frame2, frame3
};

#endif
```

### 4) Enable It in the Main Code

Change the following line in the main code:

```cpp
#define USE_BITMAP_ANIM 1
```

Adjust the `FRAME_DELAY_MS` value to control the speed. For bitmap animations, 100–150 ms generally provides a good result.

### Memory Considerations

The Arduino Uno has 2 KB of SRAM, and the display buffer alone occupies 1 KB. Since the frames are stored in flash memory using `PROGMEM`, they do not consume SRAM. A 32x32 frame requires 128 bytes, so 12 frames require approximately 1.5 KB of flash memory, which is not a problem. If you increase the frame size to 64x64, each frame will require 512 bytes. This will still fit comfortably in flash memory, but it will occupy half of the display.

### Small Tips

- To move the animation to another area of the display, change the `ANIM_CX` and `ANIM_CY` values.
- For smoother animation, keep the differences between consecutive frames small.
- Procedural animations (drawn using code) save memory. You can use the `drawGear` function as an example and write your own shape, such as a propeller, heartbeat, or orbiting dots.

## Troubleshooting

| Problem | Solution |
|---|---|
| The display does not turn on at all | VCC/GND may be reversed. Check the SDA-SCL connections. Set `OLED_ADDR` to `0x3D`. |
| "OLED not found" appears (Serial Monitor) | The address may be incorrect or a wire may be loose. Find the address using an I2C scanner sketch. |
| "RTC not found" appears | Check the RTC's SDA/SCL connections. If you are using DS1307, replace `RTC_DS3231` with `RTC_DS1307` in the code. |
| The clock resets every time the board is restarted | `SET_TIME_NOW` may still be set to 1, or the battery may be missing/depleted. |
| The display is inverted or shifted | If you are using a 128x32 display, set `SCREEN_HEIGHT` to 32. Some 1.3" displays use the SH1106 driver and require the Adafruit SH110X library. |
| The animation appears negative | Change the **Invert image colors** option in image2cpp. |
| The day name is incorrect | Set the RTC date again. The day name is calculated from the date. |

## DS1307 Note

The DS1307 is not as accurate as the DS3231 (it can drift by several minutes per month) and requires 5V. In the code, simply change the line `RTC_DS3231 rtc;` to `RTC_DS1307 rtc;`. With the DS1307, use `!rtc.isrunning()` instead of `lostPower()`.
