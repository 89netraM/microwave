#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Keypad.h>
#include <arduino-timer.h>

#define FG_COLOR GC9A01A_WHITE
#define TIMER_COLOR GC9A01A_PURPLE
#define BG_COLOR GC9A01A_BLACK
#define SIZE 240
#define RADIUS (SIZE / 2)
#define TIMER_WIDTH 10
#define TIMER_RADIUS (RADIUS - TIMER_WIDTH)

Adafruit_GC9A01A tft(10, 7, 11, 12, 8, 9);

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
byte rowPins[ROWS] = { 19, 14, 15, 17 };
byte colPins[COLS] = { 18, 20, 16 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Timer<2> timer;
bool isRunning = false;
long minutes = 0;
long seconds = 0;
long time = 0;
long power = 100;
long startTime = 0;

void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.fillScreen(BG_COLOR);

  tft.setFont(&FreeSans24pt7b);
  tft.drawChar(62 + 26 + 26, 136, ':', FG_COLOR, BG_COLOR, 1);

  writePower(power);
  writeTime(time);
  fillTimer();

  timer.every(100, drawScreenTime);
  timer.every(100, readKeypad);
}

void loop() {
  timer.tick();
}

bool readKeypad(void*) {
  char key = keypad.getKey();
  if (!key) {
    return true;
  }

  Serial.print("Key: ");
  Serial.println(key);
  if (isRunning) {
    if (key == '#') {
      isRunning = false;
      minutes = 0;
      seconds = 0;
      time = 0;
      writeTime(time);
      fillTimer();
    }
  } else {
    if ('0' <= key && key <= '9') {
      if (minutes > 9) {
        minutes = 0;
        seconds = 0;
      }

      minutes *= 10;
      seconds *= 10;
      minutes += seconds / 100;
      seconds = seconds % 100;
      seconds += key - '0';

      time = (minutes * 60 + seconds) * 1000;
      writeTime(time);
    } else if (key == '#') {
      isRunning = true;
      startTime = millis();
      fillTimer();
    }
  }
  return true;
}

bool drawScreenTime(void*) {
  if (!isRunning) {
    return true;
  }

  long currentTime = millis();
  long elapsedTime = constrain(currentTime - startTime, 0, time);

  writeTime(time - elapsedTime);
  clearTimerPortion(1.0 - (float)elapsedTime / (float)time);

  return true;
}

char previousTimeText[4] PROGMEM;
void writeTime(long t) {
#define PRINT_CHAR(offset, index) \
  if (buffer[index] != previousTimeText[index]) { \
    if (previousTimeText[index]) { \
      tft.drawChar(62 + offset, 136, previousTimeText[index], BG_COLOR, BG_COLOR, 1); \
    } \
    tft.drawChar(62 + offset, 136, buffer[index], FG_COLOR, BG_COLOR, 1); \
    previousTimeText[index] = buffer[index]; \
  }

  long m = (t / 1000) / 60;
  long s = (t / 1000) % 60;
  if (m >= minutes) {
    s += (m - minutes) * 60;
    m = minutes;
  }
  char buffer[4];
  sprintf(buffer, "%02d%02d", m, s);
  tft.setFont(&FreeSans24pt7b);
  PRINT_CHAR(0, 0);
  PRINT_CHAR(26, 1);
  PRINT_CHAR(26 + 26 + 12, 2);
  PRINT_CHAR(26 + 26 + 12 + 26, 3);
}

char previousPowerText[4] PROGMEM;
void writePower(long power) {
#define PRINT_CHAR(offset, index) \
  if (buffer[index] != previousPowerText[index]) { \
    if (previousPowerText[index]) { \
      tft.drawChar(94 + offset, 172, previousPowerText[index], BG_COLOR, BG_COLOR, 1); \
    } \
    tft.drawChar(94 + offset, 172, buffer[index], FG_COLOR, BG_COLOR, 1); \
    previousPowerText[index] = buffer[index]; \
  }

  char buffer[4];
  sprintf(buffer, "%3d%%", power);
  tft.setFont(&FreeSans12pt7b);
  PRINT_CHAR(0, 0);
  PRINT_CHAR(13, 1);
  PRINT_CHAR(13 + 13, 2);
  PRINT_CHAR(13 + 13 + 13, 3);
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
