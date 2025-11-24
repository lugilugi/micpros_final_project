#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

// ===================== CONFIG =====================
#define I2C_ADDR 0x66
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define CMD_WHOAMI          0x01
#define CMD_GET_STOCK       0x02
#define CMD_UPDATE_DISPLAY  0x03
#define CMD_DISPENSE        0x10
#define CMD_ACK_SUCCESS     0x55
#define CMD_ACK_ERROR       0xEE

#define MAX_NAME_LEN 20

// EEPROM layout
struct SavedData {
  char name[MAX_NAME_LEN + 1];
  uint16_t stock;
  uint8_t version;
};

const char moduleUID[] = "PRD_MOD_01";

// ---------- Variables for safe ISR handling ----------
volatile uint8_t lastCommand = 0;
volatile bool requestPending = false;
volatile bool updateDisplayReceived = false;

// ACK flag: module will write this byte on next onRequest after receiving an update
volatile bool ackPending = false;
volatile uint8_t ackValue = 0;

char recvName[MAX_NAME_LEN + 1];
uint8_t recvNameLen = 0;
int recvStockValue = 0;

// In-memory authoritative copy (kept in EEPROM as well)
SavedData saved;

// ===================== I2C HANDLERS =====================
void handleWhoAmI() {
  Wire.write((const uint8_t*)moduleUID, sizeof(moduleUID)); // includes final '\0'
}

void handleGetStock() {
  uint16_t s = saved.stock;
  Wire.write((uint8_t)(s & 0xFF));
  Wire.write((uint8_t)((s >> 8) & 0xFF));
}

void handleDispense() {
  if ((int)saved.stock <= 0) {
    Wire.write(CMD_ACK_ERROR);
  } else {
    // Decrement local stock and persist
    saved.stock = (uint16_t)((int)saved.stock - 1);
    if ((int)saved.stock < 0) saved.stock = 0;
    EEPROM.put(0, saved);
    EEPROM.commit();
    Wire.write(CMD_ACK_SUCCESS);
    // Optionally: notify controller via master-read flow or let controller poll
  }
}

// ---------- READ DATA DURING onReceive (ISR-context) ----------
void onReceive(int byteCount) {
  if (byteCount < 1) return;

  lastCommand = Wire.read();
  byteCount--;

  if (lastCommand == CMD_UPDATE_DISPLAY && byteCount >= 3) {
    recvNameLen = Wire.read();
    byteCount--;

    if (recvNameLen > MAX_NAME_LEN) recvNameLen = MAX_NAME_LEN;
    for (int i = 0; i < recvNameLen && byteCount > 0; i++) {
      recvName[i] = Wire.read();
      byteCount--;
    }
    recvName[recvNameLen] = '\0';

    if (byteCount >= 2) {
      uint8_t lo = Wire.read();
      uint8_t hi = Wire.read();
      recvStockValue = (hi << 8) | lo;
      updateDisplayReceived = true; // main loop will apply
      // Prepare an ACK to be sent on the next onRequest
      ackPending = true;
      ackValue = CMD_ACK_SUCCESS;
    }
  }

  requestPending = true; // signal main loop to handle non-ISR work / logging
}

// ---------- RESPOND BASED ON lastCommand (ISR-context) ----------
void onRequest() {
  switch (lastCommand) {
    case CMD_WHOAMI:    handleWhoAmI(); break;
    case CMD_GET_STOCK: handleGetStock(); break;
    case CMD_DISPENSE:  handleDispense(); break;
    case CMD_UPDATE_DISPLAY:
      // If an ACK is pending (controller wrote an update) respond with ACK
      if (ackPending) {
        Wire.write(ackValue);
        ackPending = false;
      } else {
        // No ack pending: write success by default
        Wire.write(CMD_ACK_SUCCESS);
      }
      break;
    default: break;
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 1000) { /* wait up to 1s for serial */ }

  // EEPROM init (ESP32)
  EEPROM.begin(512);
  // Load saved data if present; default otherwise
  EEPROM.get(0, saved);
  if (saved.version == 0xFF || saved.version == 0x00) {
    // uninitialized
    memset(saved.name, 0, sizeof(saved.name));
    strncpy(saved.name, "No Product", MAX_NAME_LEN);
    saved.stock = 0;
    saved.version = 1;
    EEPROM.put(0, saved);
    EEPROM.commit();
  }

  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.begin(I2C_ADDR);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  Serial.print("I2C slave started at 0x");
  Serial.println(I2C_ADDR, HEX);
}

// ===================== LOOP =====================
void loop() {
  // Print debug info (only from main loop, not from ISR)
  if (requestPending) {
    requestPending = false;
    Serial.print("onReceive: lastCmd=0x");
    Serial.println(lastCommand, HEX);
  }

  if (updateDisplayReceived) {
    // Apply update: save to EEPROM and update in-memory
    updateDisplayReceived = false;
    strncpy(saved.name, recvName, MAX_NAME_LEN);
    saved.name[MAX_NAME_LEN] = '\0';
    saved.stock = (uint16_t)recvStockValue;
    saved.version++;
    EEPROM.put(0, saved);
    EEPROM.commit();

    Serial.print("UPDATE_DISPLAY applied: name=\"");
    Serial.print(saved.name);
    Serial.print("\" stock=");
    Serial.println(saved.stock);
  }

  delay(10);
}
