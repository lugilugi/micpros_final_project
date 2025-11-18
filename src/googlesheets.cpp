#include "googlesheets.h"
#include "config.h"
#include "datatypes.h"
#include <HTTPClient.h>

// ===================== WIFI CONNECTIVITY ============================

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.println("Attempting WiFi connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  
  while (millis() - start < 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected!");
      return;
    }
    delay(200);
  }
  
  Serial.println("WiFi connection failed");
  g_registry.logError(ERR_SHEETS_SYNC, "WiFi connection failed", "");
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ===================== DATABASE SYNCHRONIZATION ====================

void syncProductDataFromSheets() {
  // Fetch all product data from Google Sheets
  // Expected format: CODE1|NAME1|STOCK1|ADDR1;CODE2|NAME2|STOCK2|ADDR2;...
  
  ensureWiFi();
  if (!isWiFiConnected()) {
    g_registry.logError(ERR_SHEETS_SYNC, "WiFi not connected", "");
    return;
  }
  
  HTTPClient http;
  String url = String(APPS_SCRIPT_URL) + "?action=getAllProducts";
  
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure(); // TODO: Proper SSL verification in production
  
  http.begin(*client, url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Simple parsing: CODE|NAME|STOCK|ADDR separated by semicolons
    int pos = 0;
    while (pos < payload.length()) {
      int nextSemi = payload.indexOf(';', pos);
      if (nextSemi == -1) nextSemi = payload.length();
      
      String item = payload.substring(pos, nextSemi);
      
      // Parse: CODE|NAME|STOCK|ADDR
      int p1 = item.indexOf('|');
      int p2 = item.indexOf('|', p1 + 1);
      int p3 = item.indexOf('|', p2 + 1);
      
      if (p1 > 0 && p2 > p1 && p3 > p2) {
        String code = item.substring(0, p1);
        String name = item.substring(p1 + 1, p2);
        int stock = item.substring(p2 + 1, p3).toInt();
        uint8_t addr = (uint8_t)item.substring(p3 + 1).toInt();
        
        // Add to registry
        g_registry.addProduct(code, name, stock, true);
        
        // Find and update module
        ProductModule* mod = g_registry.findModuleByAddress(addr);
        if (mod) {
          mod->itemCode = code;
          mod->name = name;
          mod->stock = stock;
        }
      }
      
      pos = nextSemi + 1;
    }
    
    Serial.println("Product data synced from Google Sheets");
  } else {
    Serial.print("Google Sheets sync failed with code: ");
    Serial.println(httpCode);
    g_registry.logError(ERR_SHEETS_SYNC, "HTTP error", String(httpCode));
  }
  
  http.end();
  delete client;
}

void logTransactionToSheets(const String& itemCode, int amount) {
  // Log a transaction to Google Sheets
  
  ensureWiFi();
  if (!isWiFiConnected()) return;
  
  HTTPClient http;
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();
  
  http.begin(*client, APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"action\":\"logTransaction\",";
  payload += "\"item\":\"" + itemCode + "\",";
  payload += "\"amount\":" + String(amount) + ",";
  payload += "\"timestamp\":" + String(millis()) + "}";
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("Transaction logged to Google Sheets");
  } else {
    Serial.print("Transaction logging failed: ");
    Serial.println(httpCode);
  }
  
  http.end();
  delete client;
}

void updateStockInSheets(const String& itemCode, int newStock) {
  // Update stock count in Google Sheets
  
  ensureWiFi();
  if (!isWiFiConnected()) return;
  
  HTTPClient http;
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();
  
  http.begin(*client, APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"action\":\"updateStock\",";
  payload += "\"item\":\"" + itemCode + "\",";
  payload += "\"stock\":" + String(newStock) + "}";
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("Stock updated in Google Sheets");
  }
  
  http.end();
  delete client;
}

void logErrorToSheets(const String& errorMsg, const String& errorDetails) {
  // Log error to Google Sheets for remote diagnostics
  
  ensureWiFi();
  if (!isWiFiConnected()) return;
  
  HTTPClient http;
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();
  
  http.begin(*client, APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"action\":\"logError\",";
  payload += "\"message\":\"" + errorMsg + "\",";
  payload += "\"details\":\"" + errorDetails + "\",";
  payload += "\"timestamp\":" + String(millis()) + "}";
  
  int httpCode = http.POST(payload);
  (void)httpCode; // Suppress warning
  
  http.end();
  delete client;
}

void registerNewModuleToSheets(const String& moduleUID, uint8_t i2cAddress) {
  // Register newly discovered module to Google Sheets
  
  ensureWiFi();
  if (!isWiFiConnected()) return;
  
  HTTPClient http;
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();
  
  http.begin(*client, APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"action\":\"registerModule\",";
  payload += "\"uid\":\"" + moduleUID + "\",";
  payload += "\"address\":\"" + String(i2cAddress, HEX) + "\"}";
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("New module registered to Google Sheets");
  }
  
  http.end();
  delete client;
}
