# Vending System - HMI Module (Revised Implementation)

## Overview

This is a complete rewrite of the vending system HMI module with the following major changes:

### What's New (Revised Specs)
- ✅ **No RFID/NFC Authentication** - Replaced with simple keypad confirmation
- ✅ **Simplified State Machine** - 9 states (added ERROR_STATE)
- ✅ **Comprehensive Error Handling** - 10+ error codes with remote logging
- ✅ **Module Auto-Discovery** - I2C bus scan with UID-based matching
- ✅ **Google Sheets Sync** - Products, stock, and transaction logging
- ✅ **Modular Architecture** - Clear separation of concerns
- ✅ **Online/Offline Tracking** - Module health monitoring

---

## System Architecture

### Core Modules

#### 1. **datatypes.h/cpp** - Data Structures & Registry
- `ErrorCode` enum - 10+ error types
- `ProductItem` - Product metadata (code, name, stock)
- `ProductModule` - Module data (address, UID, stock, health)
- `ProductRegistry` - Global registry for products & modules
- `ErrorLog` - Error tracking with timestamps

**Key Methods:**
- `addProduct()`, `findProduct()`
- `addModule()`, `findModuleByCode()`, `findModuleByUID()`
- `updateModuleHealth()`, `updateModuleStock()`
- `logError()` - Local error logging

#### 2. **fsm.h/cpp** - Event-Driven State Machine
- 9 States: IDLE, ITEM_SELECT, CHECK_AVAIL, WAIT_CONFIRM, DISPENSE, THANK_YOU, OUT_OF_STOCK, CANCEL_STATE, ERROR_STATE
- 13 Events: Keypad input, stock events, dispense events, timeouts
- Transition table `[9 states][13 events]`
- State entry/exit handlers
- Event-specific handlers per state

**State Flow:**
```
IDLE (sync every 30s)
├─> Keypad input → ITEM_SELECT
└─> Periodically refresh products from Google Sheets

ITEM_SELECT (buffer input)
├─> Valid code + "#" → CHECK_AVAIL
├─> Invalid code → ERROR_STATE
├─> Module offline → ERROR_STATE
└─> "*" → CANCEL_STATE

CHECK_AVAIL (quick check)
├─> Stock > 0 → WAIT_CONFIRM
└─> Stock = 0 → OUT_OF_STOCK

WAIT_CONFIRM (30s timeout)
├─> "#" confirmed → DISPENSE
├─> "*" cancelled → CANCEL_STATE
└─> Timeout → CANCEL_STATE

DISPENSE (I2C communication)
├─> ACK success → THANK_YOU
└─> Error/timeout → ERROR_STATE

THANK_YOU (3s auto-return)
└─> Timeout → IDLE

OUT_OF_STOCK (3s auto-return)
└─> Timeout → IDLE

CANCEL_STATE (3s auto-return)
└─> Timeout → IDLE

ERROR_STATE (5s auto-return)
└─> Timeout → IDLE
```

#### 3. **productmoduleinterface.h/cpp** - I2C Module Control
**I2C Protocol Commands:**
- `CMD_WHOAMI (0x01)` - Get module UID
- `CMD_GET_STOCK (0x02)` - Query stock level
- `CMD_UPDATE_DISPLAY (0x03)` - Update OLED
- `CMD_DISPENSE (0x10)` - Dispense command
- `CMD_ACK_SUCCESS (0x55)` - Success ACK
- `CMD_ACK_ERROR (0xEE)` - Error ACK

**Key Functions:**
- `discoverProductModules()` - Full I2C bus scan
- `matchModulesToSheets()` - Match modules with Google Sheets
- `checkModuleHealth()` - Verify all modules online
- `syncModuleDisplays()` - Update all OLEDs
- `i2c_dispense()` - Send dispense command with timeout handling

#### 4. **googlesheets.h/cpp** - Cloud Synchronization
**API Actions:**
- `getAllProducts` - Fetch product list, stock, I2C addresses
- `logTransaction` - Log item, timestamp, amount
- `updateStock` - Decrement stock after dispensing
- `logError` - Remote error tracking
- `registerModule` - Register new module discovery

**Expected Google Sheets Format:**
```
CODE|NAME|STOCK|ADDR;
SNACK01|Chips|15|0x10;
SNACK02|Candy|8|0x11;
DRINK01|Water|20|0x12;
```

#### 5. **main.cpp** - Event Loop & Initialization

**Startup Sequence:**
1. Initialize I2C, LCD, WiFi
2. Discover modules on I2C bus
3. Fetch products from Google Sheets
4. Match modules with products
5. Sync all module displays
6. Enter IDLE state

**Main Loop:**
- Detect keypad input
- Check periodic sync timer
- Process events through FSM
- Execute state actions (timeouts, LCD updates)

---

## Error Handling (NEW)

### Error Codes
```cpp
ERR_NONE = 0
ERR_MODULE_OFFLINE = 1         // Module not responding
ERR_I2C_COMM = 2               // I2C bus error
ERR_DISPENSE_FAILED = 3        // Motor/mechanism error
ERR_MODULE_UID_MISMATCH = 4    // UID doesn't match sheet
ERR_STOCK_MISMATCH = 5         // Stock discrepancy
ERR_SHEETS_SYNC = 6            // Google Sheets sync failed
ERR_MODULE_DISCONNECTED = 7    // Was online, now offline
ERR_INVALID_PRODUCT = 8        // Product code not found
ERR_TIMEOUT = 9                // Operation timeout
```

### Error State Behavior
- Displays error code and message on LCD
- Logs error locally and to Google Sheets (if online)
- Auto-returns to IDLE after 5 seconds
- All previous session data cleared

---

## Module Discovery & Registration

### Startup Discovery Process
1. **I2C Bus Scan** - Address 0x08 to 0x77
2. **WHOAMI Query** - Get module UID from each device
3. **Stock Query** - Get current stock level
4. **Google Sheets Match** - Link modules to products via:
   - Module UID (primary)
   - I2C address (fallback)
5. **New Module Handling** - If UID not in Sheets:
   - Create placeholder entry
   - Send to Google Sheets for admin assignment
   - Display "New Module Detected" on LCD

### Module Health Monitoring
- Periodic health checks every 30s (in IDLE)
- Mark modules as `online`/`offline`
- Reject vending from offline modules
- Error on missing modules mid-transaction

---

## Transaction Flow (Complete Example)

### User Purchase Workflow
1. **IDLE**: System displays welcome, periodically syncs data
2. **User Input**: Presses any key → ITEM_SELECT
3. **Code Entry**: User types "SNACK01" and presses "#"
   - Code validated against registry
   - Module checked for online status
   - Stock verified (must be > 0)
4. **CHECK_AVAIL**: Brief "Checking stock..." display
   - If available → WAIT_CONFIRM
   - If unavailable → OUT_OF_STOCK
5. **WAIT_CONFIRM**: Display shows product name, prompts "# Confirm * Cancel"
   - User presses "#" to confirm
   - 30-second timeout if no response
6. **DISPENSE**: Send I2C command to module
   - Module drives stepper motor
   - Product dispensed
   - Wait for ACK_SUCCESS or timeout (5s)
7. **THANK_YOU**: Display "Thank you!" + "Item dispensed"
   - Decrement stock locally
   - Log transaction to Google Sheets
   - Update stock in Google Sheets
   - Auto-return to IDLE after 3 seconds
8. **IDLE**: Back to ready state

### Error Flow (Example)
1. User enters invalid code → ERROR_STATE
2. Display shows: "ERROR: 08" (ERR_INVALID_PRODUCT)
3. Local error logged with timestamp
4. If online, error sent to Google Sheets
5. Auto-return to IDLE after 5 seconds

---

## Timing Constants

```cpp
SYNC_INTERVAL_MS = 30000        // Refresh products every 30s
PAYMENT_TIMEOUT_MS = 30000      // Confirmation wait: 30s
OOS_TIMEOUT_MS = 3000           // Out of stock display: 3s
CANCEL_TIMEOUT_MS = 3000        // Cancel message: 3s
ERROR_TIMEOUT_MS = 5000         // Error display: 5s
I2C_RESPONSE_TIMEOUT = 5000     // I2C command ACK timeout: 5s
```

---

## Hardware Pinout

### I2C (Product Modules & LCD)
- SDA: GPIO21 (default Arduino I2C)
- SCL: GPIO22 (default Arduino I2C)

### Keypad
- Rows: GPIO32, 33, 34, 35
- Cols: GPIO36, 37, 38, 39

### WiFi
- Built-in ESP32 WiFi module

---

## Development Notes

### Building & Uploading
1. Configure WiFi credentials in `config.h`:
   ```cpp
   #define WIFI_SSID "YOUR_SSID"
   #define WIFI_PASS "YOUR_PASSWORD"
   ```

2. Configure Google Apps Script URL:
   ```cpp
   #define APPS_SCRIPT_URL "YOUR_APPS_SCRIPT_URL"
   ```

3. Use PlatformIO to build and upload:
   ```bash
   pio run --target upload
   ```

### Serial Debugging
Enable Serial output at 115200 baud for detailed logs:
- Initialization steps
- Module discovery
- Google Sheets sync
- I2C communication errors
- Error logging

### Google Sheets Setup
Create an Apps Script with these endpoints:
- `getAllProducts` - GET/POST returns product list
- `logTransaction` - POST item, amount, timestamp
- `updateStock` - POST item code, new stock count
- `logError` - POST error message and details
- `registerModule` - POST module UID and I2C address

---

## Future Enhancements

- [ ] Admin mode for manual stock adjustment
- [ ] Multi-language support on LCD
- [ ] Barcode scanner integration
- [ ] QR code payment integration (replacing keypad)
- [ ] Inventory alerts when stock low
- [ ] Temperature sensor for refrigerated items
- [ ] Tamper detection
- [ ] Statistics dashboard in Google Sheets

---

## License

Proprietary - Vending System Project
