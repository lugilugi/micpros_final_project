#include "fsm.h"
#include "googlesheets.h"
#include "productmoduleinterface.h"
#include "config.h"
#include <LiquidCrystal_I2C.h>

// ===================== FSM STATE VARIABLES ============================

volatile State currentState = IDLE;
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
  {IDLE, ITEM_SELECT, IDLE, CANCEL_STATE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, ERROR_STATE},
  
  // ITEM_SELECT (state 1)
  {ITEM_SELECT, ITEM_SELECT, CHECK_AVAIL, CANCEL_STATE, ITEM_SELECT, CHECK_AVAIL, CHECK_AVAIL, ITEM_SELECT, ITEM_SELECT, ITEM_SELECT, ITEM_SELECT, ITEM_SELECT, ERROR_STATE},
  
  // CHECK_AVAIL (state 2)
  {CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, WAIT_CONFIRM, OUT_OF_STOCK, CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, CHECK_AVAIL, ERROR_STATE},
  
  // WAIT_CONFIRM (state 3)
  {WAIT_CONFIRM, WAIT_CONFIRM, DISPENSE, CANCEL_STATE, WAIT_CONFIRM, WAIT_CONFIRM, WAIT_CONFIRM, WAIT_CONFIRM, WAIT_CONFIRM, WAIT_CONFIRM, CANCEL_STATE, WAIT_CONFIRM, ERROR_STATE},
  
  // DISPENSE (state 4)
  {DISPENSE, DISPENSE, DISPENSE, CANCEL_STATE, DISPENSE, DISPENSE, DISPENSE, DISPENSE, DISPENSE, THANK_YOU, CANCEL_STATE, DISPENSE, ERROR_STATE},
  
  // THANK_YOU (state 5)
  {THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, THANK_YOU, IDLE, THANK_YOU, ERROR_STATE},
  
  // OUT_OF_STOCK (state 6)
  {OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, OUT_OF_STOCK, IDLE, OUT_OF_STOCK, ERROR_STATE},
  
  // CANCEL_STATE (state 7)
  {CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, CANCEL_STATE, IDLE, CANCEL_STATE, ERROR_STATE},
  
  // ERROR_STATE (state 8)
  {ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, ERROR_STATE, IDLE, ERROR_STATE, ERROR_STATE}
};

// ===================== FSM INITIALIZATION =============================

void initFSM() {
  currentState = IDLE;
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
    case IDLE:
      inputBuffer = "";
      selectedCode = "";
      selectedModule = nullptr;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("VENDING SYSTEM");
      lcd.setCursor(0, 1);
      lcd.print("Enter Product Code");
      break;
      
    case ITEM_SELECT:
      inputBuffer = "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Product Code:");
      lcd.setCursor(0, 1);
      lcd.print("");
      break;
      
    case CHECK_AVAIL:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Checking stock...");
      lcd.setCursor(0, 1);
      lcd.print(selectedCode.c_str());
      break;
      
    case WAIT_CONFIRM:
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
      
    case DISPENSE:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dispensing...");
      lcd.setCursor(0, 1);
      lcd.print(selectedModule ? selectedModule->name.c_str() : "Unknown");
      break;
      
    case THANK_YOU:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Thank You!");
      lcd.setCursor(0, 1);
      lcd.print("Item dispensed");
      break;
      
    case OUT_OF_STOCK:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Out of Stock");
      lcd.setCursor(0, 1);
      lcd.print(selectedCode.c_str());
      break;
      
    case CANCEL_STATE:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Transaction");
      lcd.setCursor(0, 1);
      lcd.print("Cancelled");
      break;
      
    case ERROR_STATE:
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
    case WAIT_CONFIRM:
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
    case IDLE:
      // Periodically sync with Google Sheets every 30 seconds
      if (now - syncTimer > SYNC_INTERVAL_MS) {
        syncProductDataFromSheets();
        syncModuleDisplays();
        syncTimer = now;
      }
      break;
      
    case ITEM_SELECT:
      // Update LCD with buffer as user types
      if (inputBuffer.length() > 0) {
        lcd.setCursor(0, 1);
        lcd.print(inputBuffer);
        for (int i = inputBuffer.length(); i < 20; i++) {
          lcd.print(" ");
        }
      }
      break;
      
    case CHECK_AVAIL:
      // Handled by event processing
      break;
      
    case WAIT_CONFIRM:
      // Check for timeout
      if (now > confirmDeadline) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case THANK_YOU:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > 3000) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case OUT_OF_STOCK:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > OOS_TIMEOUT_MS) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case CANCEL_STATE:
      // Auto-return to IDLE after 3 seconds
      if (now - stateEnteredAt > CANCEL_TIMEOUT_MS) {
        processEvent(EVT_TIMEOUT);
      }
      break;
      
    case ERROR_STATE:
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
    case IDLE:
      break;
      
    case ITEM_SELECT:
      shouldTransition = handleItemSelectEvent(evt);
      break;
      
    case CHECK_AVAIL:
      shouldTransition = handleCheckAvailEvent(evt);
      break;
      
    case WAIT_CONFIRM:
      shouldTransition = handleConfirmEvent(evt);
      break;
      
    case DISPENSE:
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
      enterState(CHECK_AVAIL);
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
      enterState(DISPENSE);
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
      return true; // Transition to CANCEL_STATE
      
    default:
      return false;
  }
}
