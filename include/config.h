#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===================== WIFI CREDENTIALS ================================
#define WIFI_SSID "SEMShelter"
#define WIFI_PASS "SEM2010!"

// ===================== GOOGLE SHEETS API ===============================
#define APPS_SCRIPT_URL "https://script.google.com/macros/s/AKfycbz0ZfBuJ25KbkwfKOymWLuGpaHEPXwZzLGQaCfiJ1z5otdSJXWjeuWJxd5J-90BLL2_/exec"

// ===================== I2C CONFIGURATION ==============================
#define I2C_MIN_ADDR 0x08
#define I2C_MAX_ADDR 0x77
#define PRODUCT_MODULE_BASE_ADDR 0x10

// ===================== LCD DISPLAY ====================================
#define LCD_I2C_ADDR 0x27  // Common I2C LCD address
#define LCD_COLS 20
#define LCD_ROWS 4

// ===================== KEYPAD CONFIGURATION ===========================
#define ROWS 4
#define COLS 4

// ===================== I2C PINS =======================================
#define I2C_SDA 21
#define I2C_SCL 22

// Row/column pin arrays (defined in one .cpp file to avoid multiple definitions)
extern byte rowPins[ROWS];
extern byte colPins[COLS];

// ===================== TIMING CONSTANTS ==============================
#define PAYMENT_TIMEOUT_MS 30000    // Confirmation wait timeout
#define OOS_TIMEOUT_MS 3000         // Out of stock display time
#define CANCEL_TIMEOUT_MS 3000      // Cancel message display time
#define ERROR_TIMEOUT_MS 5000       // Error message display time
#define SYNC_INTERVAL_MS 30000      // Periodic sync interval
#define I2C_RESPONSE_TIMEOUT 5000   // I2C response timeout

// ===================== I2C PROTOCOL COMMANDS ==========================
#define CMD_WHOAMI          0x01  // Get module identity
#define CMD_GET_STOCK       0x02  // Query stock level
#define CMD_UPDATE_DISPLAY  0x03  // Update OLED display
#define CMD_DISPENSE        0x10  // Dispense itemb
#define CMD_ACK_SUCCESS     0x55  // Success acknowledgment
#define CMD_ACK_ERROR       0xEE  // Error acknowledgment

#endif // CONFIG_H
