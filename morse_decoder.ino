#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/*
====================================================
          USER CONFIGURATION
====================================================

Before uploading this project to your ESP32,
replace the following values with your own.

WiFi:
    YOUR_WIFI_NAME
    YOUR_WIFI_PASSWORD

Telegram:
    YOUR_TELEGRAM_BOT_TOKEN
    YOUR_TELEGRAM_CHAT_ID

How to get Bot Token:
1. Open Telegram.
2. Search for @BotFather.
3. Create a bot using /newbot.
4. Copy the Bot Token.

How to get Chat ID:
1. Send a message to your bot.
2. Open:
https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getUpdates
3. Find your "chat":{"id":xxxxxxxx}

====================================================
*/

// WiFi Credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram Configuration
String botToken = "YOUR_TELEGRAM_BOT_TOKEN";
String chatID   = "YOUR_TELEGRAM_CHAT_ID";


// -------- OLED --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------- PINS --------
// GPIO Connections
const int morseButton  = 18;   // Morse Button
const int spaceButton  = 19;   // Space/Delete Button
const int phraseButton = 21;   // Phrase/Send Button
const int buzzerPin    = 2;    // Passive Buzzer

// -------- MORSE --------
String morseCode = "";
String decodedMessage = "";
unsigned long pressStartTime = 0;
unsigned long releaseTime = 0;

// -------- TIMING --------
const unsigned int dotThreshold = 300;
const unsigned int letterPause  = 800;

const unsigned long spaceThreshold  = 1000;
const unsigned long letterThreshold = 2000;

// -------- TEXT SIZE (LARGER) --------
const int textSize   = 2;   // Increased size
const int charWidth  = 12;  // Approx width for size 2
const int charHeight = 16;  // Approx height for size 2
const int lineHeight = 18;

int cursorX = 0;
int cursorY = 0;

// -------- CURSOR --------
bool cursorVisible = true;
unsigned long lastBlink = 0;

// -------- PHRASES --------
String phrases[] = {
  "I NEED WATER",
  "I NEED FOOD",
  "HELP ME",
  "THANK YOU",
  "I NEED MEDICINE",
  "I AM NOT WELL"
};
int phraseIndex = 0;

/*
OLED Wiring

OLED SDA -> GPIO 4
OLED SCL -> GPIO 5

Buttons use INPUT_PULLUP.
Connect one side of each button to the GPIO pin
and the other side to GND.
*/


void setup() {
  pinMode(morseButton, INPUT_PULLUP);
  pinMode(spaceButton, INPUT_PULLUP);
  pinMode(phraseButton, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  Wire.begin(4, 5);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  display.clearDisplay();
  display.println("WiFi Connected");
  display.display();
  delay(1000);

  clearScreen();
}

void loop() {
  // -------- MORSE BUTTON --------
  if (digitalRead(morseButton) == LOW) {
    if (pressStartTime == 0) pressStartTime = millis();
  } else if (pressStartTime != 0) {
    unsigned long duration = millis() - pressStartTime;
    morseCode += (duration < dotThreshold) ? "." : "-";
    tone(buzzerPin, duration < dotThreshold ? 1000 : 600, 120);
    pressStartTime = 0;
    releaseTime = millis();
  }

  // -------- LETTER COMPLETE --------
  if (releaseTime && morseCode.length() && millis() - releaseTime > letterPause) {
    printChar(decodeMorse(morseCode));
    morseCode = "";
    releaseTime = 0;
  }

  // -------- SPACE / DELETE / CLEAR BUTTON --------
  static unsigned long spacePress = 0;
  if (digitalRead(spaceButton) == LOW) {
    if (!spacePress) spacePress = millis();
  } else if (spacePress) {
    unsigned long d = millis() - spacePress;
    if (d < spaceThreshold) printChar(' ');
    else if (d < letterThreshold) deleteLetter();
    else clearScreen();
    spacePress = 0;
  }

  // -------- PHRASE / SEND BUTTON --------
  static unsigned long phrasePress = 0;
  if (digitalRead(phraseButton) == LOW) {
    if (!phrasePress) phrasePress = millis();
  } else if (phrasePress) {
    if (millis() - phrasePress < 2000)
      printPhrase();
    else
      sendToPhone(decodedMessage);
    phrasePress = 0;
  }

  drawCursor();
  display.display();
}

// -------- SEND MESSAGE --------
void sendToPhone(String msg) {
  if (msg.length() == 0) return;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return;
  }

  HTTPClient http;

  msg.replace(" ", "%20");   // VERY IMPORTANT

  String url = "https://api.telegram.org/bot" + botToken +
               "/sendMessage?chat_id=" + chatID +
               "&text=" + msg;

  Serial.println("📤 Sending to Telegram...");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  Serial.print("HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);
  }

  http.end();

  tone(buzzerPin, 1000, 200);
}

// -------- UI FUNCTIONS --------
void drawCursor() {
  if (millis() - lastBlink > 500) {
    cursorVisible = !cursorVisible;
    lastBlink = millis();
  }
  display.fillRect(cursorX, cursorY + charHeight,
                   charWidth, 2,
                   cursorVisible ? SSD1306_WHITE : SSD1306_BLACK);
}

void printChar(char c) {
  decodedMessage += c;
  display.setCursor(cursorX, cursorY);
  display.print(c);
  cursorX += charWidth;
  if (cursorX > SCREEN_WIDTH - charWidth) {
    cursorX = 0;
    cursorY += lineHeight;
    if (cursorY > SCREEN_HEIGHT - lineHeight) cursorY = 0; // wrap vertically
  }
}

void deleteLetter() {
  if (decodedMessage.length() == 0) return;
  decodedMessage.remove(decodedMessage.length() - 1);
  redraw();
}

void redraw() {
  display.clearDisplay();
  cursorX = 0;
  cursorY = 0;
  for (char c : decodedMessage) printChar(c);
}

void printPhrase() {
  clearScreen();
  decodedMessage = phrases[phraseIndex];
  
  // Split phrase into words for line wrapping
  int lineLen = 0;
  int maxCharsPerLine = SCREEN_WIDTH / charWidth;
  String word = "";
  String temp = "";
  for (int i = 0; i < decodedMessage.length(); i++) {
    char c = decodedMessage[i];
    if (c == ' ' || i == decodedMessage.length() - 1) {
      if (i == decodedMessage.length() - 1) word += c;
      if (lineLen + word.length() > maxCharsPerLine) {
        temp += '\n';
        lineLen = 0;
      }
      temp += word + " ";
      lineLen += word.length() + 1;
      word = "";
    } else {
      word += c;
    }
  }
  decodedMessage = temp;
  redraw();
  phraseIndex = (phraseIndex + 1) % 6;
}

void clearScreen() {
  display.clearDisplay();
  decodedMessage = "";
  cursorX = 0;
  cursorY = 0;
}

// -------- MORSE DECODING --------
char decodeMorse(String c) {
  if (c == ".-") return 'A';
  if (c == "-...") return 'B';
  if (c == "-.-.") return 'C';
  if (c == "-..") return 'D';
  if (c == ".") return 'E';
  if (c == "..-.") return 'F';
  if (c == "--.") return 'G';
  if (c == "....") return 'H';
  if (c == "..") return 'I';
  if (c == ".---") return 'J';
  if (c == "-.-") return 'K';
  if (c == ".-..") return 'L';
  if (c == "--") return 'M';
  if (c == "-.") return 'N';
  if (c == "---") return 'O';
  if (c == ".--.") return 'P';
  if (c == "--.-") return 'Q';
  if (c == ".-.") return 'R';
  if (c == "...") return 'S';
  if (c == "-") return 'T';
  if (c == "..-") return 'U';
  if (c == "...-") return 'V';
  if (c == ".--") return 'W';
  if (c == "-..-") return 'X';
  if (c == "-.--") return 'Y';
  if (c == "--..") return 'Z';
  return '?';
}



/*
======================================================
        ESP32 MORSE COMMUNICATION DEVICE
======================================================

Features
--------
• Morse code decoding
• OLED text display
• Preset emergency phrases
• Delete / Clear text
• Telegram message sending
• Audible buzzer feedback

Hardware
--------
ESP32 Dev Module
SSD1306 OLED (128x64)
3 Push Buttons
Passive Buzzer

Author:
Kishor.S

License:
MIT License

======================================================
*/

