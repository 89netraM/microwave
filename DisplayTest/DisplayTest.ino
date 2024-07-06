#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans24pt7b.h>

Adafruit_GC9A01A tft(10, 7, 11, 12, 8, 9);

void setup() {
  tft.begin();
  tft.setFont(&FreeSans24pt7b);
  tft.setTextWrap(false);
  tft.fillScreen(GC9A01A_BLACK);
}

void loop() {
  for (int i = 120; i >= 0; i--) {
    tft.setTextColor(GC9A01A_WHITE);
    writeTime(i);
    tft.setTextColor(GC9A01A_BLACK);
    writeTime(i);
  }
}

void writeTime(unsigned int seconds) {
  unsigned int minutes = seconds / 60;
  seconds = seconds % 60;
  char buffer[5];
  sprintf(buffer, "%02d:%02d", minutes, seconds);
  tft.setCursor(120, 120);
  tft.print(buffer);
}
