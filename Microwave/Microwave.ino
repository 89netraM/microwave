#include <WiFiNINA.h>
#include <arduino-timer.h>

#include "arduino_secrets.h" 
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiServer server(80);

int INDICATOR = 13;
int LIGHTS = 2;
int MICRO = 3;

unsigned long TIMER_TICK = 100;

unsigned long POWER = 0;
unsigned long DURATION = 0;

unsigned long POWER_PERIOD = 10000 / TIMER_TICK;
unsigned long DURATION_SCALE = 1000 / TIMER_TICK;
unsigned long POWER_SCALE = POWER_PERIOD / 100;

unsigned long COMMING_POWER = 0;
unsigned long COMMING_DURATION = 0;

Timer<2> timer;

void setup() {
  // USB Logging
  Serial.begin(9600);

  // Setup WiFi and server
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    for (int i = 0; i < 100; i++) {
      delay(100);
      digitalWrite(INDICATOR, i % 2 == 0 ? PinStatus::HIGH : PinStatus::LOW);
    }
  }
  server.begin();
  
  // Set outputs
  pinMode(LIGHTS, OUTPUT);
  pinMode(MICRO, OUTPUT);
  pinMode(INDICATOR, OUTPUT);
  digitalWrite(INDICATOR, PinStatus::HIGH);

  // Activate timer
  timer.every(10, handle_client);
  timer.every(TIMER_TICK, handle_timer);
}

void loop() {
  timer.tick();
}

bool handle_client(void*) {
  WiFiClient client = server.available();
  void (*responseAction)(WiFiClient*) = write_web_page;
  if (client) {
    Serial.println("new client");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          if (currentLine.length() == 0) {
            responseAction(&client);
            break;
          } else {
            if (currentLine.startsWith("GET /start")) {
              read_start_input(&currentLine);
              responseAction = start_timer;
            } else if (currentLine.startsWith("GET /stop")) {
              responseAction = stop_timer;
            }
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("client disconnected");
  }

  return true;
}

void write_web_page(WiFiClient* client) {
  char formatBuffer[21];

  client->println("HTTP/1.1 200 OK");
  client->println("Content-type:text/html");
  client->println();
  client->print("Time left: ");
  sprintf(formatBuffer, "%u", DURATION / DURATION_SCALE);
  client->print(formatBuffer);
  client->print("<br>Power: ");
  sprintf(formatBuffer, "%u", POWER / POWER_SCALE);
  client->print(formatBuffer);
  client->println("<br>");
  client->println("<form action=\"/start\"><input name=\"power\" type=\"number\" required><input name=\"duration\" type=\"number\" required><input type=\"submit\" value=\"Start\"></form>");
  client->println("<form action=\"/stop\"><input type=\"submit\" value=\"Stop\"></form>");
}

void write_power_duration_error(WiFiClient* client) {
  client->println("<p>The power and/or duration could not be read, and the action was aborted.</p>");
}

void read_start_input(String* line) {
  auto powerIndex = line->indexOf("power=");
  auto durationIndex = line->indexOf("duration=");
  if (powerIndex && durationIndex) {
    COMMING_DURATION = read_number(line, durationIndex + 9) * DURATION_SCALE;
    auto commingPower = read_number(line, powerIndex + 6);
    if (0 < commingPower && commingPower <= 100) {
      COMMING_POWER = commingPower * POWER_SCALE;
    } else {
      COMMING_POWER = 0;
      COMMING_DURATION = 0;
    }
  } else {
    COMMING_POWER = 0;
    COMMING_DURATION = 0;
  }
}

unsigned long read_number(String* line, unsigned int index) {
  unsigned long number = 0;
  for (; index < line->length(); index++) {
    char c = line->charAt(index);
    if ('0' <= c && c <= '9') {
      number = number * 10 + (unsigned long)(c - '0');
    } else {
      break;
    }
  }
  return number;
}

void start_timer(WiFiClient* client) {
  POWER = COMMING_POWER;
  DURATION = COMMING_DURATION;

  write_web_page(client);

  if (POWER && DURATION) {
    digitalWrite(LIGHTS, PinStatus::HIGH);
  } else {
    write_power_duration_error(client);
  }
}

void stop_timer(WiFiClient* client) {
  POWER = 0;
  DURATION = 0;
  digitalWrite(MICRO, PinStatus::LOW);
  digitalWrite(LIGHTS, PinStatus::LOW);

  write_web_page(client);
}

bool handle_timer(void*) {
  if (DURATION == 0) {
    digitalWrite(MICRO, PinStatus::LOW);
    digitalWrite(LIGHTS, PinStatus::LOW);
    return true;
  }

  DURATION--;

  if (DURATION % POWER_PERIOD < POWER) {
    digitalWrite(MICRO, PinStatus::HIGH);
  } else {
    digitalWrite(MICRO, PinStatus::LOW);
  }

  return true;
}
