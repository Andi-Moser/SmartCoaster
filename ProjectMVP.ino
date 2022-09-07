//////////////////////////////////////////////////////////////////////
// Libraries
//////////////////////////////////////////////////////////////////////

#include "M5Stack.h"
#include "HX711.h"
#include "Adafruit_NeoPixel.h"
#include "StateMachine.h"

//////////////////////////////////////////////////////////////////////
// IO Mapping
//////////////////////////////////////////////////////////////////////
#define LED_PIN             2
#define LOADCELL_DOUT_PIN   16
#define LOADCELL_SCK_PIN    17

//////////////////////////////////////////////////////////////////////
// Configuration
//////////////////////////////////////////////////////////////////////
#define PIXELS_COUNT        20
#define LOADCELL_OFFSET     50682624
#define LOADCELL_DIVIDER    5895655

//////////////////////////////////////////////////////////////////////
// Global Variables
//////////////////////////////////////////////////////////////////////
Adafruit_NeoPixel g_Pixels = Adafruit_NeoPixel(PIXELS_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);;
HX711 g_LoadCell;
StateMachine machine = StateMachine();

long tareFactor;
long lastRead;
long currentRead;

long emptyWeight = 0;
long fullWeight = 0;

//////////////////////////////////////////////////////////////////////
// States
//////////////////////////////////////////////////////////////////////
State* s_Init = machine.addState(&stateInit);
State* s_Calibrate_Empty = machine.addState(&stateCalibrateEmpty);
State* s_Empty_Done = machine.addState(&stateEmptyDone);
State* s_Calibrate_Full = machine.addState(&stateCalibrateFull);
State* s_Drinking = machine.addState(&stateDrinking);

// ****** StateInit
int currentLedInit = 0;
void stateInit() {
  showOnlySingleLED(currentLedInit++, g_Pixels.Color(0, 0, 255));
  if (currentLedInit == PIXELS_COUNT) {
    currentLedInit = 0;
  }
}

// ****** StateCalibrationEmpty
bool calibrationEmptyDone = false;
long totalEmptyReads = 0;
int calibrateEmptyFrom = 0;
int calibrateEmptyUntil = 0;
int calibrateEmptyCounter = 0;
void stateCalibrateEmpty() {
  if (machine.executeOnce) {
    calibrationEmptyDone = false;
    calibrateEmptyCounter = 0;
    totalEmptyReads = 0;
    calibrateEmptyFrom = currentLedInit;
    calibrateEmptyUntil = currentLedInit;
  }

  showLEDFromUntil(calibrateEmptyFrom, calibrateEmptyUntil++, g_Pixels.Color(0, 0, 255));
  if (calibrateEmptyUntil >= PIXELS_COUNT) {
    calibrateEmptyUntil = 0;
  }
  
  totalEmptyReads += getReading();
  if (calibrateEmptyCounter++ == PIXELS_COUNT) {
    emptyWeight = (long)(totalEmptyReads/PIXELS_COUNT);
    calibrationEmptyDone = true;
  }
}

int currentLedEmptyDone = 0;
void stateEmptyDone() {
    showOnlySingleLED(currentLedEmptyDone++, g_Pixels.Color(0, 255, 0));
  if (currentLedEmptyDone == PIXELS_COUNT) {
    currentLedEmptyDone = 0;
  }
}

// ****** StateCalibrationFull
bool calibrationFullDone = false;
long totalFullReads = 0;
int calibrateFullFrom = 0;
int calibrateFullUntil = 0;
int calibrateFullCounter = 0;
void stateCalibrateFull() {
  if (machine.executeOnce) {
    calibrationFullDone = false;
    calibrateFullFrom = currentLedEmptyDone;
    calibrateFullUntil = currentLedEmptyDone;
    calibrateFullCounter = 0;
    totalFullReads = 0;
  }

  showLEDFromUntil(calibrateFullFrom, calibrateFullUntil++, g_Pixels.Color(0, 255, 0));
  if (calibrateFullUntil >= PIXELS_COUNT) {
    calibrateFullUntil = 0;
  }

  totalFullReads += getReading();
  if (calibrateFullCounter++ == PIXELS_COUNT) {
    fullWeight = (long)(totalFullReads/PIXELS_COUNT);
    calibrationFullDone = true;
  }
}

// ****** StateDrinking
int currentPixelsShowing = 0;
void stateDrinking() {
  if (machine.executeOnce) {
    lightAllLeds(g_Pixels.Color(0, 255, 0));
  }

  long valueRaw = currentRead - emptyWeight;

  float ratio = (float)valueRaw / (float)fullWeight;
  int ledsToShow = round((float)ratio * (float)PIXELS_COUNT);

  Serial.println(String(ledsToShow) + "\t\t" + String(ratio) + "\t\t" + String(currentRead));

  if (ledsToShow != currentPixelsShowing) {
    currentPixelsShowing = ledsToShow;

    showLEDUntil(currentPixelsShowing, g_Pixels.Color(0, 255, 0));
  }

}


//////////////////////////////////////////////////////////////////////
// Transitions
//////////////////////////////////////////////////////////////////////

int t_initToCalibrateEmpty_Counter = 0;
bool t_initToCalibrateEmpty() {
  if (currentRead > 2000) {
    t_initToCalibrateEmpty_Counter++;

    if (t_initToCalibrateEmpty_Counter > 10) {
      t_initToCalibrateEmpty_Counter = 0;
      return true;
    }
  }

  return false;
}

bool t_EmptyCalibrationToEmptyDone() {
  return calibrationEmptyDone;
}

int t_EmptyDoneToFull_Counter = 0;
bool t_EmptyDoneToFull() {
    if (currentRead > (2000 + emptyWeight)) {
    t_EmptyDoneToFull_Counter++;

    if (t_EmptyDoneToFull_Counter > 10) {
      t_EmptyDoneToFull_Counter = 0;
      return true;
    }
  }

  return false;
  
}

bool t_FullToDrinking() {
  if (calibrationFullDone) {
    for (int i = 0; i < 3; i++) {
      lightAllLeds(g_Pixels.Color(0, 255, 0));
      delay(250);
      lightAllLeds(g_Pixels.Color(0, 0, 0));
      delay(250);
    }
    return true;
  }

  return false;
}

bool t_DrinkingToInit() {
  return currentRead < emptyWeight || abs(currentRead - emptyWeight) < 2000;
}

int t_ToReset_Counter = 0;
bool t_ToReset() {
  if (machine.executeOnce) {
    t_ToReset_Counter = 0;
  }

  if (currentRead > (fullWeight + 30000)) {
    t_ToReset_Counter++;

    if (t_ToReset_Counter > 10) {
      return true;
    }
  }

  return false;
}


void setup() {
  M5.begin();
  M5.Power.begin();

  g_LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  tareFactor = g_LoadCell.read();
  g_Pixels.begin();

  s_Init->addTransition(&t_initToCalibrateEmpty, s_Calibrate_Empty);
  s_Calibrate_Empty->addTransition(&t_EmptyCalibrationToEmptyDone, s_Empty_Done);
  s_Empty_Done->addTransition(&t_EmptyDoneToFull, s_Calibrate_Full);
  s_Calibrate_Full->addTransition(&t_FullToDrinking, s_Drinking);
  s_Drinking->addTransition(&t_ToReset, s_Init);
  s_Drinking->addTransition(&t_DrinkingToInit, s_Init);

}

void loop() {
  lastRead = currentRead;
  currentRead = getReading();
  
  machine.run();
  delay(200);
  //showLEDFromUntil(6, 2, g_Pixels.Color(255,255,0));
}

//////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////
void lightAllLeds(uint32_t color) {
  for (int i = 0; i < PIXELS_COUNT; i++) {
    g_Pixels.setPixelColor(i, color);
  }
  g_Pixels.show();
}
void showOnlySingleLED(int ledNumber, uint32_t color) {
  for (int i = 0; i < PIXELS_COUNT; i++) {
    g_Pixels.setPixelColor(i, i == ledNumber ? color : g_Pixels.Color(0,0,0));
  }
  g_Pixels.show();
}
void showLEDUntil(int ledNumber, uint32_t color) {
  for (int i = 0; i < PIXELS_COUNT; i++) {
    g_Pixels.setPixelColor(i, i <= ledNumber ? color : g_Pixels.Color(0,0,0));
  }
  g_Pixels.show();
}

void showLEDFromUntil(int from, int until, uint32_t color) {
  if (from > until) {
      for (int i = 0; i < PIXELS_COUNT; i++) {
        Serial.println(String(i) + " " + String(i <= until) + " " + String(i >= from));
        g_Pixels.setPixelColor(i, (i <= until || i >= from) ? color : g_Pixels.Color(0,0,0));
      }
      g_Pixels.show();
  }
  else {
    for (int i = 0; i < PIXELS_COUNT; i++) {
      g_Pixels.setPixelColor(i, (i >= from && i <= until) ? color : g_Pixels.Color(0,0,0));
    }
    g_Pixels.show();
  }
}

long getReading() {
  return g_LoadCell.read() - tareFactor;
}
