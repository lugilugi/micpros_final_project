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
      m.moduleUID =     uid;
      m.itemCode =      code;
      m.name =          name;
      m.stock =         stock;
      m.lastSeen =      millis();
      m.online =        true;
      return;
    }
  }
  
  ProductModule module;
  module.i2cAddress =   addr;
  module.moduleUID =    uid;
  module.itemCode =     code;
  module.name =         name;
  module.stock =        stock;
  module.healthy =      true;
  module.online =       true;
  module.lastSeen =     millis();
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
  err.code =          code;
  err.message =       message;
  err.timestamp =     millis();
  err.affectedItem =  affectedItem;
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

// ===================== DEBUG PRINT HELPERS ===========================

void ProductRegistry::debugPrintProducts() {
  Serial.println("--- Product Registry: Products ---");
  if (products.empty()) {
    Serial.println("(no products)");
    return;
  }
  for (size_t i = 0; i < products.size(); ++i) {
    auto &p = products[i];
    Serial.print("["); Serial.print(i); Serial.print("] ");
    Serial.print("code="); Serial.print(p.itemCode);
    Serial.print(" name="); Serial.print(p.name);
    Serial.print(" stock="); Serial.print(p.stock);
    Serial.print(" target="); Serial.print(p.targetAmount);
    Serial.print(" available="); Serial.println(p.available ? "true" : "false");
  }
}

void ProductRegistry::debugPrintModules() {
  Serial.println("--- Product Registry: Modules ---");
  if (modules.empty()) {
    Serial.println("(no modules)");
    return;
  }
  for (size_t i = 0; i < modules.size(); ++i) {
    auto &m = modules[i];
    Serial.print("["); Serial.print(i); Serial.print("] ");
    Serial.print("addr=0x"); Serial.print(m.i2cAddress, HEX);
    Serial.print(" (dec="); Serial.print((int)m.i2cAddress); Serial.print(")");
    Serial.print(" uid="); Serial.print(m.moduleUID);
    Serial.print(" code="); Serial.print(m.itemCode);
    Serial.print(" name="); Serial.print(m.name);
    Serial.print(" stock="); Serial.print(m.stock);
    Serial.print(" healthy="); Serial.print(m.healthy ? "true" : "false");
    Serial.print(" online="); Serial.print(m.online ? "true" : "false");
    Serial.print(" lastSeen="); Serial.println(m.lastSeen);
  }
}
