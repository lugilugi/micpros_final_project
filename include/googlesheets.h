#ifndef GOOGLESHEETS_H
#define GOOGLESHEETS_H

#include <Arduino.h>
#include <WiFi.h>

// ===================== DATABASE SYNCHRONIZATION =======================

// Fetch all product data from Google Sheets
void syncProductDataFromSheets();

// Update stock count in Google Sheets after dispensing
void updateStockInSheets(const String& itemCode, int newStock);

// Log a transaction to Google Sheets
void logTransactionToSheets(const String& itemCode, int amount);

// Log error to Google Sheets for remote tracking
void logErrorToSheets(const String& errorMsg, const String& errorDetails);

// Register new product module to Google Sheets
void registerNewModuleToSheets(const String& moduleUID, uint8_t i2cAddress);

// Check if a module UID is already registered in the Modules sheet.
// If found, `outAddress` is set to the stored address (0 if missing) and
// the function returns true. Returns false if not found or on error.
bool isModuleRegistered(const String& moduleUID, uint8_t &outAddress);
// ===================== WIFI CONNECTIVITY ============================

// Ensure WiFi connection is active
void ensureWiFi();

// Get current WiFi connection status
bool isWiFiConnected();

#endif // GOOGLESHEETS_H
