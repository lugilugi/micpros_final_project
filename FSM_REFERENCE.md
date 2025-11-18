# FSM State Transition Reference

## Quick State Table

| Current | Event | Action | Next |
|---------|-------|--------|------|
| **IDLE** | Any key | Show "Enter Product Code" | ITEM_SELECT |
| **IDLE** | Sync timer | Refresh products from Sheets | IDLE |
| **ITEM_SELECT** | Key char | Append to buffer | ITEM_SELECT |
| **ITEM_SELECT** | Submit (#) | Validate code → Check stock | CHECK_AVAIL |
| **ITEM_SELECT** | Cancel (*) | Clear buffer | CANCEL_STATE |
| **CHECK_AVAIL** | Stock > 0 | Show available stock | WAIT_CONFIRM |
| **CHECK_AVAIL** | Stock = 0 | Show "Out of Stock" | OUT_OF_STOCK |
| **WAIT_CONFIRM** | Submit (#) | Send I2C dispense | DISPENSE |
| **WAIT_CONFIRM** | Cancel (*) | Show "Cancelled" | CANCEL_STATE |
| **WAIT_CONFIRM** | Timeout | Show "Cancelled" | CANCEL_STATE |
| **DISPENSE** | ACK success | Log + update stock | THANK_YOU |
| **DISPENSE** | Error/timeout | Show error | ERROR_STATE |
| **THANK_YOU** | 3s timeout | Reset variables | IDLE |
| **OUT_OF_STOCK** | 3s timeout | Reset variables | IDLE |
| **CANCEL_STATE** | 3s timeout | Reset variables | IDLE |
| **ERROR_STATE** | 5s timeout | Reset variables | IDLE |

## LCD Display Mapping

### IDLE
```
VENDING SYSTEM
Enter Product Code
```

### ITEM_SELECT
```
Product Code:
SNACK01_________
```
(Updates as user types)

### CHECK_AVAIL
```
Checking stock...
SNACK01_________
```

### WAIT_CONFIRM
```
Ready: Chips
# Confirm  * Cancel
```

### DISPENSE
```
Dispensing...
Chips___________
```

### THANK_YOU
```
Thank You!
Item dispensed__
```

### OUT_OF_STOCK
```
Out of Stock
SNACK01_________
```

### CANCEL_STATE
```
Transaction
Cancelled_______
```

### ERROR_STATE
```
ERROR: 08
Code not found__
```

## Event Priority

1. **Keypad input** (all states) - Highest priority
2. **Periodic sync** (IDLE only) - Every 30 seconds
3. **State timeouts** - Handled in `onStateAction()`
4. **I2C responses** - Polled during DISPENSE
5. **WiFi status** - Background, non-blocking

## Key Mappings

| Key | Function | States |
|-----|----------|--------|
| `1-9, 0` | Append to buffer | ITEM_SELECT |
| `#` | Submit/Confirm | ITEM_SELECT, WAIT_CONFIRM |
| `*` | Cancel | All states |
| `A-D` | Reserved | All states |

## Transition Matrix Indices

```cpp
// transitionTable[state][event] → next_state
States (0-8):
  0 = IDLE
  1 = ITEM_SELECT
  2 = CHECK_AVAIL
  3 = WAIT_CONFIRM
  4 = DISPENSE
  5 = THANK_YOU
  6 = OUT_OF_STOCK
  7 = CANCEL_STATE
  8 = ERROR_STATE

Events (0-12):
  0 = EVT_NONE
  1 = EVT_KEY_CHAR
  2 = EVT_KEY_SUBMIT
  3 = EVT_KEY_CANCEL
  4 = EVT_SYNC_TIMEOUT
  5 = EVT_PRODUCT_FOUND
  6 = EVT_PRODUCT_NOT_FOUND
  7 = EVT_STOCK_AVAILABLE
  8 = EVT_STOCK_EMPTY
  9 = EVT_DISPENSE_ACK
  10 = EVT_DISPENSE_ERROR
  11 = EVT_TIMEOUT
  12 = EVT_ERROR_OCCURRED
```

## I2C Protocol Summary

### Master → Slave (HMI → Module)

#### WHOAMI (0x01)
```
Command: 0x01
Response: Module UID (string, null-terminated)
Purpose: Module identification and validation
```

#### GET_STOCK (0x02)
```
Command: 0x02
Response: 2 bytes (LSB first, uint16)
Purpose: Query current stock level
```

#### UPDATE_DISPLAY (0x03)
```
Command: 0x03, length, name[length], stock_lo, stock_hi
Purpose: Update OLED with product info
```

#### DISPENSE (0x10)
```
Command: 0x10
Response: 0x55 (success) or 0xEE (error)
Timeout: 5 seconds
Purpose: Trigger stepper motor dispensing
```

## Error Code Reference

```cpp
1  = ERR_MODULE_OFFLINE      → "Module offline"
2  = ERR_I2C_COMM           → "I2C error"
3  = ERR_DISPENSE_FAILED    → "Dispense failed"
4  = ERR_MODULE_UID_MISMATCH → "UID mismatch"
5  = ERR_STOCK_MISMATCH     → "Stock mismatch"
6  = ERR_SHEETS_SYNC        → "Sync failed"
7  = ERR_MODULE_DISCONNECTED → "Module lost"
8  = ERR_INVALID_PRODUCT    → "Code not found"
9  = ERR_TIMEOUT            → "Timeout"
```

## Data Structures

### ProductItem
```cpp
String itemCode              // "SNACK01"
String name                  // "Chips"
int stock                    // 15
int targetAmount             // 1
bool available               // true
```

### ProductModule
```cpp
uint8_t i2cAddress           // 0x10
String moduleUID             // "MOD_001"
String itemCode              // "SNACK01"
String name                  // "Chips"
int stock                    // 15
bool healthy                 // true
bool online                  // true
unsigned long lastSeen       // millis()
```

## Google Sheets Integration

### Expected CSV Format
```
item_code,name,stock,i2c_address,module_uid
SNACK01,Chips,15,0x10,MOD_001
SNACK02,Candy,8,0x11,MOD_002
DRINK01,Water,20,0x12,MOD_003
```

### API Endpoints
- GET `?action=getAllProducts` → Returns product list
- POST `{"action":"logTransaction","item":"...","amount":...}` → Log sale
- POST `{"action":"updateStock","item":"...","stock":...}` → Update stock
- POST `{"action":"logError","message":"..."}` → Log error
- POST `{"action":"registerModule","uid":"...","address":"..."}` → Register new

---

## Testing Checklist

- [ ] System initializes without errors
- [ ] I2C modules discovered on startup
- [ ] Google Sheets successfully synced
- [ ] Keypad input responsive
- [ ] Product code entry and validation works
- [ ] Out of stock handled correctly
- [ ] Dispense command successful
- [ ] Transaction logged to Sheets
- [ ] Stock decremented correctly
- [ ] All timeouts working (3-5s)
- [ ] Error states display correctly
- [ ] Error logging to Sheets works
- [ ] Module offline detection works
- [ ] Recovery from WiFi loss
- [ ] LCD updates responsive and clear

---

## Debugging Tips

### Serial Output
```
[1/5] I2C initialized
[2/5] LCD initialized
[3/5] WiFi connection started
[4/5] Module discovery complete
[5/5] Google Sheets sync complete

Scanning I2C bus for product modules...
Found device at 0x10
  Module UID: MOD_001
Found device at 0x11
  Module UID: MOD_002
Module discovery complete
Product data synced from Google Sheets
```

### Common Issues

**No modules discovered:**
- Check I2C wiring (SDA/SCL)
- Verify module I2C addresses (0x08-0x77)
- Ensure modules powered on

**WiFi connection fails:**
- Verify SSID/password in config.h
- Check WiFi signal strength
- Ensure Apps Script URL is valid

**Google Sheets sync fails:**
- Verify APPS_SCRIPT_URL in config.h
- Check Google Apps Script is deployed and active
- Ensure expected CSV format matches

**Dispense times out:**
- Check I2C communication (long wires?)
- Verify module responding to WHOAMI
- Increase I2C_RESPONSE_TIMEOUT if needed

**Stock not updating:**
- Verify module returns correct stock
- Check Google Sheets API connectivity
- Confirm updateStock endpoint working

---
