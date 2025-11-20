#include "fsm.h"
#include "googlesheets.h"
#include "productmoduleinterface.h"
#include "config.h"
#include <LiquidCrystal_I2C_ESP32.h>

// ===================== FSM STATE VARIABLES ============================

volatile State currentState = STATE_IDLE;
String inputBuffer = "";
String selectedCode = "";
ProductModule* selectedModule = nullptr;
unsigned long stateEnteredAt = 0;
unsigned long confirmDeadline = 0;
unsigned long syncTimer = 0;
ErrorCode lastErrorCode = ERR_NONE;
String lastErrorMsg = "";

extern LiquidCrystal_I2C lcd;

// ===================== FSM TRANSITION TABLE ============================
// transitionTable[current_state][event] -> next_state

static State transitionTable[9][13] = {
  // IDLE (state 0)
  {STATE_IDLE, STATE_ITEM_SELECT, STATE_IDLE, STATE_CANCEL, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_ERROR},
  
  // ITEM_SELECT (state 1)
  {STATE_ITEM_SELECT, STATE_ITEM_SELECT, STATE_CHECK_AVAIL, STATE_CANCEL, STATE_ITEM_SELECT, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_ITEM_SELECT, STATE_ITEM_SELECT, STATE_ITEM_SELECT, STATE_ITEM_SELECT, STATE_ITEM_SELECT, STATE_ERROR},
  
  // CHECK_AVAIL (state 2)
  {STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_WAIT_CONFIRM, STATE_OUT_OF_STOCK, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_CHECK_AVAIL, STATE_ERROR},
  
  // WAIT_CONFIRM (state 3)
  {STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_DISPENSE, STATE_CANCEL, STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_WAIT_CONFIRM, STATE_CANCEL, STATE_WAIT_CONFIRM, STATE_ERROR},
  
  // DISPENSE (state 4)
  {STATE_DISPENSE, STATE_DISPENSE, STATE_DISPENSE, STATE_CANCEL, STATE_DISPENSE, STATE_DISPENSE, STATE_DISPENSE, STATE_DISPENSE, STATE_DISPENSE, STATE_THANK_YOU, STATE_CANCEL, STATE_DISPENSE, STATE_ERROR},
  
  // THANK_YOU (state 5)
  {STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_THANK_YOU, STATE_IDLE, STATE_THANK_YOU, STATE_ERROR},
  
  // OUT_OF_STOCK (state 6)
  {STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_OUT_OF_STOCK, STATE_IDLE, STATE_OUT_OF_STOCK, STATE_ERROR},
  
  // CANCEL_STATE (state 7)
  {STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_CANCEL, STATE_IDLE, STATE_CANCEL, STATE_ERROR},
  
  // ERROR_STATE (state 8)
  {STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_ERROR, STATE_IDLE, STATE_ERROR, STATE_ERROR}
};

// ===================== FSM INITIALIZATION =============================

void initFSM() {
  currentState = STATE_IDLE;
  inputBuffer = "";
  selectedCode = "";
  selectedModule = nullptr;
  stateEnteredAt = millis();
  confirmDeadline = 0;
  syncTimer = millis();
  lastErrorCode = ERR_NONE;
  lastErrorMsg = "";
}

// ===================== STATE ENTRY HANDLER =============================

void onStateEntry(State s) {
  stateEnteredAt = millis();
  
  switch (s) {
    case STATE_IDLE:
      inputBuffer = "";
      selectedCode = "";
      selectedModule = nullptr;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("VENDING SYSTEM");
      lcd.setCursor(0, 1);
      lcd.print("Enter Product Code");
      break;
      
    case STATE_ITEM_SELECT:
      inputBuffer = "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Product Code:");
      lcd.setCursor(0, 1);
      lcd.print("");
      break;
      
    case STATE_CHECK_AVAIL:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Checking stock...");
      lcd.setCursor(0, 1);
      lcd.print(selectedCode.c_str());
      break;
      
    case STATE_WAIT_CONFIRM:
      confirmDeadline = millis() + PAYMENT_TIMEOUT_MS;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ready: ");
      if (selectedModule) {
        lcd.print(selectedModule->name.c_str());
      }
      lcd.setCursor(0, 1);
      lcd.print("# Confirm  * Cancel");
      break;
      
    case STATE_DISPENSE:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dispensing...");
      lcd.setCursor(0, 1);
      lcd.print(selectedModule ? selectedModule->name.c_str() : "Unknown");
      break;
      
    case STATE_THANK_YOU:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Thank You!");
      lcd.setCursor(0, 1);
      lcd.print("Item dispensed");
      break;
      
    case STATE_OUT_OF_STOCK:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Out of Stock");
      lcd.setCursor(0, 1);
      lcd.print(selectedCode.c_str());
      break;
      
    case STATE_CANCEL:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Transaction");
      lcd.setCursor(0, 1);
      lcd.print("Cancelled");
      break;
      
    case STATE_ERROR:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR:");
      lcd.print((int)lastErrorCode);
      lcd.setCursor(0, 1);
      lcd.print(lastErrorMsg.c_str());
      break;
  }
}

void onStateExit(State s) {
  switch (s) {
    case STATE_WAIT_CONFIRM:
      confirmDeadline = 0;
      break;
    default:
      break;
  }
}

// ===================== STATE ACTION HANDLER =============================

void onStateAction(State s) {
  unsigned long now = millis();
  
  switch (s) {
    case STATE_IDLE:
      // Periodically sync with Google Sheets every 30 seconds
      if (now - syncTimer > SYNC_INTERVAL_MS) {
        syncProductDataFromSheets();
        syncModuleDisplays();
        syncTimer = now;
      }
      break;
      
    case STATE_ITEM_SELECT:
      // Update LCD with buffer as user types
      if (inputBuffer.length() > 0) {
        lcd.setCursor(0, 1);
        lcd.print(inputBuffer);
        for (int i = inputBuffer.length(); i < 20; i++) {
          lcd.print(" ");
        }
      }
      break;
      
    case STATE_CHECK_AVAIL:
      // Handled by event processing
      break;
      
    case STATE_WAIT_CONFIRM:
      // Check for timeout
      if (now > confirmDeadline) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case STATE_THANK_YOU:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > 3000) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case STATE_OUT_OF_STOCK:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > OOS_TIMEOUT_MS) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case STATE_CANCEL:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > CANCEL_TIMEOUT_MS) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case STATE_ERROR:
      // Auto-return to IDLE after 5 seconds
      if (now - stateEnteredAt > ERROR_TIMEOUT_MS) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    default:
      break;
  }
}

// ===================== MAIN STATE TRANSITION FUNCTION ==================

void enterState(State newState) {
  if (newState == currentState) return;
  
  onStateExit(currentState);
  currentState = newState;
  onStateEntry(currentState);
}

void processEvent(Event evt) {
  if (evt == EVT_NONE) return;
  
  State nextState = transitionTable[currentState][evt];
  bool shouldTransition = true;
  
  switch (currentState) {
    case STATE_IDLE:
      break;
      
    case STATE_ITEM_SELECT:
      shouldTransition = handleItemSelectEvent(evt);
      break;
      
    case STATE_CHECK_AVAIL:
      shouldTransition = handleCheckAvailEvent(evt);
      break;
      
    case STATE_WAIT_CONFIRM:
      shouldTransition = handleConfirmEvent(evt);
      break;
      
    case STATE_DISPENSE:
      shouldTransition = handleDispenseEvent(evt);
      break;
      
    default:
      break;
  }
  
  if (shouldTransition && nextState != currentState) {
    enterState(nextState);
  }
}

// ===================== STATE-SPECIFIC EVENT HANDLERS ====================

bool handleItemSelectEvent(Event evt) {
  switch (evt) {
    case EVT_KEY_CHAR:
      // Already handled in main loop
      return false;
      
    case EVT_KEY_SUBMIT: {
      // User submitted product code
      selectedCode = inputBuffer;
      selectedModule = g_registry.findModuleByCode(selectedCode);
      
      if (!selectedModule) {
        // Product not found
        lastErrorCode = ERR_INVALID_PRODUCT;
        lastErrorMsg = "Code not found";
        processEvent(EVT_ERROR_OCCURRED);
        return false;
      }
      
      if (!selectedModule->online) {
        // Module offline
        lastErrorCode = ERR_MODULE_OFFLINE;
        lastErrorMsg = "Module offline";
        processEvent(EVT_ERROR_OCCURRED);
        return false;
      }
      
      // Proceed to check availability
      enterState(STATE_CHECK_AVAIL);
      delay(500);
      
      if (selectedModule->stock > 0) {
        processEvent(EVT_STOCK_AVAILABLE);
      } else {
        processEvent(EVT_STOCK_EMPTY);
      }
      return false;
    }
      
    case EVT_KEY_CANCEL:
      return true;
      
    default:
      return false;
  }
}

bool handleCheckAvailEvent(Event evt) {
  switch (evt) {
    case EVT_STOCK_AVAILABLE:
      return true; // Transition to WAIT_CONFIRM
      
    case EVT_STOCK_EMPTY:
      return true; // Transition to OUT_OF_STOCK
      
    default:
      return false;
  }
}

bool handleConfirmEvent(Event evt) {
  switch (evt) {
    case EVT_KEY_SUBMIT: {
      // User confirmed purchase
      enterState(STATE_DISPENSE);
      delay(500);
      
      // Attempt to dispense
      if (!selectedModule) {
        lastErrorCode = ERR_MODULE_OFFLINE;
        lastErrorMsg = "Module lost";
        processEvent(EVT_ERROR_OCCURRED);
        return false;
      }
      
      bool ok = i2c_dispense(selectedModule->i2cAddress);
      if (ok) {
        // Update stock
        selectedModule->stock--;
        g_registry.updateModuleStock(selectedModule->i2cAddress, selectedModule->stock);
        
        // Log to Google Sheets
        logTransactionToSheets(selectedCode, 1);
        updateStockInSheets(selectedCode, selectedModule->stock);
        
        processEvent(EVT_DISPENSE_ACK);
      } else {
        lastErrorCode = ERR_DISPENSE_FAILED;
        lastErrorMsg = "Dispense failed";
        processEvent(EVT_ERROR_OCCURRED);
      }
      return false;
    }
      
    case EVT_KEY_CANCEL:
    case EVT_TIMEOUT:
      return true;
      
    default:
      return false;
  }
}

bool handleDispenseEvent(Event evt) {
  switch (evt) {
    case EVT_DISPENSE_ACK:
      return true; // Transition to THANK_YOU
      
    case EVT_DISPENSE_ERROR:
      return true; // Transition to STATE_CANCEL
      
    default:
      return false;
  }
}
