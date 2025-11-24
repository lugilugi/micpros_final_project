#include "googlesheets.h"
#include "config.h"
#include "datatypes.h"
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include "time.h"
#include <vector>

// Convenience alias for the library instance
// The library provides a global `GSheet` instance (see examples)

// Helper: parse the JSON string returned by GSheet.values.get(String*) for "values" array
static void parseValuesJson(const String &json, std::vector<std::vector<String>> &outRows)
{
  outRows.clear();
  int idx = json.indexOf("\"values\":");
  if (idx < 0) return;

  // find opening '[' of the values array
  int start = json.indexOf('[', idx);
  if (start < 0) return;

  // find matching closing bracket for the values array
  int depth = 0;
  int end = -1;
  for (int i = start; i < (int)json.length(); ++i) {
    char c = json[i];
    if (c == '[') ++depth;
    else if (c == ']') {
      --depth;
      if (depth == 0) { end = i; break; }
    }
  }
  if (end <= start) return;

  String body = json.substring(start + 1, end); // content between outer [ ... ]

  // Each row looks like ["A","B","C"] and rows are separated by ",["
  int pos = 0;
  while (pos < body.length()) {
    // find next row start
    int rowStart = body.indexOf('[', pos);
    if (rowStart == -1) break;
    int rowDepth = 0;
    int rowEnd = -1;
    for (int i = rowStart; i < (int)body.length(); ++i) {
      char c = body[i];
      if (c == '[') ++rowDepth;
      else if (c == ']') { --rowDepth; if (rowDepth == 0) { rowEnd = i; break; } }
    }
    if (rowEnd == -1) break;

    String row = body.substring(rowStart + 1, rowEnd); // e.g. "A","B","C"

    std::vector<String> cols;
    int p = 0;
    while (p < row.length()) {
      // find next quoted value
      int q1 = row.indexOf('"', p);
      if (q1 == -1) break;
      int q2 = row.indexOf('"', q1 + 1);
      if (q2 == -1) break;
      String val = row.substring(q1 + 1, q2);
      cols.push_back(val);
      p = q2 + 1;
      // skip comma or other separators
      int comma = row.indexOf(',', p);
      if (comma == -1) break;
      p = comma + 1;
    }

    outRows.emplace_back();
    for (auto &c : cols) outRows.back().push_back(c);

    pos = rowEnd + 1;
  }
}

// Helper: return current time as an ISO-like string using NTP/localtime
static String getNtpTimeString()
{
  struct tm timeinfo;
  // try getLocalTime which uses the configured NTP
  if (getLocalTime(&timeinfo)) {
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
  }

  // fallback to time() epoch seconds if available
  time_t now = time(nullptr);
  if (now > 1000000000) {
    struct tm *tmr = localtime(&now);
    if (tmr) {
      char buf[64];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tmr);
      return String(buf);
    }
  }

  // final fallback: use millis() to avoid empty values
  return String(millis());
}

void tokenStatusCallback(TokenInfo info);

// ===================== WIFI CONNECTIVITY ============================

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.println("Attempting WiFi connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();

  configTime(0, 0, "pool.ntp.org");
  delay(1000);

  time_t now;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    time(&now);
    GSheet.setSystemTime(now);
    Serial.println("✅ System time set for GSheet");
  } else {
    Serial.println("⚠️ Failed to obtain time from NTP");
  }

  GSheet.setTokenCallback(tokenStatusCallback);
  GSheet.setPrerefreshSeconds(10 * 60);
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
  
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
  // Fetch all product data from Google Sheets using service-account
  ensureWiFi();
  if (!isWiFiConnected()) {
    g_registry.logError(ERR_SHEETS_SYNC, "WiFi not connected", "");
    return;
  }

  // Wait for GSheet to be ready (token acquired)
  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) {
    delay(50);
  }

  String resp;
  String range = "Products!A2:D"; // A: code, B: name, C: stock, D: i2c address
  bool ok = GSheet.values.get(&resp, spreadsheetId, range.c_str());
  if (!ok) {
    Serial.println("GSheet: failed to read Products range: ");
    Serial.println(GSheet.errorReason());
    g_registry.logError(ERR_SHEETS_SYNC, "GSheet read failed", GSheet.errorReason());
    return;
  }

  std::vector<std::vector<String>> rows;
  parseValuesJson(resp, rows);

  for (size_t i = 0; i < rows.size(); ++i) {
    auto &r = rows[i];
    if (r.size() < 4) continue;
    String code = r[0];
    String name = r[1];
    int stock = r[2].toInt();
    uint8_t addr = (uint8_t)r[3];

    g_registry.addProduct(code, name, stock, true);
    ProductModule* mod = g_registry.findModuleByAddress(addr);
    if (mod) {
      mod->itemCode = code;
      mod->name = name;
      mod->stock = stock;
    }
  }

  Serial.println("Product data synced from Google Sheets (service-account)");
  g_registry.debugPrintProducts();

  // --- Also load Modules mapping into registry (Modules!A2:C -> UID, Address, ProductCode)
  String respMod;
  bool ok2 = GSheet.values.get(&respMod, spreadsheetId, "Modules!A2:C");
  if (!ok2) {
    Serial.println("GSheet: failed to read Modules range: ");
    Serial.println(GSheet.errorReason());
    // don't treat this as fatal; registry still has products
  } else {
    std::vector<std::vector<String>> modRows;
    parseValuesJson(respMod, modRows);
    for (size_t i = 0; i < modRows.size(); ++i) {
      auto &r = modRows[i];
      if (r.size() < 1) continue;
      String uid = r[0];
      uint8_t addr = 0;
      String code = "";
      if (r.size() >= 2) addr = (uint8_t)r[1].toInt();
      if (r.size() >= 3) code = r[2];
      // Add module entry; name/stock will be filled when product exists
      g_registry.addModule(addr, uid, code, "", 0);
    }
    Serial.println("Module mapping synced from Google Sheets");
    g_registry.debugPrintModules();
  }
}

void logTransactionToSheets(const String& itemCode, int amount) {
  // Append a row to Transactions sheet: timestamp, itemCode, amount
  ensureWiFi();
  if (!isWiFiConnected()) return;

  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(10);

  FirebaseJson valueRange;
  valueRange.add("range", "Transactions!A:C");
  valueRange.add("majorDimension", "ROWS");
  valueRange.set("values/[0]/[0]", getNtpTimeString());
  valueRange.set("values/[0]/[1]", itemCode);
  valueRange.set("values/[0]/[2]", String(amount));

  FirebaseJson response;
  bool ok = GSheet.values.append(&response, spreadsheetId, "Transactions!A:C", &valueRange, "USER_ENTERED", "INSERT_ROWS", "true");
  if (!ok) {
    Serial.print("GSheet append transaction failed: ");
    Serial.println(GSheet.errorReason());
    g_registry.logError(ERR_SHEETS_SYNC, "Transaction append failed", GSheet.errorReason());
  } else {
    Serial.println("Transaction appended to Google Sheets");
  }
}

void updateStockInSheets(const String& itemCode, int newStock) {
  // Find the product row in Products sheet and update column C
  ensureWiFi();
  if (!isWiFiConnected()) return;

  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(10);

  String range = "Products!A2:D";
  String resp;
  bool ok = GSheet.values.get(&resp, spreadsheetId, range.c_str());
  if (!ok) {
    Serial.print("GSheet read failed for updateStock: ");
    Serial.println(GSheet.errorReason());
    g_registry.logError(ERR_SHEETS_SYNC, "read for updateStock failed", GSheet.errorReason());
    return;
  }

  std::vector<std::vector<String>> rows;
  parseValuesJson(resp, rows);

  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].size() < 1) continue;
    if (rows[i][0] == itemCode) {
      // row index i corresponds to sheet row (i + 2)
      String target = "Products!C";
      target += String(i + 2);
      FirebaseJson valueRange;
      valueRange.add("range", target);
      valueRange.add("majorDimension", "ROWS");
      valueRange.set("values/[0]/[0]", String(newStock));

      FirebaseJson response;
      bool ok2 = GSheet.values.update(&response, spreadsheetId, target.c_str(), &valueRange);
      if (!ok2) {
        Serial.print("GSheet update failed: ");
        Serial.println(GSheet.errorReason());
        g_registry.logError(ERR_SHEETS_SYNC, "updateStock failed", GSheet.errorReason());
      } else {
        Serial.println("Products sheet stock updated");
      }
      return;
    }
  }

  Serial.println("Product code not found in sheet when updating stock");
}

void logErrorToSheets(const String& errorMsg, const String& errorDetails) {
  // Append an error row to Errors sheet: timestamp, message, details
  ensureWiFi();
  if (!isWiFiConnected()) return;

  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(10);

  FirebaseJson valueRange;
  valueRange.add("range", "Errors!A:C");
  valueRange.add("majorDimension", "ROWS");
  valueRange.set("values/[0]/[0]", getNtpTimeString());
  valueRange.set("values/[0]/[1]", errorMsg);
  valueRange.set("values/[0]/[2]", errorDetails);

  FirebaseJson response;
  bool ok = GSheet.values.append(&response, spreadsheetId, "Errors!A:C", &valueRange, "USER_ENTERED", "INSERT_ROWS", "true");
  if (!ok) {
    Serial.print("GSheet append error log failed: ");
    Serial.println(GSheet.errorReason());
  }
}

void registerNewModuleToSheets(const String& moduleUID, uint8_t i2cAddress) {
  // Append a row to Modules sheet: uid, address
  ensureWiFi();
  if (!isWiFiConnected()) return;

  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(10);

  FirebaseJson valueRange;
  valueRange.add("range", "Modules!A:B");
  valueRange.add("majorDimension", "ROWS");
  valueRange.set("values/[0]/[0]", moduleUID);
  valueRange.set("values/[0]/[1]", String(i2cAddress));

  FirebaseJson response;
  bool ok = GSheet.values.append(&response, spreadsheetId, "Modules!A:B", &valueRange, "USER_ENTERED", "INSERT_ROWS", "true");
  if (!ok) {
    Serial.print("GSheet append module failed: ");
    Serial.println(GSheet.errorReason());
  }
}

bool isModuleRegistered(const String& moduleUID, uint8_t &outAddress) {
  outAddress = 0;
  ensureWiFi();
  if (!isWiFiConnected()) return false;

  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(10);

  String resp;
  bool ok = GSheet.values.get(&resp, spreadsheetId, "Modules!A2:B");
  if (!ok) {
    Serial.print("GSheet read failed for Modules check: ");
    Serial.println(GSheet.errorReason());
    return false;
  }

  std::vector<std::vector<String>> rows;
  parseValuesJson(resp, rows);

  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].size() < 1) continue;
    if (rows[i][0] == moduleUID) {
      if (rows[i].size() >= 2) outAddress = (uint8_t)rows[i][1].toInt();
      return true;
    }
  }

  return false;
}

void tokenStatusCallback(TokenInfo info)
{
    if (info.status == token_status_error)
    {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else
    {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}
