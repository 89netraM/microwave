#include <BetterWiFiNINA.h>
#include <errno.h>
#include <arduino-timer.h>

#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

#include <Keypad.h>

#include "arduino_secrets.h"
#include "index.html.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiSocket server(WiFiSocket::Type::Stream, WiFiSocket::Protocol::TCP);

#define FG_COLOR GC9A01A_WHITE
#define TIMER_COLOR GC9A01A_PURPLE
#define BG_COLOR GC9A01A_BLACK
#define WIFI_CONNECTING_COLOR GC9A01A_ORANGE
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

  drawWiFiSymbol(WIFI_ERROR_COLOR);

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

#define WIFI_CONNECT_TIMEOUT 1000
long previousWiFiConnectionAttempt = -WIFI_CONNECT_TIMEOUT;
bool handle_client(void*) {
  if (WiFi.status() != WL_CONNECTED) {
    long currentTime = millis();
    if (currentTime - WIFI_CONNECT_TIMEOUT < previousWiFiConnectionAttempt || isRunning) {
      drawWiFiSymbol(WIFI_ERROR_COLOR);
      return true;
    }
    drawWiFiSymbol(WIFI_CONNECTING_COLOR);
    previousWiFiConnectionAttempt = currentTime;
    if (WiFi.begin(ssid, pass) != WL_CONNECTED) {
      Serial.println("Could not connect to WiFi");
      drawWiFiSymbol(WIFI_ERROR_COLOR);
      return true;
    }
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    if (!server.bind(80)) {
      Serial.print("Binding server socket failed: error ");
      Serial.println(WiFiSocket::lastError());
      drawWiFiSymbol(WIFI_ERROR_COLOR);
      return true;
    }
    if (!server.listen(5)) {
      Serial.print("Listen on server socket failed: error ");
      Serial.println(WiFiSocket::lastError());
      drawWiFiSymbol(WIFI_ERROR_COLOR);
      return true;
    }
  }
  drawWiFiSymbol(BG_COLOR);

  IPAddress addr;
  uint16_t port;
  WiFiSocket client = server.accept(addr, port);
  if (!client) {
    Serial.print("Accept on server socket failed: error ");
    Serial.println(WiFiSocket::lastError());
    return true;
  }
  if (!client.setNonBlocking(true)) {
    Serial.print("Setting socket to non-blocking failed: error ");
    Serial.println(WiFiSocket::lastError());
    return true;
  }

  char lineBuffer[11];
READ:
  auto read = client.recv(lineBuffer, sizeof(lineBuffer));
  if (read < 0) {
    auto err = WiFiSocket::lastError();
    if (err == EWOULDBLOCK) {
      goto READ;
    }
    Serial.print("reading from socket failed with error: ");
    Serial.println(err);
    return true;
  }
  if (strcmp(lineBuffer, "GET / ") == 0) {
    // write_web_page(&client);
  } else if (strcmp(lineBuffer, "GET /state ") == 0) {
    // write_state_json(&client);
  } else if (strcmp(lineBuffer, "PUT /state ") == 0) {
    // update_and_write_state_json(&client);
  } else {
    write_404(&client);
  }
  client.close();

  return true;
}

void write_404(WiFiSocket* client) {
  write_to_client(client, "HTTP/1.1 404 Not Found\r\n");
}

void write_to_client(WiFiSocket* client, char* data) {
  size_t length = strlen(data);
  size_t written = 0;
  while (written < length) {
    auto sent = client->send(data + written, length - written);
    if (sent < 0) {
      auto err = WiFiSocket::lastError();
      if (err == EWOULDBLOCK) {
        continue;
      }
      Serial.print("writing to socket failed with error: ");
      Serial.println(err);
      return;
    }
    written += sent;
  }
}

// void write_web_page(WiFiSocket* client) {
//   read_to_end(client);
//   client->println("HTTP/1.1 200 OK");
//   client->println("Content-Type: text/html");
//   client->println();

//   client->print(INDEX_HTML_HEAD);

//   client->print("const initialMinutes = ");
//   client->print(minutes);
//   client->print(";");

//   client->print("const initialSeconds = ");
//   client->print(seconds);
//   client->print(";");

//   client->print("const initialPower = ");
//   client->print(power);
//   client->print(";");

//   client->print("const initialIsRunning = ");
//   client->print(isRunning ? "true" : "false");
//   client->print(";");

//   client->print("const initialRemainingInSeconds = ");
//   client->print(isRunning ? (time - constrain(millis() - startTime, 0, time)) / 1000 : 0);
//   client->print(";");

//   client->println(INDEX_HTML_REST);
// }

// void write_state_json(WiFiSocket* client) {
//   read_to_end(client);
//   Serial.println("HTTP/1.1 200 OK");
//   client->println("HTTP/1.1 200 OK");
//   Serial.println("Content-Type: application/json");
//   client->println("Content-Type: application/json");
//   Serial.println();
//   client->println();

//   Serial.print("{\"minutes\":");
//   client->print("{\"minutes\":");
//   Serial.print(minutes);
//   client->print(minutes);
//   Serial.print(",");
//   client->print(",");

//   Serial.print("\"seconds\":");
//   client->print("\"seconds\":");
//   Serial.print(seconds);
//   client->print(seconds);
//   Serial.print(",");
//   client->print(",");

//   Serial.print("\"power\":");
//   client->print("\"power\":");
//   Serial.print(power);
//   client->print(power);
//   Serial.print(",");
//   client->print(",");

//   Serial.print("\"isRunning\":");
//   client->print("\"isRunning\":");
//   Serial.print(isRunning ? "true" : "false");
//   client->print(isRunning ? "true" : "false");
//   Serial.print(",");
//   client->print(",");

//   Serial.print("\"remainingInSeconds\":");
//   client->print("\"remainingInSeconds\":");
//   Serial.print(isRunning ? (time - constrain(millis() - startTime, 0, time)) / 1000 : 0);
//   client->print(isRunning ? (time - constrain(millis() - startTime, 0, time)) / 1000 : 0);
//   Serial.println("}");
//   client->println("}");
// }

// #define JSON_VALUE_NOT_AVAILABLE 0x4000000000000000
// #define JSON_VALUE_INVALID 0x8000000000000000
// void update_and_write_state_json(WiFiSocket* client) {
//   char readBuffer[13] = "          \r\n";
//   long m = JSON_VALUE_NOT_AVAILABLE;
//   long s = JSON_VALUE_NOT_AVAILABLE;
//   long p = JSON_VALUE_NOT_AVAILABLE;
//   long r = JSON_VALUE_NOT_AVAILABLE;
//   while (client->available()) {
//     read_single_byte(client, readBuffer);

//     if (strcmp(readBuffer + 2, "\"minutes\":") == 0) {
//       m = read_number(client, readBuffer);
//     } else if (strcmp(readBuffer + 2, "\"seconds\":") == 0) {
//       s = read_number(client, readBuffer);
//     } else if (strcmp(readBuffer + 4, "\"power\":") == 0) {
//       p = read_number(client, readBuffer);
//     } else if (strcmp(readBuffer, "\"isRunning\":") == 0) {
//       r = read_bool(client, readBuffer);
//     }
//   }

//   if ((m & JSON_VALUE_NOT_AVAILABLE) == 0) {
//     if (isRunning) {
//       read_to_end(client);
//       client->println("HTTP/1.1 400 Bad Request");
//       client->println();
//       return;
//     }
//     if (0 <= m && m <= 99) {
//       minutes = m;
//       time = (minutes * 60 + seconds) * 1000;
//       writeTime(time);
//     } else {
//       m = JSON_VALUE_INVALID;
//     }
//   }

//   if ((s & JSON_VALUE_NOT_AVAILABLE) == 0) {
//     if (isRunning) {
//       read_to_end(client);
//       client->println("HTTP/1.1 400 Bad Request");
//       client->println();
//       return;
//     }
//     if (0 <= s && s <= 99) {
//       seconds = s;
//       time = (minutes * 60 + seconds) * 1000;
//       writeTime(time);
//     } else {
//       s = JSON_VALUE_INVALID;
//     }
//   }

//   if ((p & JSON_VALUE_NOT_AVAILABLE) == 0) {
//     if (isRunning) {
//       read_to_end(client);
//       client->println("HTTP/1.1 400 Bad Request");
//       client->println();
//       return;
//     }
//     if (1 <= p && p <= 100) {
//       power = p;
//       writePower();
//     } else {
//       p = JSON_VALUE_INVALID;
//     }
//   }

//   if ((r & JSON_VALUE_NOT_AVAILABLE) == 0) {
//     bool rb = r & 1 == 1;
//     if (isRunning != rb) {
//       if (rb) {
//         isRunning = true;
//         editing = EditingNone;
//         startTime = millis();
//         writeEditMarker();
//         fillTimer();
//       } else {
//         isRunning = false;
//         editing = EditingTime;
//         writeTime(time);
//         writeEditMarker();
//         fillTimer();
//       }
//     }
//   }

//   if (((m | s | p | r) & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//     read_to_end(client);
//     client->println("HTTP/1.1 422 Unprocessable Content");
//     client->println();
//     client->print("[");
//     if ((m & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//       client->print("\"minutes\"");
//     }
//     if ((s & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//       if ((m & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//         client->print(",");
//       }
//       client->print("\"seconds\"");
//     }
//     if ((p & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//       if (((m | s) & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//         client->print(",");
//       }
//       client->print("\"power\"");
//     }
//     if ((r & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//       if (((m | s | p) & JSON_VALUE_INVALID) == JSON_VALUE_INVALID) {
//         client->print(",");
//       }
//       client->print("\"isRunning\"");
//     }
//     client->println("]");
//   } else {
//     read_to_end(client);
//     write_state_json(client);
//   }
// }

// long read_number(WiFiSocket* client, char* readBuffer) {
//   read_json_value(client, readBuffer);
//   int end = 11;
//   while (true) {
//     if (end < 0) {
//       return JSON_VALUE_INVALID;
//     }
//     if ('0' <= readBuffer[end] && readBuffer[end] <= '9') {
//       break;
//     }
//     end--;
//   }
//   long value = 0;
//   long mul = 1;
//   while (true) {
//     if (end < 0) {
//       return JSON_VALUE_INVALID;
//     }
//     if ('0' <= readBuffer[end] && readBuffer[end] <= '9') {
//       value += (readBuffer[end] - '0') * mul;
//     } else {
//       return value;
//     }
//     end--;
//     mul *= 10;
//   }
// }

// long read_bool(WiFiSocket* client, char* readBuffer) {
//   read_json_value(client, readBuffer);
//   if (readBuffer[6] == 'f' && readBuffer[7] == 'a' && readBuffer[8] == 'l' && readBuffer[9] == 's' && readBuffer[10] == 'e') {
//     return 0;
//   } else if (readBuffer[7] == 't' && readBuffer[8] == 'r' && readBuffer[9] == 'u' && readBuffer[10] == 'e') {
//     return 1;
//   } else {
//     return JSON_VALUE_INVALID;
//   }
// }

// int read_json_value(WiFiSocket* client, char* readBuffer) {
//   int read = 0;
//   while (client->available()) {
//     char c = client->read();
//     if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
//       continue;
//     }
//     memmove(readBuffer, readBuffer + 1, 11);
//     readBuffer[11] = c;
//     read++;
//     if (c == ',' || c == '}') {
//       return read;
//     }
//   }
//   return -read - 1;
// }

// char read_single_byte(WiFiSocket* client, char* readBuffer) {
//   memmove(readBuffer, readBuffer + 1, 11);
//   return readBuffer[11] = client->read();
// }

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

uint16_t previousWiFiSymboColor = BG_COLOR;
void drawWiFiSymbol(uint16_t wiFiSymbolColor) {
  if (previousWiFiSymboColor == wiFiSymbolColor) {
    return;
  }

  tft.fillCircle(120, 16, 2, wiFiSymbolColor);

  previousWiFiSymboColor = wiFiSymbolColor;
}
