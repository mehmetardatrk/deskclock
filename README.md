# Arduino Uno · OLED Saat ve Animasyon (RTC'li)

Arduino Uno, 4 pinli I2C OLED ekran (SSD1306, 128x64) ve RTC modülü (DS3231) ile yapılmış bir masa saati. Ekranın solunda dönen bir dişli animasyonu, sağında saat, gün ve tarih gösterilir. Animasyon istersen kendi çizdiğin kareli (bitmap) animasyonla değiştirilebilir.

## Malzemeler

| Adet | Malzeme |
|------|---------|
| 1 | Arduino Uno |
| 1 | 0.96" OLED ekran, 4 pinli I2C, SSD1306, 128x64 |
| 1 | RTC modülü DS3231 (DS1307 de olur, aşağıda notu var) |
| 1 | CR2032 pil (RTC modülü için, saatin kapalıyken de çalışması için) |
| 4-8 | Jumper kablo (breadboard kullanılırsa) |

## Bağlantılar

OLED ve RTC aynı I2C hattını paylaşır. İkisinin SDA ve SCL uçları Arduino'da aynı pinlere bağlanır. Cihazların adresleri farklı olduğu için çakışma olmaz (OLED `0x3C`, DS3231 `0x68`).

| Arduino Uno | OLED | RTC (DS3231) |
|-------------|------|--------------|
| 5V | VCC | VCC |
| GND | GND | GND |
| A4 (SDA) | SDA | SDA |
| A5 (SCL) | SCL | SCL |

Notlar:
- Uno R3 kartlarda SDA ve SCL pinleri AREF'in yanında da ayrıca çıkar. A4/A5 ile aynı hattır, hangisini kullanırsan kullan.
- Bazı OLED modüller 3.3V ister. Çoğu 3.3V-5V arası çalışır, ama emin değilsen modülün üstündeki yazıya bak.
- Breadboard kullanırsan 5V ve GND'yi güç raylarına alıp ikisini oradan beslemek işi kolaylaştırır.

```
        Arduino Uno
      +-------------+
      |          5V |-----+--------+
      |         GND |--+--|--------|--+
      |          A4 |--|--|--+-----|--|--+
      |          A5 |--|--|--|--+--|--|--|--+
      +-------------+  |  |  |  |  |  |  |  |
                       |  |  |  |  |  |  |  |
                     GND VCC SDA SCL VCC GND SDA SCL
                      [  OLED     ]  [   RTC    ]
```

## Kütüphaneler

Arduino IDE içinde **Sketch > Include Library > Manage Libraries** menüsünden şunları kur:

1. **Adafruit SSD1306**
2. **Adafruit GFX Library** (SSD1306 ile birlikte kurulması istenir, onayla)
3. **RTClib** (Adafruit)

## Kod

```cpp
/*
  Arduino Uno + SSD1306 OLED (I2C) + DS3231 RTC
  Solda donen disli animasyonu, sagda saat / gun / tarih.
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

#define USE_BITMAP_ANIM 0     // 1 yaparsan frames.h icindeki kendi animasyonun calisir
#define SET_TIME_NOW    0     // 1 yapip yuklersen RTC'yi derleme saatine ayarlar (sonra tekrar 0 yap!)
#define FRAME_DELAY_MS  60    // animasyon hizi (kucuk = hizli)

// Animasyonun cizilecegi alanin merkezi (solda)
#define ANIM_CX 20
#define ANIM_CY 32

#if USE_BITMAP_ANIM
  #include "frames.h"
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;                       // DS1307 kullaniyorsan: RTC_DS1307 rtc;

// Turkce karakter yok, bu yuzden ASCII kisaltmalar (0 = Pazar)
const char* gunler[] = {"PAZAR", "PZTESI", "SALI", "CARSAMBA", "PERSEMBE", "CUMA", "CMTESI"};

unsigned long sonKare = 0;
uint8_t kareNo = 0;
float aci = 0;

// ---------------- Prosedurel animasyon: donen disli ----------------
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

    // ic spoklar (donusu daha belirgin gosterir)
    if (i % 2 == 0) {
      display.drawLine(cx, cy, cx + cos(a) * (rIc - 2), cy + sin(a) * (rIc - 2), SSD1306_WHITE);
    }
  }
}

// ---------------- Bitmap animasyon ----------------
#if USE_BITMAP_ANIM
void drawBitmapAnim(uint8_t frame) {
  const uint8_t* ptr = (const uint8_t*)pgm_read_ptr(&frames[frame]);
  display.drawBitmap(ANIM_CX - FRAME_W / 2, ANIM_CY - FRAME_H / 2, ptr, FRAME_W, FRAME_H, SSD1306_WHITE);
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

  // Saat:Dakika (buyuk)
  display.setTextSize(2);
  display.setCursor(46, 20);
  print2(now.hour());
  // Iki nokta saniyede bir yanip soner
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

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED bulunamadi! Baglantilari ve adresi kontrol et."));
    while (true);
  }

  if (!rtc.begin()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RTC bulunamadi!"));
    display.println(F("Baglantilari kontrol et"));
    display.display();
    while (true);
  }

#if SET_TIME_NOW
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#else
  if (rtc.lostPower()) {
    // Pil takili degilse veya bittiyse saati derleme aniyla baslat
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

  // Animasyon ile saat arasina ince cizgi
  display.drawFastVLine(41, 4, SCREEN_HEIGHT - 8, SSD1306_WHITE);

  drawClock(now);
  display.display();
}
```

## Saati ayarlama

RTC'nin saati ilk kurulumda yanlış olabilir.

1. Kodda `#define SET_TIME_NOW 1` yap ve yükle. RTC, derleme anındaki bilgisayar saatine ayarlanır.
2. Saatin doğru olduğunu gördükten sonra `#define SET_TIME_NOW 0` yap ve tekrar yükle. Aksi halde Arduino her resetlendiğinde saat derleme anına geri döner.

Derleme ile yükleme arasında birkaç saniye fark olacağı için saat biraz geride kalabilir. Daha hassas ayar için yükleme tamamlanır tamamlanmaz hızlıca reset atabilir veya `rtc.adjust(DateTime(2026, 11, 6, 14, 30, 0));` şeklinde elle yazabilirsin.

## Kendi animasyonunu ekleme

Ekran 1 bit (siyah-beyaz) olduğu için animasyon, sırayla gösterilen birkaç siyah-beyaz küçük resimden (kare) oluşur. Tıpkı flipbook gibi.

### 1) Kareleri hazırla

- Önerilen boyut: **32x32 piksel** (ekranın solundaki alana sığar, en fazla yaklaşık 40x56'ya kadar çıkabilirsin).
- Her kare ayrı bir görsel olmalı: `kare0.png`, `kare1.png`, `kare2.png` ... Kare sayısı 4 ile 12 arası idealdir.
- Siyah-beyaz çiz. Piksel sanat için [Piskel](https://www.piskelapp.com/) çok uygun. 32x32 tuval açıp kareleri çiz, sonra PNG olarak her kareyi ayrı dışa aktar. GIF'in kareleri de olur.

### 2) Kareleri koda çevir

1. [image2cpp](https://javl.github.io/image2cpp/) sitesini aç.
2. Tüm kare PNG'lerini aynı anda yükle.
3. Ayarlar:
   - **Canvas size:** 32 x 32 (kendi boyutun)
   - **Background color:** Black
   - **Invert image colors:** Çıktı ters görünürse işaretle
   - **Code output format:** Arduino code
   - **Draw mode:** Horizontal – 1 bit per pixel
4. **Generate code** de. Her kare için bir dizi üretilir.

### 3) `frames.h` dosyasını oluştur

Arduino IDE'de `.ino` dosyasının yanına yeni sekme aç (sağ üstteki ok > New Tab), adını `frames.h` yap ve image2cpp çıktısını şu düzene sok:

```cpp
#ifndef FRAMES_H
#define FRAMES_H

#include <avr/pgmspace.h>

#define FRAME_W 32
#define FRAME_H 32
#define FRAME_COUNT 4          // kare sayin

const unsigned char frame0[] PROGMEM = {
  // image2cpp'den gelen 128 bayt (32x32 icin)
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

### 4) Ana kodda aç

Ana koddaki satırı değiştir:

```cpp
#define USE_BITMAP_ANIM 1
```

Hız için `FRAME_DELAY_MS` değerini ayarla. Bitmap animasyonlarda genelde 100-150 ms güzel görünür.

### Bellek hakkında

Uno'nun SRAM'i 2 KB, ekran tamponu tek başına 1 KB yer kaplar. Kareler `PROGMEM` ile flash belleğe konduğu için SRAM'i yemez. 32x32 bir kare 128 bayttır, 12 kare yaklaşık 1.5 KB flash demektir, sorun olmaz. Kare boyutunu 64x64'e çıkarırsan kare başına 512 bayt olur, flash'ta yine rahat sığar ama ekranın yarısını kaplar.

### Küçük ipuçları

- Animasyonu ekranın başka bir yerine koymak için `ANIM_CX` ve `ANIM_CY` değerlerini değiştir.
- Daha akıcı görünmesi için kareler arası farkı küçük tut.
- Prosedürel (kodla çizilen) animasyonlar bellekten tasarruf eder. `drawGear` fonksiyonunu örnek alıp kendi şeklini (pervane, kalp atışı, orbit eden noktalar) yazabilirsin.

## Sorun giderme

| Sorun | Çözüm |
|-------|-------|
| Ekran hiç yanmıyor | VCC/GND ters olabilir. SDA-SCL yerlerini kontrol et. `OLED_ADDR` değerini `0x3D` yap. |
| "OLED bulunamadi" yazıyor (Seri Monitör) | Adres yanlış veya kablo gevşek. I2C tarayıcı (I2C scanner) sketch'i ile adresi bul. |
| "RTC bulunamadi" yazıyor | RTC'nin SDA/SCL bağlantısını kontrol et. DS1307 kullanıyorsan koddaki `RTC_DS3231` yerine `RTC_DS1307` yaz. |
| Saat her resette sıfırlanıyor | `SET_TIME_NOW` hâlâ 1 olabilir veya pil takılı/dolu değildir. |
| Ekran ters veya kaymış | 128x32 ekransan `SCREEN_HEIGHT` değerini 32 yap. Bazı 1.3" ekranlar SH1106 sürücülüdür, Adafruit SH110X kütüphanesi gerekir. |
| Animasyon negatif görünüyor | image2cpp'de **Invert image colors** seçeneğini değiştir. |
| Gün adı yanlış | RTC tarihini tekrar ayarla. Gün adı tarihten hesaplanır. |

## DS1307 notu

DS1307, DS3231 kadar hassas değildir (ayda dakikalar kayabilir) ve 5V ister. Kodda sadece `RTC_DS3231 rtc;` satırını `RTC_DS1307 rtc;` olarak değiştirmen yeterli. DS1307'de `lostPower()` yerine `!rtc.isrunning()` kullanılır.

## Lisans

MIT
