//////////////////////////////////////////////////////////////////////
// Libraries
//////////////////////////////////////////////////////////////////////

#include "HX711.h"
#include "Adafruit_NeoPixel.h"
#include "StateMachine.h"

//////////////////////////////////////////////////////////////////////
// IO Mapping
//////////////////////////////////////////////////////////////////////
#define LED_PIN             2
#define LOADCELL_DOUT_PIN   17
#define LOADCELL_SCK_PIN    16

//////////////////////////////////////////////////////////////////////
// Configuration
//////////////////////////////////////////////////////////////////////
#define PIXELS_COUNT                  10
#define LOADSELL_UNITS_PER_GRAMM      354

//////////////////////////////////////////////////////////////////////
// Calibration
//////////////////////////////////////////////////////////////////////
#define WEIGHT_THRESHOLD_EMPTY 100
#define WEIGHT_THRESHOLD_FULL 100


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
long contentWeight = 0;
long drankWeight = 0;
String currentState = "";

//////////////////////////////////////////////////////////////////////
// States
//////////////////////////////////////////////////////////////////////
State* s_Init = machine.addState(&stateInit);
State* s_Calibrate_Empty = machine.addState(&stateCalibrateEmpty);
State* s_Empty_Done = machine.addState(&stateEmptyDone);
State* s_Calibrate_Full = machine.addState(&stateCalibrateFull);
State* s_DrinkingIdle = machine.addState(&stateDrinkingIdle);
State* s_DrinkingLiftup = machine.addState(&stateDrinkingLiftup);
State* s_GlassEmpty = machine.addState(&stateGlassEmpty);

// ****** StateInit
int currentLedInit = 0;
int initStandByCount = 0;
bool standByMode = false;
void stateInit() {
  currentState = "Init";
  if (standByMode) {
    lightAllLeds(g_Pixels.Color(0,0,0));
  }
  else {
    showOnlySingleLED(currentLedInit++, g_Pixels.Color(0, 0, 255));
  }
  if (currentLedInit == PIXELS_COUNT) {
    currentLedInit = 0;
  }

  initStandByCount++;
  if (initStandByCount == (5 * 30)) {
    initStandByCount = 0;
    standByMode = true;
  }
}

// ****** StateCalibrationEmpty
bool calibrationEmptyDone = false;
long totalEmptyReads = 0;
int calibrateEmptyFrom = 0;
int calibrateEmptyUntil = 0;
int calibrateEmptyCounter = 0;
void stateCalibrateEmpty() {
  currentState = "CalibrateEmpty";
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
  if (++calibrateEmptyCounter == PIXELS_COUNT) {
    emptyWeight = (long)(totalEmptyReads/PIXELS_COUNT);
    calibrationEmptyDone = true;
  }
}

int currentLedEmptyDone = 0;
void stateEmptyDone() {
  currentState = "EmptyDone";
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
  currentState = "CalibrateFull";
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
  if (++calibrateFullCounter == PIXELS_COUNT) {
    fullWeight = (long)(totalFullReads/PIXELS_COUNT);
    contentWeight = fullWeight - emptyWeight;
    calibrationFullDone = true;
  }
}

// ****** StateDrinkingIdle
int currentPixelsShowing = 0;
float ratio = 1;
void stateDrinkingIdle() {
  currentState = "DrinkingIdle";
  long valueRaw = currentRead - emptyWeight;

  drankWeight = fullWeight - currentRead;
  if (drankWeight < 0) {
    drankWeight = 0;
  }
  ratio = round2((float)((float)drankWeight/(float)contentWeight));

  float ratioRed = ratio;
  float ratioGreen = 1 - ratioRed;

  if (ratio <= 1) {
    lightAllLeds(g_Pixels.Color(round(ratioRed * 255), round(ratioGreen * 255), 0));
  }
}

// ******* stateDrinkingLiftup
void stateDrinkingLiftup() {
  currentState = "DrinkingLiftup";
  lightAllLeds(g_Pixels.Color(0,0,255));
}

void stateGlassEmpty() {
  currentState = "GlassEmpty";
  lightAllLeds(g_Pixels.Color(0,0,0));
}

//////////////////////////////////////////////////////////////////////
// Transitions
//////////////////////////////////////////////////////////////////////

int t_initToCalibrateEmpty_Counter = 0;
bool t_initToCalibrateEmpty() {
  if (currentRead > WEIGHT_THRESHOLD_EMPTY) {
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
    if (currentRead > (WEIGHT_THRESHOLD_FULL + emptyWeight)) {
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

bool t_DrinkingIdleToLiftUp() {
  return currentRead < 40;
}

bool t_LiftUpToDrinkingIdle() {
  return (currentRead + 5) > emptyWeight;
}

int t_DrinkingIdleToInit_Counter = 0;
bool t_DrinkingIdleToEmpty() {
  if (ratio > 0.95) { // if, for 10 ticks, the difference between the currentRead and emptyWeight is less than 50g
    t_DrinkingIdleToInit_Counter++;

    if (t_DrinkingIdleToInit_Counter == 10) {
      t_DrinkingIdleToInit_Counter = 0;

      for (int i = 0; i < 5; i++) {
        lightAllLeds(g_Pixels.Color(255, 0, 0));
        delay(250);
        lightAllLeds(g_Pixels.Color(0, 0, 0));
        delay(250);
      }
      return true;
    }
    return false;
  }
  t_DrinkingIdleToInit_Counter = 0;
  return false;
}

int t_EmptyToInitCounter = 0;
bool t_EmptyToInit() {
  if (currentRead < 10) {
    t_EmptyToInitCounter++;
    if (t_EmptyToInitCounter == 10) {
      t_EmptyToInitCounter = 0;
      return true;
    }
  }
  return false;
}

int t_ToReset_Counter = 0;
bool t_ToReset() {
  if (machine.executeOnce) {
    t_ToReset_Counter = 0;
  }

  if (currentRead > (fullWeight + 1000)) { // 1kg above the full weight
    t_ToReset_Counter++;

    if (t_ToReset_Counter > 10) {
      return true;
    }
  }

  return false;
}


void setup() {
  Serial.begin(115200);
  g_LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  long tareTotal = 0;
  for (int i = 0; i < 20; i++) {
    tareTotal += g_LoadCell.read();
  }
  tareFactor = (long)(tareTotal/20);
  g_Pixels.begin();

  s_Init->addTransition(&t_initToCalibrateEmpty, s_Calibrate_Empty);
  s_Calibrate_Empty->addTransition(&t_EmptyCalibrationToEmptyDone, s_Empty_Done);
  s_Empty_Done->addTransition(&t_EmptyDoneToFull, s_Calibrate_Full);
  s_Calibrate_Full->addTransition(&t_FullToDrinking, s_DrinkingIdle);
  s_DrinkingIdle->addTransition(&t_DrinkingIdleToEmpty, s_GlassEmpty);
  s_DrinkingIdle->addTransition(&t_DrinkingIdleToLiftUp, s_DrinkingLiftup);
  s_DrinkingLiftup->addTransition(&t_LiftUpToDrinkingIdle, s_DrinkingIdle);
  s_GlassEmpty->addTransition(&t_EmptyToInit, s_Init);

  // Resets
  s_DrinkingIdle->addTransition(&t_ToReset, s_Init);
  s_DrinkingLiftup->addTransition(&t_ToReset, s_Init);
  //s_GlassEmpty->addTransition(&t_ToReset, s_Init);
  //s_Empty_Done->addTransition(&t_ToReset, s_Init);
  //s_Calibrate_Full->addTransition(&t_ToReset, s_Init);

}

void loop() {
  long thisRead = getReading();

  if (abs(thisRead - lastRead) > 10) { // only detect changes above 10g
    lastRead = currentRead;
    currentRead = thisRead;
    initStandByCount = 0;
    standByMode = false;
  }

  //Serial.println(thisRead);
  //Serial.println("fullWeight: " + String(fullWeight) + "     ");
  //Serial.println("emptyWeight: " + String(emptyWeight) + "      ");
  //Serial.println("drinkStatus: " + String(drankWeight) + " / " + String(contentWeight) + "        ");
  //Serial.println("currentRead: " + String(currentRead) + "       ");
  //Serial.println("ratio: " + String(ratio));
  
  Serial.println(String(ratio) + "\t\tempty: " + String(emptyWeight) + "\t\tfull: " + String(fullWeight) + "\t\tcurrent: " + String(currentRead) + "\t\tstate: " + currentState + "\t\tcontent: " + contentWeight + "\t\tdrank: " + drankWeight);
  machine.run();
  delay(200);
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
  return (long)((g_LoadCell.read() - tareFactor)/LOADSELL_UNITS_PER_GRAMM);
}

float round2(float var)
{
    float value = (int)(var * 100 + .5);
    return (float)value / 100;
}
