#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans24pt7b.h>
#include <arduino-timer.h>

#define FG_COLOR GC9A01A_WHITE
#define TIMER_COLOR GC9A01A_PURPLE
#define BG_COLOR GC9A01A_BLACK
#define SIZE 240
#define RADIUS (SIZE / 2)
#define TIMER_WIDTH 10
#define TIMER_RADIUS (RADIUS - TIMER_WIDTH)

Adafruit_GC9A01A tft(10, 7, 11, 12, 8, 9);

Timer<1> timer;
long time = 120000;
long startTime = -time - 1;

void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.setFont(&FreeSans24pt7b);
  tft.setTextWrap(false);
  tft.setTextColor(FG_COLOR);
  tft.fillScreen(BG_COLOR);
  tft.drawChar(62 + 26 + 26, 136, ':', FG_COLOR, BG_COLOR, 1);

  timer.every(100, drawScreen);
}

void loop() {
  timer.tick();
}

bool drawScreen(void*) {
  long currentTime = millis();
  long elapsedTime = currentTime - startTime;

  if (elapsedTime > time) {
    startTime = currentTime;
    elapsedTime = 0;
    fillTimer();
  }

  writeTime((time - elapsedTime) / 1000);
  clearTimerPortion(1.0 - (float)elapsedTime / (float)time);

  return true;
}

char previousText[4] PROGMEM;
void writeTime(unsigned int seconds) {
#define PRINT_CHAR(offset, index) \
  if (buffer[index] != previousText[index]) { \
    if (previousText[index]) { \
      tft.drawChar(62 + offset, 136, previousText[index], BG_COLOR, BG_COLOR, 1); \
    } \
    tft.drawChar(62 + offset, 136, buffer[index], FG_COLOR, BG_COLOR, 1); \
    previousText[index] = buffer[index]; \
  }

  unsigned int minutes = seconds / 60;
  seconds = seconds % 60;
  char buffer[4];
  sprintf(buffer, "%02d%02d", minutes, seconds);
  PRINT_CHAR(0, 0);
  PRINT_CHAR(26, 1);
  PRINT_CHAR(26 + 26 + 12, 2);
  PRINT_CHAR(26 + 26 + 12 + 26, 3);
}

float timerPortionCleared = 1.0;
void fillTimer() {
  tft.startWrite();

  tft.writeFillRect(0, 0, RADIUS - TIMER_RADIUS, SIZE, TIMER_COLOR);
  tft.writeFillRect(SIZE - (RADIUS - TIMER_RADIUS), 0, RADIUS - TIMER_RADIUS, SIZE, TIMER_COLOR);
  tft.writeFillRect(0, 0, SIZE, RADIUS - TIMER_RADIUS, TIMER_COLOR);
  tft.writeFillRect(0, SIZE - (RADIUS - TIMER_RADIUS), SIZE, RADIUS - TIMER_RADIUS, TIMER_COLOR);

  int16_t f = 1 - TIMER_RADIUS;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * TIMER_RADIUS;
  int16_t x = 0;
  int16_t y = TIMER_RADIUS;
  int16_t px = x;
  int16_t py = y;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    // These checks avoid double-drawing certain lines, important
    // for the SSD1306 library which has an INVERT drawing mode.
    if (x < (y + 1)) {
      tft.writeFastVLine(RADIUS + x, RADIUS - TIMER_RADIUS, TIMER_RADIUS - y, TIMER_COLOR);
      tft.writeFastVLine(RADIUS + x, RADIUS + y, TIMER_RADIUS - y, TIMER_COLOR);

      tft.writeFastVLine(RADIUS - x, RADIUS - TIMER_RADIUS, TIMER_RADIUS - y, TIMER_COLOR);
      tft.writeFastVLine(RADIUS - x, RADIUS + y, TIMER_RADIUS - y, TIMER_COLOR);
    }
    if (y != py) {
      tft.writeFastVLine(RADIUS + py, RADIUS - TIMER_RADIUS, TIMER_RADIUS - px, TIMER_COLOR);
      tft.writeFastVLine(RADIUS + py, RADIUS + px, TIMER_RADIUS - px, TIMER_COLOR);

      tft.writeFastVLine(RADIUS - py, RADIUS - TIMER_RADIUS, TIMER_RADIUS - px, TIMER_COLOR);
      tft.writeFastVLine(RADIUS - py, RADIUS + px, TIMER_RADIUS - px, TIMER_COLOR);
      py = y;
    }
    px = x;
  }
  tft.endWrite();
  timerPortionCleared = 1.0;
}

uint16_t timerClearBuffer[7200] PROGMEM;
void clearTimerPortion(float percentage) {
#define PI 3.14159265

  int16_t fromX0 = RADIUS + (int16_t)((TIMER_RADIUS - 1) * cos((1.5 - 2.0 * timerPortionCleared) * PI));
  int16_t fromX1 = RADIUS + (int16_t)((RADIUS + 1) * cos((1.5 - 2.0 * timerPortionCleared) * PI));
  int16_t fromY0 = RADIUS + (int16_t)((TIMER_RADIUS - 1) * sin((1.5 - 2.0 * timerPortionCleared) * PI));
  int16_t fromY1 = RADIUS + (int16_t)((RADIUS + 1) * sin((1.5 - 2.0 * timerPortionCleared) * PI));

  int16_t toX0 = RADIUS + (int16_t)((TIMER_RADIUS - 1) * cos((1.5 - 2.0 * percentage) * PI));
  int16_t toX1 = RADIUS + (int16_t)((RADIUS + 1) * cos((1.5 - 2.0 * percentage) * PI));
  int16_t toY0 = RADIUS + (int16_t)((TIMER_RADIUS - 1) * sin((1.5 - 2.0 * percentage) * PI));
  int16_t toY1 = RADIUS + (int16_t)((RADIUS + 1) * sin((1.5 - 2.0 * percentage) * PI));

  int16_t minX = min(fromX0, min(fromX1, min(toX0, toX1)));
  int16_t maxX = max(fromX0, max(fromX1, max(toX0, toX1)));
  int16_t minY = min(fromY0, min(fromY1, min(toY0, toY1)));
  int16_t maxY = max(fromY0, max(fromY1, max(toY0, toY1)));

  int16_t width = maxX - minX;
  int16_t height = maxY - minY;
  uint32_t length = width * height;

  float toAngle = atan2(toY0 - RADIUS, toX0 - RADIUS);

  for (int16_t y = minY; y <= maxY; y++) {
    for (int16_t x = minX; x <= maxX; x++) {
      if (min(toX0, toX1) <= x && x < max(toX0, toX1) && min(toY0, toY1) <= y && y < max(toY0, toY1)) {
        float distance = sqrt(pow(x - RADIUS, 2) + pow(y - RADIUS, 2));
        if (distance > TIMER_RADIUS) {
          float angle = atan2(y - RADIUS, x - RADIUS);
          if (angle > toAngle) {
            timerClearBuffer[(y - minY) * width + (x - minX)] = TIMER_COLOR;
          } else {
            timerClearBuffer[(y - minY) * width + (x - minX)] = BG_COLOR;
          }
        } else {
          timerClearBuffer[(y - minY) * width + (x - minX)] = BG_COLOR;
        }
      } else {
        timerClearBuffer[(y - minY) * width + (x - minX)] = BG_COLOR;
      }
    }
  }

  tft.startWrite();
  tft.setAddrWindow(minX, minY, width, height);
  tft.writePixels(timerClearBuffer, length);
  tft.endWrite();

  timerPortionCleared = percentage;
}
