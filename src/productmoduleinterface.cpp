#include "productmoduleinterface.h"
#include "googlesheets.h"

// Retry/ACK configuration for I2C reliability
static const int I2C_MAX_RETRIES = 3;
static const int I2C_RETRY_DELAY_MS = 100;

// ===================== I2C PROTOCOL IMPLEMENTATION ======================

bool i2c_whoami(uint8_t addr, String &moduleUID) {
  moduleUID = "";
  for (int attempt = 0; attempt < I2C_MAX_RETRIES; ++attempt) {
    Wire.beginTransmission(addr);
    Wire.write(CMD_WHOAMI);
    int tx = Wire.endTransmission();
    if (tx != 0) {
      // transmission failed; retry
      if (attempt < I2C_MAX_RETRIES - 1) delay(I2C_RETRY_DELAY_MS);
      continue;
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

    if (moduleUID.length() > 0) {
      return true;
    }

    // no response, retry
    if (attempt < I2C_MAX_RETRIES - 1) delay(I2C_RETRY_DELAY_MS);
  }

  g_registry.logError(ERR_I2C_COMM, "WHOAMI failed after retries", String(addr));
  return false;
}

bool i2c_getStock(uint8_t addr, int &stock) {
  for (int attempt = 0; attempt < I2C_MAX_RETRIES; ++attempt) {
    Wire.beginTransmission(addr);
    Wire.write(CMD_GET_STOCK);
    int tx = Wire.endTransmission();
    if (tx != 0) {
      if (attempt < I2C_MAX_RETRIES - 1) delay(I2C_RETRY_DELAY_MS);
      continue;
    }

    delay(10);
    Wire.requestFrom((int)addr, 2);
    if (Wire.available() < 2) {
      if (attempt < I2C_MAX_RETRIES - 1) {
        delay(I2C_RETRY_DELAY_MS);
        continue;
      }
      g_registry.logError(ERR_I2C_COMM, "GET_STOCK response incomplete", String(addr));
      return false;
    }

    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    stock = (int)hi << 8 | lo;
    return true;
  }

  g_registry.logError(ERR_I2C_COMM, "GET_STOCK failed after retries", String(addr));
  return false;
}

bool i2c_updateDisplay(uint8_t addr, const String& name, int stock) {
  // UPDATE_DISPLAY currently does not have an ACK from module; retry on transmission error
  for (int attempt = 0; attempt < I2C_MAX_RETRIES; ++attempt) {
    Wire.beginTransmission(addr);
    Wire.write(CMD_UPDATE_DISPLAY);
    uint8_t len = (uint8_t)name.length();
    if (len > 20) len = 20;
    Wire.write(len);

    for (int i = 0; i < len; i++) {
      Wire.write(name[i]);
    }

    Wire.write((uint8_t)(stock & 0xFF));
    Wire.write((uint8_t)((stock >> 8) & 0xFF));

    int tx = Wire.endTransmission();
    if (tx == 0) {
      // After sending the update, request an ACK byte from the module
      delay(10);
      unsigned long t0 = millis();
      while (millis() - t0 < I2C_RESPONSE_TIMEOUT) {
        Wire.requestFrom((int)addr, 1);
        if (Wire.available()) {
          uint8_t ack = Wire.read();
          if (ack == CMD_ACK_SUCCESS) return true;
          // module explicitly returned error
          g_registry.logError(ERR_I2C_COMM, "UPDATE_DISPLAY module NACK", String(addr));
          return false;
        }
        delay(10);
      }
      // No ACK received within timeout; treat as failure for this attempt
    }

    if (attempt < I2C_MAX_RETRIES - 1) delay(I2C_RETRY_DELAY_MS);
  }

  g_registry.logError(ERR_I2C_COMM, "UPDATE_DISPLAY failed after retries", String(addr));
  return false;
}

bool i2c_dispense(uint8_t addr) {
  // Perform up to N attempts to request a dispense and receive an ACK
  for (int attempt = 0; attempt < I2C_MAX_RETRIES; ++attempt) {
    Wire.beginTransmission(addr);
    Wire.write(CMD_DISPENSE);
    int tx = Wire.endTransmission();
    if (tx != 0) {
      // transmission failed, retry
      if (attempt < I2C_MAX_RETRIES - 1) delay(I2C_RETRY_DELAY_MS);
      continue;
    }

    // Wait for acknowledgment with timeout
    unsigned long start = millis();
    while (millis() - start < I2C_RESPONSE_TIMEOUT) {
      Wire.requestFrom((int)addr, 1);
      if (Wire.available()) {
        uint8_t ack = Wire.read();
        if (ack == CMD_ACK_SUCCESS) {
          // Module reports successful dispense and is expected to have
          // decremented its local stock. Controller now decrements the
          // authoritative stock in Google Sheets and logs a transaction.
          ProductModule* mod = g_registry.findModuleByAddress(addr);
          if (mod && mod->itemCode.length() > 0) {
            ProductItem* p = g_registry.findProduct(mod->itemCode);
            if (p) {
              int newStock = p->stock - 1;
              if (newStock < 0) newStock = 0;
              // update local cache and Sheets
              p->stock = newStock;
              g_registry.updateModuleStock(addr, newStock);
              updateStockInSheets(p->itemCode, newStock);
              logTransactionToSheets(p->itemCode, 1);
              // Optionally push updated display back to module
              i2c_updateDisplay(addr, p->name, newStock);
            }
          }
          return true;
        }
        if (ack == CMD_ACK_ERROR) {
          g_registry.logError(ERR_DISPENSE_FAILED, "Module reported error", String(addr));
          return false;
        }
      }
      delay(50);
    }

    // ACK timeout for this attempt; retry if attempts remain
    if (attempt < I2C_MAX_RETRIES - 1) {
      delay(I2C_RETRY_DELAY_MS);
      continue;
    }
  }

  g_registry.logError(ERR_APP_TIMEOUT, "Dispense ACK timeout after retries", String(addr));
  return false;
}

// ===================== MODULE DISCOVERY ==============================

void discoverProductModules() {
  Serial.println("Scanning I2C bus for product modules...");

  // Ensure we have the latest product & module mapping from Sheets
  syncProductDataFromSheets();

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

        // Check registry (which was seeded from Sheets) for this UID
        ProductModule* sheetModule = g_registry.findModuleByUID(moduleUID);
        if (!sheetModule) {
          // Not present in Sheets: add and register so operator can assign product later
          Serial.println("  Module UID not found in Sheets; registering new module");
          g_registry.addModule(addr, moduleUID, "", "New Module", 0);
          registerNewModuleToSheets(moduleUID, addr);
        } else {
          // Ensure registry reflects the currently-scanned I2C address
          sheetModule->i2cAddress = addr;
          sheetModule->online = true;
          sheetModule->lastSeen = millis();

          // If a product code is assigned in Modules sheet, push the product
          if (sheetModule->itemCode.length() > 0) {
            ProductItem* prod = g_registry.findProduct(sheetModule->itemCode);
            if (prod) {
              // Send product name and stock to module using existing helper
              i2c_updateDisplay(addr, prod->name, prod->stock);
              // Update the module entry with authoritative values
              g_registry.addModule(addr, moduleUID, prod->itemCode, prod->name, prod->stock);
            } else {
              Serial.println("  Product code assigned to module not found in Products sheet");
              g_registry.logError(ERR_INVALID_PRODUCT, "Product code not found in Products sheet", sheetModule->itemCode);
            }
          } else {
            Serial.println("  Module has no product code assigned in Sheets");
          }
        }
        g_registry.updateModuleHealth(addr, true);
      } else {
        g_registry.updateModuleHealth(addr, false);
      }
    }
  }

  // After scanning and updating modules, attempt a local reconcile
  matchModulesToSheets();
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
