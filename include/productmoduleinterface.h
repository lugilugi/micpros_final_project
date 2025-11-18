#ifndef PRODUCTMODULEINTERFACE_H
#define PRODUCTMODULEINTERFACE_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "datatypes.h"

// ===================== I2C DISCOVERY & COMMUNICATION ==================

// Send WHO_ARE_YOU command to module, get UID and item code
bool i2c_whoami(uint8_t addr, String &moduleUID);

// Query current stock from module
bool i2c_getStock(uint8_t addr, int &stock);

// Update OLED display on module with product info
bool i2c_updateDisplay(uint8_t addr, const String& name, int stock);

// Send dispense command and wait for acknowledgment
bool i2c_dispense(uint8_t addr);

// ===================== MODULE DISCOVERY & INITIALIZATION ============

// Full I2C bus scan and module discovery
void discoverProductModules();

// Attempt to match discovered modules with Google Sheets data
void matchModulesToSheets();

// Sync all module displays with current product data
void syncModuleDisplays();

// Check module health (online/offline status)
void checkModuleHealth();

// Get module by address
ProductModule* getModuleByAddress(uint8_t addr);

// Get module by item code
ProductModule* getModuleByCode(const String& code);

// Update module stock
void updateModuleStock(uint8_t addr, int newStock);

#endif // PRODUCTMODULEINTERFACE_H
