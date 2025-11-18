# QUICK START GUIDE

## 5-Minute Setup

### Step 1: Edit Configuration (1 min)
Open `include/config.h` and update:

```cpp
// Line 8-9: WiFi
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASS "YourPassword"

// Line 12: Google Sheets API
#define APPS_SCRIPT_URL "https://script.google.com/macros/d/YOUR_ID/userweb"
```

### Step 2: Set Up Google Sheets (2 min)

**Create Sheets:**
1. Create new Google Sheet called "VendingSystem"
2. Rename sheet to "Products"
3. Add columns: `item_code | name | stock | i2c_address | module_uid`
4. Add sample row: `SNACK01 | Chips | 15 | 0x10 | MOD_001`

**Create Apps Script:**
1. Go to Extensions → Apps Script
2. Copy code from IMPLEMENTATION_GUIDE.md
3. Save and Deploy as Web App
4. Copy deployment URL to config.h

### Step 3: Upload Code (1 min)
```bash
cd ~/Documents/PlatformIO/Projects/micpros_final_project
pio run --target upload
pio device monitor --baud 115200
```

### Step 4: Verify (1 min)
Serial output should show:
```
=== VENDING SYSTEM INITIALIZATION ===
[1/5] I2C initialized
[2/5] LCD initialized
[3/5] WiFi connection started
[4/5] Module discovery complete
[5/5] Google Sheets sync complete
=== INITIALIZATION COMPLETE ===
```

---

## Testing Checklist

- [ ] LCD displays "VENDING SYSTEM"
- [ ] Keypad responsive (press any key)
- [ ] Product codes load from Sheets
- [ ] Modules discovered on I2C bus
- [ ] Product entry works (type code, press #)
- [ ] Confirmation prompt appears
- [ ] Dispense triggers motor
- [ ] Stock updates in Google Sheets
- [ ] Transaction logs appear in Sheets
- [ ] Error messages display correctly

---

## Common Issues

### Nothing on LCD
```
→ Check I2C address 0x27
→ Verify power supply
→ Test with I2C scanner sketch
```

### No modules found
```
→ Check I2C wiring (SDA=GPIO21, SCL=GPIO22)
→ Verify module I2C address (0x10-0x7F)
→ Test module with WHOAMI command
```

### WiFi fails
```
→ Check SSID/password in config.h
→ Verify signal strength
→ Try 2.4GHz WiFi (not 5GHz)
```

### Google Sheets not syncing
```
→ Verify APPS_SCRIPT_URL is deployed
→ Check Google Sheets format matches
→ Test URL in browser
```

---

## State Flow (User Perspective)

```
1. System boots
   Display: "VENDING SYSTEM" "Enter Product Code"

2. Press any key
   Display: "Product Code:" (ready for input)

3. Type "SNACK01"
   Display: Shows entered code updating

4. Press "#"
   Display: "Checking stock..." "SNACK01"

5. If available:
   Display: "Ready: Chips" "# Confirm  * Cancel"
   
6. Press "#" to buy
   Display: "Dispensing..." "Chips"

7. Motor runs, product dispensed
   Display: "Thank You!" "Item dispensed"

8. Auto-return to welcome (3 seconds)
   Display: Back to step 1
```

---

## Error Codes Quick Reference

| Code | Error | Solution |
|------|-------|----------|
| 01 | Module offline | Check module power & I2C |
| 02 | I2C communication | Check wiring |
| 03 | Dispense failed | Check motor & mechanism |
| 08 | Invalid product | Check product code |
| 06 | Sheets sync failed | Check WiFi & URL |

---

## FAQ

**Q: Can I add more products?**
A: Yes, just add rows to the Products sheet. Sync happens automatically every 30 seconds.

**Q: How do I add a new module?**
A: Connect module to I2C bus. System auto-detects at startup. Register in Google Sheets.

**Q: What if WiFi goes down?**
A: System continues operating locally. Transactions are logged when WiFi reconnects.

**Q: How do I change the dispense amount?**
A: Edit `targetAmount` in ProductItem struct (default is 1).

**Q: Can I modify timeout values?**
A: Yes, edit timing constants in config.h.

---

## Next Steps

1. **Production Deployment:**
   - Move WiFi credentials to secure storage
   - Use proper SSL certificates for HTTPS
   - Set up backup power (UPS)
   - Monitor error logs regularly

2. **Add Features:**
   - Admin mode for manual adjustments
   - Inventory alerts
   - Sales analytics
   - Remote diagnostics

3. **Monitor System:**
   - Check Google Sheets error log weekly
   - Monitor stock levels
   - Track transaction volume
   - Verify module health

---

## Support

- See **IMPLEMENTATION_GUIDE.md** for detailed setup
- See **FSM_REFERENCE.md** for state machine details
- See **README_REVISED.md** for system architecture
- Check serial output at 115200 baud for debug info

---

**You're ready to go! 🚀**
