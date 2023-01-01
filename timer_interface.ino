#include <stdio.h>
#include <LiquidCrystal.h>

//LCD pin to Arduino
const short pin_RS = 8, pin_EN = 9, pin_d4 = 4, pin_d5 = 5, pin_d6 = 6, pin_d7 = 7, pin_BL = 10;

LiquidCrystal lcd(pin_RS, pin_EN, pin_d4, pin_d5, pin_d6, pin_d7);

long msSample;

bool lcdOn = false;
long lcdTimeout = 15e3L;
long lastInteraction = 0;

/// GENERAL
const long MS_PER_MINUTE = 6e4L;
//const long MS_PER_MINUTE = 100;

// MENU
const short MAIN_MENU = -1, CONSTANT = 0, ON_FOR = 1, OFF_FOR = 2, MIN_PER_HOUR = 3;
long lastCheck = 0;
bool released = true;

// DEFINE MENU ITEMS
struct MenuItem {
  short selectedItem;
  const short nStates;
  const String menuNames[10];
  short options[5];
  short currentOption;
};

MenuItem mainMenu { CONSTANT, 4, { "Constant", "On for...", "Off for...", "Minutes per hour" }, {}, MAIN_MENU };
MenuItem constantOnOff { 0, 2, { "Off", "On" }, {false, true}, 0};
MenuItem onFor { 0, 5, { "30 mins", "1 hr", "2 hrs", "3 hrs", "4 hrs" }, { 30, 60, 120, 180, 240 }, 0 };
MenuItem offFor { 0, 5, { "30 mins", "1 hr", "2 hrs", "3 hrs", "4 hrs" }, { 30, 60, 120, 180, 240 }, 0 };
MenuItem minPerHour { 0, 5, { "10 mins / hr", "20 mins / hr", "30 mins / hr", "40 mins / hr", "50 mins / hr" }, { 10, 20, 30, 40, 50 }, 0 };

namespace relay {
// whether the relay is on or off
const bool ON = true, OFF = false;
short onOff = OFF;
long lastChange = 0;
long toggleIn = 100000;
byte pinNo = 0;
// the mode (e.g. CONSTANT, ON_FOR ... etc. )
short selectedMode = CONSTANT;
}

struct Events {
// MENU EVENTS
bool menuStateChange;
bool debounced;
bool left, right, up, down, select, released;
bool modeChange;
bool timeElapsed;
bool onOffChange;
bool lcdTimedOut;
bool interaction;
};

Events events {true, false, false, false, false, false, false, true, true, false, false, false};

void setup() {
  pinMode(pin_BL, OUTPUT);
  Serial.begin(9600);
  lcd.begin(16, 2);
}

void loop() {

  /// GET MS ONCE PER LOOP
  msSample = millis();

  /// FIRE EVENTS ///

  if (msSample - lastCheck > 400 or events.released) {
    lastCheck = msSample;

    events.debounced = true;
  }

  if ( events.debounced ) {
    getCurrentButton(events);
    events.debounced = false;
  }

  if ( (events.up || events.down || events.right || events.left || events.select) && !events.released ) {
    events.interaction = true;
    lastInteraction = msSample;
  }

  if (msSample - lastInteraction > lcdTimeout) {
    events.lcdTimedOut = true;
  }

  // fire event if time expired i.e. time to toggle
  if ((msSample - relay::lastChange) > relay::toggleIn) {
    events.timeElapsed = true;
  }

  // fire event if LCD timed out
  if (msSample - lastCheck > lcdTimeout) {
    events.lcdTimedOut = true;
  }

  /// HANDLE KEYPRESSES & MENU

    /// MAIN MENU ///

   if ((!lcdOn) && events.interaction) {
      digitalWrite(pin_BL, HIGH);
      lcdOn = true;
    }
  

    if (mainMenu.currentOption == MAIN_MENU) {
      if (events.menuStateChange) {
        lcd.setCursor(0, 0);
        lcd.print("                ");
        lcd.setCursor(0, 0);
        lcd.print(mainMenu.menuNames[mainMenu.selectedItem]);

        events.menuStateChange = false;
      }

      if (events.up || events.down) {
        cycleState(mainMenu.selectedItem, mainMenu.nStates, events.up);

        events.up = false;
        events.down = false;
        events.menuStateChange = true;
      }

      else if (events.select) {
        // back to previous menu
        mainMenu.currentOption = mainMenu.selectedItem;

        events.select = false;
        events.menuStateChange = true;
      }
  
    }
    /// CONSTANT - ON/OFF FOREVER ///
    else if (mainMenu.currentOption == CONSTANT) {
      menuHandler(constantOnOff, events, MAIN_MENU, relay::selectedMode);
    }

    /// ON_FOR -> on for a predefined time
    else if (mainMenu.currentOption == ON_FOR) {
      menuHandler(onFor, events, MAIN_MENU, relay::selectedMode);
    }

    else if (mainMenu.currentOption == OFF_FOR) {
      menuHandler(offFor, events, MAIN_MENU, relay::selectedMode);
    }

    /// MIN_PER_HOUR -> on for n minutes per hour (off for the rest)
    else if (mainMenu.currentOption == MIN_PER_HOUR) {
      menuHandler(minPerHour, events, MAIN_MENU, relay::selectedMode);
    }

  /// HANDLE RELAY EVENTS ///

  if (events.modeChange) {
    // set up when mode changes
    if (relay::selectedMode == CONSTANT) {
      relay::onOff = constantOnOff.options[constantOnOff.currentOption];
      events.onOffChange = true;
    }
    else if ((relay::selectedMode == ON_FOR) || (relay::selectedMode == OFF_FOR)) {
      relay::onOff = (relay::selectedMode == ON_FOR ? relay::ON : relay::OFF);
      relay::toggleIn = (relay::selectedMode == ON_FOR ? onFor.options[onFor.currentOption] * MS_PER_MINUTE : offFor.options[offFor.currentOption] * MS_PER_MINUTE);
      events.timeElapsed = false;
      events.onOffChange = true;
    }
    else if (relay::selectedMode == MIN_PER_HOUR) {
      relay::onOff = relay::ON;
      relay::toggleIn = minPerHour.options[minPerHour.currentOption] * MS_PER_MINUTE;
      events.timeElapsed = false;
      events.onOffChange = true;
    }

    events.modeChange = false;
  }

  // handle time-elapsing event
  if (events.timeElapsed) {
    if ((relay::selectedMode == ON_FOR) || (relay::selectedMode == OFF_FOR)) {
      relay::onOff = !relay::onOff;
      relay::selectedMode = CONSTANT;
      events.onOffChange = true;
    } else if (relay::selectedMode == MIN_PER_HOUR) {
      relay::onOff = !relay::onOff;
      // if we've just turned it on, then stay on for the set time, if not, stay off for 60 mins - set time
      relay::toggleIn = (relay::onOff ? (minPerHour.options[minPerHour.currentOption] * MS_PER_MINUTE) : 60 * MS_PER_MINUTE - (minPerHour.options[minPerHour.currentOption] * MS_PER_MINUTE));
      events.onOffChange = true;
    }
    events.timeElapsed = false;
  }

  if (events.onOffChange) {
    // DISPLAY CURRENT RELAY STATE
    lcd.setCursor(13, 1);
    lcd.print((relay::onOff ? " ON" : "OFF"));
    events.onOffChange = false;
    relay::lastChange = msSample;
    Serial.println((relay::onOff ? " ON" : "OFF"));
  }

  // handle LCD timeout
  if (events.lcdTimedOut) {
    digitalWrite(pin_BL, LOW);
    lcdOn = false;
    events.lcdTimedOut = false;
  }

  // consume interaction event
  events.interaction = false;
}
