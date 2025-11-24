#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===================== WIFI CREDENTIALS ================================
#define WIFI_SSID "hello"
#define WIFI_PASS "aaaaaaaal"

// ===================== GOOGLE SHEETS API ===============================
#define APPS_SCRIPT_URL "https://script.google.com/macros/s/AKfycbz0ZfBuJ25KbkwfKOymWLuGpaHEPXwZzLGQaCfiJ1z5otdSJXWjeuWJxd5J-90BLL2_/exec"

// ===================== SERVICE ACCOUNT CREDENTIALS ====================
// Follow Random Nerd Tutorials: service-account-based Google Sheets access
#define PROJECT_ID     "micprosproject"
#define CLIENT_EMAIL   "micprosproject@micprosproject.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM = R"KEY(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQD1kUUVhZHbF2hx
/b5VeKG0sA5SFWGcSbWB6HOhH/igUnC8E5JGFdH2wFJ2msYwyS06w+ikTl9pam37
MapeOwnKnTVTyaA1TCC7IaNSwoYo8J23QI8zwPvPFZtQIRshggkxdL9A7sc+hPvP
beNPdHopCaqZG5aUnzXuJmz5fuupDxeM9buxww7R0koiDwDZFq1FF6uQNQbeWdJ0
0IptCftapS6WJ/F8zpXyGDgNlal4sQWbmT8GUO0PiR8yr7plilPYc3Z+mNRygkvV
LRpQH4iWyhtHxI4LF1B3YeYgI4XoecGkKqck5rJDwcyUiNtAATX2NsTfQZJQaKKl
OUOaU4nTAgMBAAECggEAQG4EDTiFY1GJ8tabmLtU2h6TM1Au0x23xMTjibPkvNPE
hmQwLblN5IrjWAEV3Pj7p/58zJdPi04EWzLVu3GMCSAkPL6bDUDTGaYivvUQ1C8F
gm4q5G5O4y+NF4IIJ0uB8/rorzW3Cx1DIFJ5oIA0CQ5jN1a4tHftY3Wrg+6cEDfE
6KAjy56Wqpwn++opL/EFl3aPKKWaNRkfFarlomNRhP00pU+0X5JdjyXVULSjmaAs
Ucp/DpxY1odL/HY70OXGay7idhwm3ji9dfH+RG/otUoAQPGw3uhZ5IAKP7YuwERA
XkvgsWe6VsxK4HtdF7fJ2fk0SJuhDUj3lZNu8C+zyQKBgQD8EBZKyNdbBQuzqtXY
e49GTSdy2ig1tPSu4dPxODX5Fn7NJtjQDTN2btlD3fZ7IYi8uduEUsN/ookhgUvS
u40bQ4xLEuTisftcS17FFLN9MOsRylmH4A8AOllQJxLz8tfu4Ju3jsMzb4I9B/T8
nicVYjvvpXCEC5WPwyUZqZhExQKBgQD5ZzXBdTCwdkdgG7+6rv5CrdpqidI8F5WF
u/V6PCdeUH8HjEap/Cr7/z4c/HPHHh/nTlopCP0roykiRsYOxeFrmoazSgRa4spS
4ETeUJro8ayiCJ9E3h4VLgMzOWO6WBY1Z2mcmKYCA8bDkkLij7wtzVuG5xH1Mtq2
mVITWJDttwKBgHTYUD0ilRIQaLhEvRS5UlVYdqz7DCC5XaOj88eiMwLgtq8LDv6C
4BRKllSrlBLIHMa/sU7Jvu5vvfnWIfvyDRtSWLqEa63aq5bBKZFaY0npX07D6nTe
HJYSdkx9kH+dVxPY8tZIS5yQWGNKSPrBpYR4ISiaHGpZpF8cKxqWT4uxAoGAEVxf
35GazA3Pth74X7Riup2DgLsLSWeS3vZQhiu9ydDspsfa+2Y0T8patoXUQV4VdnJ7
0DNx/CGlcV9f1hNsN6NQERbr6q+yycYWxSrzPZflHnpfK9oSWgMT8fLiwEv1b849
CcuOcsF0ipSZ10+OF9odruxS0bCyjNdrYTFfFU0CgYByw8qQl3jSm73WHKyQju/S
MJ60RBbImHdUst5ylMuUnYQdLf38ofdHgL0dPygiSNzbuOh3a+JOqS03S+dXbqXi
TsNjZlQ31IPKmf8w9y9NASG+jFcRYNh1VTVG7I6vaSVcT85VJhTOSiSaY5xJ1skj
MuJnall1AfC/mCduWLxDcA==
-----END PRIVATE KEY-----
)KEY";

const char spreadsheetId[] = "1fR9fOknNBAp4gqQ_3jFW9tzgvwDybs3g5iWqzlVjvp4";
// ===================== I2C CONFIGURATION ==============================
#define I2C_MIN_ADDR 0x08 // for Product Modules
#define I2C_MAX_ADDR 0x77

// ===================== LCD DISPLAY ====================================
#define LCD_I2C_ADDR    0x27 
#define LCD_COLS        20
#define LCD_ROWS        4

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
#define PAYMENT_TIMEOUT_MS      30000    // Confirmation wait timeout
#define OOS_TIMEOUT_MS          3000         // Out of stock display time
#define CANCEL_TIMEOUT_MS       3000      // Cancel message display time
#define ERROR_TIMEOUT_MS        5000       // Error message display time
#define SYNC_INTERVAL_MS        30000      // Periodic sync interval
#define I2C_RESPONSE_TIMEOUT    5000   // I2C response timeout

// ===================== I2C PROTOCOL COMMANDS ==========================
#define CMD_WHOAMI              0x01  // Get module identity
#define CMD_GET_STOCK           0x02  // Query stock level
#define CMD_UPDATE_DISPLAY      0x03  // Update OLED display
#define CMD_DISPENSE            0x10  // Dispense itemb
#define CMD_ACK_SUCCESS         0x55  // Success acknowledgment
#define CMD_ACK_ERROR           0xEE  // Error acknowledgment

#endif // CONFIG_H
