#ifndef FSM_H
#define FSM_H

#include <Arduino.h>
#include "datatypes.h"

// ===================== FSM STATES ======================================

enum State {
  IDLE = 0,              // Default state, waiting for input
  ITEM_SELECT = 1,       // User entering product code
  CHECK_AVAIL = 2,       // Checking stock availability
  WAIT_CONFIRM = 3,      // Waiting for user confirmation (* = cancel, # = confirm)
  DISPENSE = 4,          // Dispensing product
  THANK_YOU = 5,         // Transaction complete
  OUT_OF_STOCK = 6,      // Product unavailable
  CANCEL_STATE = 7,      // Transaction cancelled
  ERROR_STATE = 8        // Error state
};

// ===================== FSM EVENTS ======================================

enum Event {
  EVT_NONE = 0,
  EVT_KEY_CHAR = 1,              // Alphanumeric key pressed
  EVT_KEY_SUBMIT = 2,            // '#' key pressed (confirm)
  EVT_KEY_CANCEL = 3,            // '*' key pressed (cancel)
  EVT_SYNC_TIMEOUT = 4,          // Periodic Google Sheets sync
  EVT_PRODUCT_FOUND = 5,         // Product exists in registry
  EVT_PRODUCT_NOT_FOUND = 6,     // Product doesn't exist
  EVT_STOCK_AVAILABLE = 7,       // Stock > 0
  EVT_STOCK_EMPTY = 8,           // Stock == 0
  EVT_DISPENSE_ACK = 9,          // Module confirmed dispensing
  EVT_DISPENSE_ERROR = 10,       // Dispensing failed
  EVT_TIMEOUT = 11,              // Generic timeout
  EVT_ERROR_OCCURRED = 12        // Error occurred
};

// ===================== FSM STATE MANAGEMENT ===========================

extern volatile State currentState;
extern String inputBuffer;
extern String selectedCode;
extern ProductModule* selectedModule;
extern unsigned long stateEnteredAt;
extern unsigned long confirmDeadline;
extern unsigned long syncTimer;
extern ErrorCode lastErrorCode;
extern String lastErrorMsg;

// FSM functions
void initFSM();
void enterState(State newState);
void processEvent(Event evt);
void onStateEntry(State s);
void onStateExit(State s);
void onStateAction(State s);

// State-specific handlers
bool handleItemSelectEvent(Event evt);
bool handleCheckAvailEvent(Event evt);
bool handleConfirmEvent(Event evt);
bool handleDispenseEvent(Event evt);

#endif // FSM_H
