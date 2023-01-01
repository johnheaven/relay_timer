
void getCurrentButton(Events& events) {

  events.right, events.up, events.down, events.left, events.select = false;
  events.released = true;
  
  int buttonReading = 0;

  buttonReading = analogRead(0);

  if (buttonReading < 60) {
    events.right = true;
    events.released = false;
  }
  else if (buttonReading < 200) {
    events.up = true;
    events.released = false;
  }
  else if (buttonReading < 400){
    events.down = true;
    events.released = false;
  }
  else if (buttonReading < 600){
    events.left = true;
    events.released = false;
  }
  else if (buttonReading < 800){
    events.select = true;
    events.released = false;
  }
}

void cycleState(short& state, const short& nStates, const bool& direction) {
    // add or subtract according to direction
    if (direction) state++; else state--;
    // if it's -1, return to max value
    if (state < 0) state = nStates - 1;
    // if it's bigger than max value, start from zero
    if (state == nStates) state = 0;
  }

void menuHandler(
  MenuItem& menuItem,
  Events& events,
  const short& MAIN_MENU,
  short& relayMode)
   {

  if (events.menuStateChange) {
          events.menuStateChange = false;
          lcd.setCursor(0, 0);
          lcd.print("                ");
          lcd.setCursor(0, 0);
          lcd.print(menuItem.menuNames[menuItem.selectedItem]);
        }

  if (events.up || events.down) {
    cycleState(menuItem.selectedItem, menuItem.nStates, events.up);
    events.up = false;
    events.down = false;
    events.menuStateChange = true;
  }

  else if (events.select) {
    // save mode
    relayMode = mainMenu.currentOption;

    // save option
    menuItem.currentOption = menuItem.selectedItem;

    // back to previous menu
    mainMenu.currentOption = MAIN_MENU;

    events.select = false;
    events.menuStateChange = true;
    events.modeChange = true;
  }

  else if (events.left) {
    // leave without saving
    mainMenu.currentOption = MAIN_MENU;
    events.left = false;
    events.menuStateChange = true;
   }
}
