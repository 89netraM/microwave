#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <arduino-timer.h>

#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

#include <Keypad.h>

#include "arduino_secrets.h"
#include "index.html.h"

class ClientReaderWriter
{
private:
  WiFiClient* client;
public:
  ClientReaderWriter(WiFiClient* c) : client(c) { }

  int read() {
    return this->client->read();
  }

  size_t readBytes(char* buffer, size_t length) {
    return this->client->readBytes(buffer, length);
  }

  size_t write(uint8_t c) {
    return this->client->write(c);
  }

  size_t write(const uint8_t* buffer, size_t length) {
    return this->client->write(buffer, length);
  }
};

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiServer server(80);

#define FG_COLOR GC9A01A_WHITE
#define TIMER_COLOR GC9A01A_PURPLE
#define BG_COLOR GC9A01A_BLACK
#define WIFI_ERROR_COLOR GC9A01A_RED
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

int INDICATOR = 13;
int LIGHTS = 2;
int MICRO = 3;

#define POWER_PERIOD 10000

Timer<4> timer;
bool isRunning = false;
long minutes = 0;
long seconds = 0;
long time = 0;
long power = 100;
long startTime = 0;

enum Editing {
  EditingNone,
  EditingTime,
  EditingPower,
};
enum Editing editing = EditingTime;

void setup() {
  // USB Logging
  Serial.begin(9600);

  // Setup WiFi and server
  WiFi.setHostname("microwave");

  // Initializes display
  tft.begin();
  tft.fillScreen(BG_COLOR);

  tft.setFont(&FreeSans24pt7b);
  tft.drawChar(62 + 26 + 26, 136, ':', FG_COLOR, BG_COLOR, 1);

  writePower();
  writeTime(time);
  writeEditMarker();
  fillTimer();

  drawWiFiSumbol(false);
  
  // Set outputs
  pinMode(LIGHTS, OUTPUT);
  pinMode(MICRO, OUTPUT);
  pinMode(INDICATOR, OUTPUT);
  digitalWrite(INDICATOR, PinStatus::HIGH);

  // Activate timer
  timer.every(100, drawScreenTime);
  timer.every(100, handle_timer);
  timer.every(100, handle_client);
  timer.every(100, readKeypad);
}

void loop() {
  timer.tick();
}

bool handle_client(void*) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Trying to connect");
    if (WiFi.begin(ssid, pass) == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      Serial.print("IP Address: ");
      Serial.println(ip);
      server.begin();
    } else {
      drawWiFiSumbol(false);
      return true;
    }
  }
  drawWiFiSumbol(true);
  
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.startsWith("GET / ")) {
            write_web_page(&client);
            Serial.println("Done write_web_page");
          } else if (currentLine.startsWith("GET /state ")) {
            write_state_json(&client);
            Serial.println("Done write_state_json");
          } else if (currentLine.startsWith("PUT /state ")) {
            update_and_write_state_json(&client);
            Serial.println("Done update_and_write_state_json");
            Serial.println("Done update_and_write_state_json 2");
          } else {
            write_404(&client);
            Serial.println("Done write_404");
          }
          break;
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    Serial.println("Stopping");
    client.stop();
    Serial.println("Stopped");
  }

  return true;
}

void write_404(WiFiClient* client) {
  client->println("HTTP/1.1 404 Not Found");
  client->println();
}

void write_web_page(WiFiClient* client) {
  char formatBuffer[40];

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println();
  client->println(INDEX_HTML_HEAD);
	sprintf(formatBuffer, "const initialMinutes = %d;", minutes);
  client->println(formatBuffer);
	sprintf(formatBuffer, "const initialSeconds = %d;", seconds);
  client->println(formatBuffer);
	sprintf(formatBuffer, "const initialPower = %d;", power);
  client->println(formatBuffer);
	sprintf(formatBuffer, "const initialIsRunning = %s;", isRunning ? "true" : "false");
  client->println(formatBuffer);
	sprintf(formatBuffer, "const initialRemainingInSeconds = %d;", isRunning ? (time - constrain(millis() - startTime, 0, time)) / 1000 : 0);
  client->println(formatBuffer);
  client->println(INDEX_HTML_REST);
}

void write_state_json(WiFiClient* client) {
  JsonDocument doc;
  doc["minutes"] = minutes;
  doc["seconds"] = seconds;
  doc["power"] = power;
  doc["isRunning"] = isRunning;
  doc["remainingTimeInSeconds"] = isRunning ? (time - constrain(millis() - startTime, 0, time)) / 1000 : 0;
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println();
  ClientReaderWriter writer(client);
  serializeJson(doc, writer);
  client->println();
}

void update_and_write_state_json(WiFiClient* client) {
  char readBuffer[5] = "  \r\n";
  while (strcmp(readBuffer, "\r\n\r\n") != 0) {
    if (!client->available()) {
      return;
    }
    readBuffer[0] = readBuffer[1];
    readBuffer[1] = readBuffer[2];
    readBuffer[2] = readBuffer[3];
    readBuffer[3] = client->read();
  }

  JsonDocument doc;
  ClientReaderWriter readerWriter(client);
  DeserializationError error = deserializeJson(doc, readerWriter);
  if (error) {
    client->println("HTTP/1.1 500 Internal Server Error");
    client->println();
    return;
  }

  JsonDocument responseDoc;

  Serial.println("3");
  if (doc.containsKey("minutes")) {
    if (isRunning) {
      client->println("HTTP/1.1 400 Bad Request");
      client->println();
      return;
    }
    long m = doc["minutes"];
    if (0 <= m && m <= 99) {
      minutes = m;
      time = (minutes * 60 + seconds) * 1000;
      writeTime(time);
    } else {
      responseDoc.add("minutes");
    }
  }

  Serial.println("2");
  if (doc.containsKey("seconds")) {
    if (isRunning) {
      client->println("HTTP/1.1 400 Bad Request");
      client->println();
      return;
    }
    long s = doc["seconds"];
    if (0 <= s && s <= 99) {
      seconds = s;
      time = (minutes * 60 + seconds) * 1000;
      writeTime(time);
    } else {
      responseDoc.add("seconds");
    }
  }

  Serial.println("1");
  long p = doc["power"] | power;
  if (p != power) {
    Serial.println("q");
    if (isRunning) {
    Serial.println("r");
      client->println("HTTP/1.1 400 Bad Request");
    Serial.println("t");
      client->println();
    Serial.println("y");
      return;
    }
    Serial.println("u");
    Serial.println("i");
    if (1 <= p && p <= 100) {
    Serial.println("o");
      power = p;
    Serial.println("p");
      writePower();
    Serial.println("a");
    } else {
    Serial.println("s");
      responseDoc.add("power");
    Serial.println("d");
    }
    Serial.println("f");
  }
    Serial.println("g");

  Serial.println("0");
  if (doc.containsKey("isRunning")) {
    bool r = doc["isRunning"];
    if (isRunning != r) {
      if (r) {
        isRunning = true;
        editing = EditingNone;
        startTime = millis();
        writeEditMarker();
        fillTimer();
      } else {
        isRunning = false;
        editing = EditingTime;
        writeTime(time);
        writeEditMarker();
        fillTimer();
      }
    }
  }

  Serial.println("A");
  if (responseDoc.size() != 0) {
    client->println("HTTP/1.1 422 Unprocessable Content");
  } else {
  Serial.println("B");
    responseDoc["minutes"] = minutes;
  Serial.println("c");
    responseDoc["seconds"] = seconds;
  Serial.println("d");
    responseDoc["power"] = power;
  Serial.println("e");
    responseDoc["isRunning"] = isRunning;
  Serial.println("F");
    responseDoc["remainingTimeInSeconds"] = (time - constrain(millis() - startTime, 0, time)) / 1000;
  Serial.println("G");
    client->println("HTTP/1.1 200 OK");
  }

  Serial.println("H");
  client->println("Content-Type: application/json");
  Serial.println("I");
  client->println();
  Serial.println("J");
  serializeJson(responseDoc, readerWriter);
  Serial.println("K");
  client->println("Hej Vidde!");
  client->println();
  client->flush();
  Serial.println("End of state json");
  return;
}

bool handle_timer(void*) {
  if (!isRunning) {
    return true;
  }

  long currentTime = millis();
  long elapsedTime = constrain(currentTime - startTime, 0, time);
  long remainingTime = time - elapsedTime;

  if (remainingTime <= 0) {
    digitalWrite(MICRO, PinStatus::LOW);
    digitalWrite(LIGHTS, PinStatus::LOW);
    isRunning = false;
    editing = EditingTime;
    minutes = 0;
    seconds = 0;
    time = 0;
    writeTime(time);
    writeEditMarker();
    fillTimer();
    return true;
  }

  if (remainingTime % POWER_PERIOD < power) {
    digitalWrite(MICRO, PinStatus::HIGH);
  } else {
    digitalWrite(MICRO, PinStatus::LOW);
  }

  return true;
}

bool readKeypad(void*) {
  char key = keypad.getKey();
  if (!key) {
    return true;
  }
  Serial.print("Reading key: ");
  Serial.println(key);

  if (isRunning) {
    if (key == '#') {
      isRunning = false;
      editing = EditingTime;
      minutes = 0;
      seconds = 0;
      time = 0;
      writeTime(time);
      writeEditMarker();
      fillTimer();
    }
  } else {
    if (key == '*') {
      if (editing == EditingTime) {
        editing = EditingPower;
      } else if (editing == EditingPower) {
        editing = EditingTime;
      } else {
        editing = EditingTime;
      }
      writeEditMarker();
    } else if (key == '#') {
      isRunning = true;
      editing = EditingNone;
      startTime = millis();
      writeEditMarker();
      fillTimer();
    } else if ('0' <= key && key <= '9') {
      if (editing == EditingTime) {
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
      } else if (editing == EditingPower) {
        if (power > 99) {
          power = 0;
        }

        power *= 10;
        power += key - '0';
        if (power > 100) {
          power = 100;
        }
        writePower();
      }
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
  if (m > minutes) {
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
void writePower() {
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

enum Editing previousEditing = EditingNone;
void writeEditMarker() {
#define EDITING_MARKER_HEIGHT 3

#define DRAW_EDITING_TIME(color) tft.fillRect(62, 142, 116, EDITING_MARKER_HEIGHT, color);
#define DRAW_EDITING_POWER(color) tft.fillRect(94, 178, 60, EDITING_MARKER_HEIGHT, color);

  if (previousEditing == editing) {
    return;
  }

  if (previousEditing == EditingTime) {
    DRAW_EDITING_TIME(BG_COLOR);
  } else if (previousEditing == EditingPower) {
    DRAW_EDITING_POWER(BG_COLOR);
  }

  if (editing == EditingTime) {
    DRAW_EDITING_TIME(TIMER_COLOR);
  } else if (editing == EditingPower) {
    DRAW_EDITING_POWER(TIMER_COLOR);
  }

  previousEditing = editing;
}

bool previousWiFiSymbolConnected = true;
void drawWiFiSumbol(bool wiFiSymbolConnected) {
  if (previousWiFiSymbolConnected == wiFiSymbolConnected) {
    return;
  }

  tft.fillCircle(120, 14, 2, wiFiSymbolConnected ? BG_COLOR : WIFI_ERROR_COLOR);

  previousWiFiSymbolConnected = wiFiSymbolConnected;
}
