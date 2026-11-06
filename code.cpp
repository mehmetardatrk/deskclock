/*
  Arduino Uno + SSD1306 OLED (I2C, 4 pin) + DS3231 RTC
  Solda donen disli animasyonu, sagda gun / saat / tarih.

  Baglantilar (OLED ve RTC ayni hatta):
    5V  -> VCC (OLED ve RTC)
    GND -> GND (OLED ve RTC)
    A4  -> SDA (OLED ve RTC)
    A5  -> SCL (OLED ve RTC)

  Kutuphaneler (Library Manager'dan kur):
    - Adafruit SSD1306
    - Adafruit GFX Library
    - RTClib (Adafruit)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ---------------- Ayarlar ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64      // 128x32 ekran kullaniyorsan 32 yap
#define OLED_RESET   -1
#define OLED_ADDR    0x3C     // calismazsa 0x3D dene

#define SET_TIME_NOW    0     // 1: RTC'yi derleme saatine ayarla (bir kez yukle, sonra 0 yapip tekrar yukle)
#define USE_BITMAP_ANIM 0     // 1: frames.h icindeki kendi animasyonunu kullan
#define FRAME_DELAY_MS  60    // animasyon hizi (kucuk = hizli)

// Animasyonun cizilecegi alanin merkezi
#define ANIM_CX 20
#define ANIM_CY 32

#if USE_BITMAP_ANIM
  #include "frames.h"
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;               // DS1307 kullaniyorsan: RTC_DS1307 rtc;

// Turkce karakter yok, ASCII yaziyoruz (0 = Pazar)
const char* const gunler[] = {"PAZAR", "PZTESI", "SALI", "CARSAMBA", "PERSEMBE", "CUMA", "CMTESI"};

unsigned long sonKare = 0;
uint8_t kareNo = 0;
float aci = 0;

// ---------------- Donen disli (kodla cizilen animasyon) ----------------
void drawGear(int cx, int cy, float angle) {
  const int dis  = 8;
  const int rIc  = 12;
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

    if (i % 2 == 0) {
      display.drawLine(cx, cy,
                       cx + cos(a) * (rIc - 2),
                       cy + sin(a) * (rIc - 2),
                       SSD1306_WHITE);
    }
  }
}

// ---------------- Bitmap animasyon (frames.h) ----------------
#if USE_BITMAP_ANIM
void drawBitmapAnim(uint8_t frame) {
  const uint8_t* ptr = (const uint8_t*)pgm_read_ptr(&frames[frame]);
  display.drawBitmap(ANIM_CX - FRAME_W / 2, ANIM_CY - FRAME_H / 2,
                     ptr, FRAME_W, FRAME_H, SSD1306_WHITE);
}
#endif

// ---------------- Yardimci: 2 haneli yazdir ----------------
void print2(int v) {
  if (v < 10) display.print('0');
  display.print(v);
}

// ---------------- Saat ve tarih ----------------
void drawClock(const DateTime& now) {
  // Gun adi
  display.setTextSize(1);
  display.setCursor(46, 4);
  display.print(gunler[now.dayOfTheWeek()]);

  // Saat:Dakika (buyuk), iki nokta saniyede bir yanip soner
  display.setTextSize(2);
  display.setCursor(46, 20);
  print2(now.hour());
  display.print((now.second() % 2 == 0) ? ':' : ' ');
  print2(now.minute());

  // Saniye (kucuk)
  display.setTextSize(1);
  display.setCursor(108, 27);
  print2(now.second());

  // Tarih
  display.setCursor(46, 46);
  print2(now.day());
  display.print('.');
  print2(now.month());
  display.print('.');
  display.print(now.year());
}

// ---------------- Hata ekrani ----------------
void showError(const __FlashStringHelper* line1, const __FlashStringHelper* line2) {
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
    Serial.println(F("OLED bulunamadi! Kablolari ve OLED_ADDR degerini kontrol et."));
    while (true);
  }
  display.setTextColor(SSD1306_WHITE);

  if (!rtc.begin()) {
    Serial.println(F("RTC bulunamadi! Kablolari kontrol et."));
    showError(F("RTC bulunamadi!"), F("Kablolari kontrol et"));
    while (true);
  }

#if SET_TIME_NOW
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#else
  if (rtc.lostPower()) {
    // Pil yoksa/bittiyse saati derleme anina ayarla
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
#endif
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

  // Animasyon ile saat arasina ince dikey cizgi
  display.drawFastVLine(41, 4, SCREEN_HEIGHT - 8, SSD1306_WHITE);

  drawClock(now);
  display.display();
}
