#include "productmoduleinterface.h"
#include "googlesheets.h"

// ===================== I2C PROTOCOL IMPLEMENTATION ======================

bool i2c_whoami(uint8_t addr, String &moduleUID) {
  Wire.beginTransmission(addr);
  Wire.write(CMD_WHOAMI);
  if (Wire.endTransmission() != 0) {
    g_registry.logError(ERR_I2C_COMM, "WHOAMI transmission failed", String(addr));
    return false;
  }
  
  delay(10);
  Wire.requestFrom((int)addr, 32);
  
  moduleUID = "";
  while (Wire.available()) {
    char c = Wire.read();
    if (c == 0) break;
    moduleUID += c;
  }
  moduleUID.trim();
  
  if (moduleUID.length() == 0) {
    g_registry.logError(ERR_I2C_COMM, "WHOAMI no response", String(addr));
    return false;
  }
  
  return true;
}

bool i2c_getStock(uint8_t addr, int &stock) {
  Wire.beginTransmission(addr);
  Wire.write(CMD_GET_STOCK);
  if (Wire.endTransmission() != 0) {
    g_registry.logError(ERR_I2C_COMM, "GET_STOCK transmission failed", String(addr));
    return false;
  }
  
  delay(10);
  Wire.requestFrom((int)addr, 2);
  if (Wire.available() < 2) {
    g_registry.logError(ERR_I2C_COMM, "GET_STOCK response incomplete", String(addr));
    return false;
  }
  
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  stock = (int)hi << 8 | lo;
  return true;
}

bool i2c_updateDisplay(uint8_t addr, const String& name, int stock) {
  Wire.beginTransmission(addr);
  Wire.write(CMD_UPDATE_DISPLAY);
  Wire.write((uint8_t)name.length());
  
  for (int i = 0; i < name.length() && i < 20; i++) {
    Wire.write(name[i]);
  }
  
  Wire.write((uint8_t)(stock & 0xFF));
  Wire.write((uint8_t)((stock >> 8) & 0xFF));
  
  if (Wire.endTransmission() != 0) {
    g_registry.logError(ERR_I2C_COMM, "UPDATE_DISPLAY failed", String(addr));
    return false;
  }
  
  delay(10);
  return true;
}

bool i2c_dispense(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(CMD_DISPENSE);
  if (Wire.endTransmission() != 0) {
    g_registry.logError(ERR_I2C_COMM, "DISPENSE transmission failed", String(addr));
    return false;
  }
  
  // Wait for acknowledgment with timeout
  unsigned long start = millis();
  while (millis() - start < I2C_RESPONSE_TIMEOUT) {
    Wire.requestFrom((int)addr, 1);
    if (Wire.available()) {
      uint8_t ack = Wire.read();
      if (ack == CMD_ACK_SUCCESS) {
        return true;
      }
      if (ack == CMD_ACK_ERROR) {
        g_registry.logError(ERR_DISPENSE_FAILED, "Module reported error", String(addr));
        return false;
      }
    }
    delay(50);
  }
  
  g_registry.logError(ERR_APP_TIMEOUT, "Dispense ACK timeout", String(addr));
  return false;
}

// ===================== MODULE DISCOVERY ==============================

void discoverProductModules() {
  g_registry.clearRegistry();

  Serial.println("Scanning I2C bus for product modules...");

  for (uint8_t addr = I2C_MIN_ADDR; addr <= I2C_MAX_ADDR; ++addr) {
    if (addr == 0x27) continue; // Skip LCD address

    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    
    if (err == 0) {
      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
      
      String moduleUID;
      if (i2c_whoami(addr, moduleUID)) {
        Serial.print("  Module UID: ");
        Serial.println(moduleUID);
        
        int stock = 0;
        i2c_getStock(addr, stock);
        
        // Check Google Sheets to see if this module UID is already registered
        uint8_t sheetAddr = 0;
        bool registered = isModuleRegistered(moduleUID, sheetAddr);

        if (!registered) {
          // Not present in Sheets: add and register
          g_registry.addModule(addr, moduleUID, "NEW_MODULE", "New Module", stock);
          registerNewModuleToSheets(moduleUID, addr);
        } else {
          // Already registered in Sheets: add to registry. Product matching
          // to sheet data happens elsewhere (sync/match routines).
          g_registry.addModule(addr, moduleUID, "REGISTERED", "Registered Module", stock);
        }

        g_registry.updateModuleHealth(addr, true);
      } else {
        g_registry.updateModuleHealth(addr, false);
      }
    }
  }

  // After scanning and populating the module list, pull the latest product
  // data from Google Sheets and let the sync routine match products to
  // discovered modules by I2C address.
  syncProductDataFromSheets();
  // Do a best-effort local match (if product codes were set)
  matchModulesToSheets();
  // For debugging, print discovered modules
  g_registry.debugPrintModules();
  
  Serial.println("Module discovery complete");
}

void matchModulesToSheets() {
  // After `syncProductDataFromSheets()` has run, modules whose I2C address
  // matched a row in the Products sheet will already have `itemCode`/`name`
  // populated. This helper performs a best-effort local reconcile: if a
  // module already has an `itemCode`, ensure the module's name/stock mirror
  // the registered product data in the local registry.
  for (auto& module : g_registry.getModules()) {
    if (module.itemCode.length() == 0) continue;
    ProductItem* product = g_registry.findProduct(module.itemCode);
    if (product) {
      module.name = product->name;
      module.stock = product->stock;
    }
  }
}

void syncModuleDisplays() {
  // Update all module OLEDs with current product data
  for (auto& module : g_registry.getModules()) {
    if (module.online && !module.itemCode.startsWith("NEW")) {
      i2c_updateDisplay(module.i2cAddress, module.name, module.stock);
    }
  }
}

void checkModuleHealth() {
  // Poll all known modules to verify they're still online
  for (auto& module : g_registry.getModules()) {
    int stock;
    bool ok = i2c_getStock(module.i2cAddress, stock);
    g_registry.updateModuleHealth(module.i2cAddress, ok);
    
    if (ok) {
      g_registry.updateModuleStock(module.i2cAddress, stock);
    }
  }
}

ProductModule* getModuleByAddress(uint8_t addr) {
  return g_registry.findModuleByAddress(addr);
}

ProductModule* getModuleByCode(const String& code) {
  return g_registry.findModuleByCode(code);
}

void updateModuleStock(uint8_t addr, int newStock) {
  g_registry.updateModuleStock(addr, newStock);
}
