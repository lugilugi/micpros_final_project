#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <WiFi.h>
#include "config.h"
#include "datatypes.h"
#include "fsm.h"
#include "productmoduleinterface.h"
#include "googlesheets.h"

// ===================== HARDWARE INSTANCES ==============================

// LCD Display (I2C 20x4)
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 20, 4);

// Keypad (4x4 Matrix)
char keysMap[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pin definitions for keypad (define here to satisfy externs in config.h)
byte rowPins[ROWS] = {14, 27, 26, 25};
byte colPins[COLS] = {33, 32, 18, 19};

Keypad keypad = Keypad(makeKeymap(keysMap), rowPins, colPins, ROWS, COLS);
// Last read keypad character (stored so we don't call getKey() twice)
char lastKey = 0;

// ===================== EVENT DETECTION ===================================

Event detectEvent() {
  // Priority 1: Keypad input (all states)
  char key = keypad.getKey();
  if (key) {
    lastKey = key; // stash the key for later use
    if (key == '*') return EVT_KEY_CANCEL;
    if (key == '#') return EVT_KEY_SUBMIT;
    Serial.println(key);
    return EVT_KEY_CHAR;
  }

  // Priority 2: Periodic sync check (in IDLE only)
  if (currentState == STATE_IDLE) {
    unsigned long now = millis();
    if (now - syncTimer > SYNC_INTERVAL_MS) {
      return EVT_SYNC_TIMEOUT;
    }
  }

  return EVT_NONE;
}

// ===================== EVENT PROCESSING LOOP =============================

void processEventLoop() {
  Event evt = detectEvent();
  if (evt != EVT_NONE) {
    Serial.print("[EVENT] detected evt=");
    Serial.print((int)evt);
    Serial.print(" currentState=");
    Serial.println((int)currentState);
  }
  
  // Process events through FSM. For character events we call the FSM
  // first (it may transition into ITEM_SELECT) then consume the
  // stashed `lastKey` if we're in `STATE_ITEM_SELECT` to display it.
  if (evt != EVT_NONE) {
    if (evt == EVT_KEY_CHAR) {
      processEvent(evt);
      // If we're now in ITEM_SELECT, consume and display the character
      if (currentState == STATE_ITEM_SELECT && lastKey) {
        char key = lastKey;
        lastKey = 0; // consume
        if (key && inputBuffer.length() < 20) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          lcd.print(inputBuffer);
          // Pad with spaces to clear old text
          for (int i = inputBuffer.length(); i < 20; i++) {
            lcd.print(" ");
          }
        }
      }
    } else {
      // non-character events should clear any stashed key
      lastKey = 0;
      processEvent(evt);
    }
  }
  
  // Execute current state actions (timeouts, periodic tasks, etc.)
  onStateAction(currentState);
}

// ===================== INITIALIZATION ====================================

void setup() {
  Serial.begin(115200);
  delay(200);
  
  Serial.println("\n\n=== VENDING SYSTEM INITIALIZATION ===\n");
  
  // Initialize I2C for product modules
  Wire.begin();
  Serial.println("[1/5] I2C initialized");
  
  // Initialize LCD display
  lcd.init(I2C_SDA, I2C_SCL);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VENDING SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  Serial.println("[2/5] LCD initialized");
  
  delay(1000);
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("[3/5] WiFi connection started");
  
  // Discover product modules on I2C bus
  discoverProductModules();
  Serial.println("[4/5] Module discovery complete");
  
  // Sync initial product data from Google Sheets
  ensureWiFi();
  if (isWiFiConnected()) {
    syncProductDataFromSheets();
    matchModulesToSheets();
    syncModuleDisplays();
    Serial.println("[5/5] Google Sheets sync complete");
  } else {
    Serial.println("[!] WiFi not connected - will sync when available");
  }
  
  // Initialize FSM
  initFSM();
  enterState(STATE_IDLE);
  
  Serial.println("\n=== INITIALIZATION COMPLETE ===\n");
}

// ===================== MAIN LOOP ==========================================

void loop() {
  // Process events and state machine
  processEventLoop();
}
