#include "datatypes.h"

// Global product registry instance
ProductRegistry g_registry;

// ===================== PRODUCT MANAGEMENT =============================

void ProductRegistry::addProduct(const String& code, const String& name, int stock, bool available) {
  // Avoid duplicates
  for (auto& p : products) {
    if (p.itemCode == code) {
      p.name = name;
      p.stock = stock;
      p.available = available;
      return;
    }
  }
  
  ProductItem item;
  item.itemCode = code;
  item.name = name;
  item.stock = stock;
  item.targetAmount = 1;
  item.available = available;
  products.push_back(item);
}

ProductItem* ProductRegistry::findProduct(const String& code) {
  for (auto& p : products) {
    if (p.itemCode == code) return &p;
  }
  return nullptr;
}

// ===================== MODULE MANAGEMENT ==========================

void ProductRegistry::addModule(uint8_t addr, const String& uid, const String& code, const String& name, int stock) {
  // Avoid duplicates
  for (auto& m : modules) {
    if (m.i2cAddress == addr) {
      m.moduleUID = uid;
      m.itemCode = code;
      m.name = name;
      m.stock = stock;
      m.lastSeen = millis();
      m.online = true;
      return;
    }
  }
  
  ProductModule module;
  module.i2cAddress = addr;
  module.moduleUID = uid;
  module.itemCode = code;
  module.name = name;
  module.stock = stock;
  module.healthy = true;
  module.online = true;
  module.lastSeen = millis();
  modules.push_back(module);
}

void ProductRegistry::updateModuleStock(uint8_t addr, int stock) {
  for (auto& m : modules) {
    if (m.i2cAddress == addr) {
      m.stock = stock;
      m.lastSeen = millis();
      return;
    }
  }
}

void ProductRegistry::updateModuleHealth(uint8_t addr, bool online) {
  for (auto& m : modules) {
    if (m.i2cAddress == addr) {
      m.online = online;
      if (online) {
        m.lastSeen = millis();
      }
      return;
    }
  }
}

ProductModule* ProductRegistry::findModuleByCode(const String& code) {
  for (auto& m : modules) {
    if (m.itemCode == code) return &m;
  }
  return nullptr;
}

ProductModule* ProductRegistry::findModuleByAddress(uint8_t addr) {
  for (auto& m : modules) {
    if (m.i2cAddress == addr) return &m;
  }
  return nullptr;
}

ProductModule* ProductRegistry::findModuleByUID(const String& uid) {
  for (auto& m : modules) {
    if (m.moduleUID == uid) return &m;
  }
  return nullptr;
}

// ===================== ERROR LOGGING ================================

void ProductRegistry::logError(ErrorCode code, const String& message, const String& affectedItem) {
  ErrorLog err;
  err.code = code;
  err.message = message;
  err.timestamp = millis();
  err.affectedItem = affectedItem;
  errorLogs.push_back(err);
  
  // Keep only last 50 errors
  if (errorLogs.size() > 50) {
    errorLogs.erase(errorLogs.begin());
  }
}

// ===================== REGISTRY OPERATIONS ===========================

void ProductRegistry::clearRegistry() {
  products.clear();
  modules.clear();
}

bool ProductRegistry::validateProductExists(const String& code) {
  return findProduct(code) != nullptr;
}
