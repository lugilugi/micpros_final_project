# Vending System - Implementation Guide

## Project Structure

```
micpros_final_project/
├── include/
│   ├── config.h                      # Configuration & pinout
│   ├── datatypes.h                   # Data structures & registry
│   ├── fsm.h                         # State machine definitions
│   ├── googlesheets.h                # Cloud API functions
│   └── productmoduleinterface.h      # I2C module control
│
├── src/
│   ├── main.cpp                      # Main event loop & initialization
│   ├── datatypes.cpp                 # Registry implementation
│   ├── fsm.cpp                       # FSM state handlers
│   ├── googlesheets.cpp              # Google Sheets API
│   └── productmoduleinterface.cpp    # I2C communication
│
├── platformio.ini                    # PlatformIO config
├── README_REVISED.md                 # System overview
└── FSM_REFERENCE.md                  # State machine reference
```

## Setup Instructions

### 1. Hardware Requirements

**HMI Control Module (Master):**
- ESP32 microcontroller
- 20×4 I2C LCD display (address 0x27)
- 4×4 Matrix keypad
- I2C bus (SDA=GPIO21, SCL=GPIO22)
- WiFi connectivity

**Product Module (Slave) - per module:**
- STM32 or Arduino microcontroller
- OLED display (for visual feedback)
- TMC2209 stepper motor driver
- Stepper motor (NEMA 17 or similar)
- I2C interface (unique address 0x10-0x77)

### 2. Configuration

**config.h - WiFi & Google Sheets:**
```cpp
#define WIFI_SSID "YourWiFiNetwork"
#define WIFI_PASS "YourPassword"
#define APPS_SCRIPT_URL "https://script.google.com/..."
```

**config.h - Pin Definitions (Already set):**
- Keypad: GPIO 32-39
- I2C: GPIO 21 (SDA), GPIO 22 (SCL)
- LCD: I2C address 0x27

### 3. PlatformIO Setup

**platformio.ini:**
```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
lib_deps =
    Wire
    LiquidCrystal I2C
    Keypad
    WiFi
    HTTPClient
monitor_speed = 115200
```

### 4. Google Sheets Setup

**Create Apps Script with these functions:**

```javascript
// GET - Fetch all products
function doGet(e) {
  const action = e.parameter.action;
  
  if (action === "getAllProducts") {
    const sheet = SpreadsheetApp.getActiveSheet();
    const data = sheet.getDataRange().getValues();
    let result = "";
    
    for (let i = 1; i < data.length; i++) {
      const code = data[i][0];
      const name = data[i][1];
      const stock = data[i][2];
      const addr = data[i][3];
      result += `${code}|${name}|${stock}|${addr};`;
    }
    
    return ContentService.createTextOutput(result);
  }
}

// POST - Handle transactions, stock updates, errors, module registration
function doPost(e) {
  const data = JSON.parse(e.postData.contents);
  const sheet = SpreadsheetApp.getActiveSheet();
  
  if (data.action === "logTransaction") {
    // Append to transactions sheet
    const transSheet = SpreadsheetApp.getActiveSpreadsheet()
                                    .getSheetByName("Transactions");
    transSheet.appendRow([
      new Date(),
      data.item,
      data.amount,
      new Date(parseInt(data.timestamp))
    ]);
  }
  
  if (data.action === "updateStock") {
    // Find and update stock
    const range = sheet.getRange(2, 1, sheet.getLastRow() - 1);
    const values = range.getValues();
    for (let i = 0; i < values.length; i++) {
      if (values[i][0] === data.item) {
        sheet.getRange(i + 2, 3).setValue(data.stock);
        break;
      }
    }
  }
  
  if (data.action === "logError") {
    // Append to errors sheet
    const errSheet = SpreadsheetApp.getActiveSpreadsheet()
                                   .getSheetByName("Errors");
    errSheet.appendRow([
      new Date(),
      data.message,
      data.details
    ]);
  }
  
  if (data.action === "registerModule") {
    // Log new module
    const modSheet = SpreadsheetApp.getActiveSpreadsheet()
                                   .getSheetByName("Modules");
    modSheet.appendRow([
      data.uid,
      data.address,
      "PENDING_ASSIGNMENT",
      new Date()
    ]);
  }
  
  return ContentService.createTextOutput("OK");
}
```

**Expected Google Sheets Schema:**

**Sheet 1: Products**
```
| item_code | name      | stock | i2c_address | module_uid |
|-----------|-----------|-------|-------------|------------|
| SNACK01   | Chips     | 15    | 0x10        | MOD_001    |
| SNACK02   | Candy     | 8     | 0x11        | MOD_002    |
| DRINK01   | Water     | 20    | 0x12        | MOD_003    |
```

**Sheet 2: Transactions**
```
| timestamp           | item_code | amount | transaction_time    |
|------------------- |-----------|--------|---------------------|
| 2025-01-15 10:30   | SNACK01   | 1      | 2025-01-15 10:30:45 |
```

**Sheet 3: Errors**
```
| timestamp          | message         | details           |
|--------------------|-----------------|-------------------|
| 2025-01-15 10:25  | Module offline  | Address: 0x10     |
```

**Sheet 4: Modules**
```
| module_uid | i2c_address | status               | registered_at       |
|------------|-------------|----------------------|---------------------|
| MOD_001    | 0x10        | SNACK01              | 2025-01-15 09:00    |
| MOD_NEW    | 0x13        | PENDING_ASSIGNMENT   | 2025-01-15 10:45    |
```

### 5. Building and Uploading

**Using PlatformIO CLI:**
```bash
# Build only
pio run

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

**Using PlatformIO IDE (VS Code):**
- Click "Build" button to compile
- Click "Upload" button to flash
- Click "Serial Monitor" to view debug output

### 6. Testing the System

**Step 1: Verify Hardware**
```
✓ LCD displays correctly
✓ Keypad responsive (test each key)
✓ Serial output shows initialization
✓ I2C modules discovered
```

**Step 2: Verify I2C Communication**
Serial output should show:
```
Scanning I2C bus for product modules...
Found device at 0x10
  Module UID: MOD_001
Found device at 0x11
  Module UID: MOD_002
Module discovery complete
```

**Step 3: Verify Google Sheets Sync**
Serial output should show:
```
Attempting WiFi connection...
WiFi connected!
Product data synced from Google Sheets
```

**Step 4: Test User Flow**
1. Press any key on keypad
2. Type product code (e.g., "SNACK01")
3. Press "#" to submit
4. Press "#" to confirm purchase
5. Verify product dispensed
6. Check Google Sheets for transaction log

### 7. Troubleshooting

**Problem: Modules not discovered**
```cpp
// Check config.h pin definitions
// Verify I2C wiring (SDA=GPIO21, SCL=GPIO22)
// Ensure modules have unique I2C addresses
// Test with I2C scanner first
```

**Problem: Google Sheets not syncing**
```cpp
// Verify WiFi credentials in config.h
// Check APPS_SCRIPT_URL is correct
// Test URL in browser first
// Ensure Apps Script is published
```

**Problem: Keypad not responsive**
```cpp
// Check pin definitions match hardware
// Test keypad with simple sketch first
// Verify resistor values correct
// Check for pin conflicts
```

**Problem: LCD display issues**
```cpp
// Verify I2C address (default 0x27)
// Check LCD power supply
// Test I2C communication first
// Adjust contrast potentiometer
```

### 8. Serial Debugging

**Enable verbose output in setup():**
```cpp
Serial.begin(115200);
Serial.println("=== SYSTEM STARTING ===");
```

**Common serial outputs:**
```
[1/5] I2C initialized
[2/5] LCD initialized
[3/5] WiFi connection started
[4/5] Module discovery complete
[5/5] Google Sheets sync complete
```

### 9. Production Checklist

- [ ] All GPIO pins verified against schematic
- [ ] WiFi credentials configured
- [ ] Google Apps Script deployed and tested
- [ ] Google Sheets sheets created (Products, Transactions, Errors, Modules)
- [ ] I2C addresses unique (no conflicts)
- [ ] Module UIDs unique and in Google Sheets
- [ ] Keypad fully functional
- [ ] LCD contrast adjusted
- [ ] Stepper motors tested
- [ ] Error handling tested (simulate offline module)
- [ ] Transaction logging verified
- [ ] Stock decrement working
- [ ] Module health monitoring active
- [ ] Timeout handling tested
- [ ] Full user flow tested

---

## File Compilation Dependencies

```
main.cpp
├── config.h
├── datatypes.h (extern ProductRegistry)
├── fsm.h
│   ├── datatypes.h
│   └── googlesheets.h
├── productmoduleinterface.h
│   ├── config.h
│   └── datatypes.h
└── googlesheets.h
    ├── config.h
    └── datatypes.h

datatypes.cpp
└── datatypes.h

fsm.cpp
├── fsm.h
├── googlesheets.h
├── productmoduleinterface.h
├── config.h
└── LiquidCrystal_I2C

googlesheets.cpp
├── googlesheets.h
├── config.h
├── datatypes.h
└── HTTPClient

productmoduleinterface.cpp
├── productmoduleinterface.h
├── googlesheets.h
├── config.h
└── Wire (I2C)
```

---

## Memory Considerations

**ESP32 Resources:**
- Flash: 4MB (plenty for this project)
- RAM: 520KB usable
  - Strings: ~2KB (product codes, names)
  - Registry vectors: ~5KB (max 100 products)
  - Temporary buffers: ~2KB
  - Available: ~500KB for future expansion

**Optimization Tips:**
- Use PROGMEM for constants
- Limit product registry size
- Clear error logs periodically
- Use String sparingly in loops

---

## Next Steps for Production

1. **Deploy Google Apps Script:**
   - Apps Script Editor → Deploy as web app
   - Set execute as your account
   - Allow anonymous access (or restrict)
   - Deploy → New deployment

2. **Configure ESP32:**
   - Burn WiFi credentials
   - Verify I2C addresses all unique
   - Test with serial monitor
   - Deploy to production

3. **Monitor System:**
   - Check error logs in Google Sheets regularly
   - Monitor stock levels
   - Watch for offline modules
   - Track transaction volume

4. **Maintenance:**
   - Periodically resync product data
   - Check stepper motor performance
   - Clean module sensors/optics
   - Archive old transaction logs

---

## Support & Documentation

For detailed information, see:
- `README_REVISED.md` - System architecture overview
- `FSM_REFERENCE.md` - State machine detailed reference
- `config.h` - Pin definitions and timing
- Source code comments for implementation details

---

