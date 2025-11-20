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
        
        // Add module with placeholder data
        // Will be matched with Google Sheets data later
        g_registry.addModule(addr, moduleUID, "NEW_MODULE", "New Module", stock);
        g_registry.updateModuleHealth(addr, true);
      } else {
        g_registry.updateModuleHealth(addr, false);
      }
    }
  }
  
  Serial.println("Module discovery complete");
}

void matchModulesToSheets() {
  // Match discovered modules with Google Sheets data based on:
  // 1. Module UID match (primary)
  // 2. I2C address match (fallback)
  
  for (auto& module : g_registry.getModules()) {
    ProductItem* product = g_registry.findProduct(module.moduleUID);
    if (product) {
      module.itemCode = product->itemCode;
      module.name = product->name;
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
