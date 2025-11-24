#ifndef DATATYPES_H
#define DATATYPES_H

#include <Arduino.h>
#include <vector>

// ===================== ERROR CODES =======================================

enum ErrorCode {
  ERR_NONE =                0,
  ERR_MODULE_OFFLINE =      1,       // Product module not responding
  ERR_I2C_COMM =            2,       // I2C communication failure
  ERR_DISPENSE_FAILED =     3,       // Motor/dispense mechanism error
  ERR_MODULE_UID_MISMATCH = 4,       // Module UID doesn't match sheet
  ERR_STOCK_MISMATCH =      5,       // Stock count mismatch
  ERR_SHEETS_SYNC =         6,       // Google Sheets sync failed
  ERR_MODULE_DISCONNECTED = 7,       // Module was connected, now offline
  ERR_INVALID_PRODUCT =     8,       // Product code invalid
  ERR_APP_TIMEOUT =         9        // Critical operation timeout
};

// ===================== PRODUCT DATA STRUCTURES =======================

struct ProductItem {
  String itemCode;           // Unique product identifier
  String name;               // Product name
  int stock;                 // Current stock count
  int targetAmount;          // Amount to dispense (usually 1)
  bool available;            // Is product available for purchase
};

struct ProductModule {
  uint8_t i2cAddress;        // I2C address
  String moduleUID;          // Module's unique identifier (from module itself)
  String itemCode;           // Associated item code (from Google Sheets)
  String name;               // Product name
  int stock;                 // Current stock
  bool healthy;              // Module health status
  bool online;               // Currently reachable on I2C bus
  unsigned long lastSeen;    // Last successful communication
};

// ===================== TRANSACTION & ERROR LOGGING ======================

struct Transaction {
  String itemCode;
  int amountDispensed;
  unsigned long timestamp;
  bool successful;
};

struct ErrorLog {
  ErrorCode code;
  String message;
  unsigned long timestamp;
  String affectedItem;
};

// ===================== PRODUCT REGISTRY =============================

class ProductRegistry {
private:
  std::vector<ProductItem> products;
  std::vector<ProductModule> modules;
  std::vector<ErrorLog> errorLogs;
  
public:
  // Product management
  void addProduct(const String& code, const String& name, int stock, bool available = true);
  ProductItem* findProduct(const String& code);
  std::vector<ProductItem>& getProducts() { return products; }
  
  // Module management
  void addModule(uint8_t addr, const String& uid, const String& code, const String& name, int stock);
  void updateModuleStock(uint8_t addr, int stock);
  void updateModuleHealth(uint8_t addr, bool online);
  ProductModule* findModuleByCode(const String& code);
  ProductModule* findModuleByAddress(uint8_t addr);
  ProductModule* findModuleByUID(const String& uid);
  std::vector<ProductModule>& getModules() { return modules; }
  
  // Error logging
  void logError(ErrorCode code, const String& message, const String& affectedItem = "");
  std::vector<ErrorLog>& getErrorLogs() { return errorLogs; }
  
  // Sync operations
  void clearRegistry();
  bool validateProductExists(const String& code);

  // Debug helpers: print contents to Serial
  void debugPrintProducts();
  void debugPrintModules();
};

// ===================== GLOBAL REGISTRY ===============================

extern ProductRegistry g_registry;

#endif // DATATYPES_H
