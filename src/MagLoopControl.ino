/************************************************************************
    **Arduino Uno R3 pin mapping**

    D0   <--> \n
    D1   <--> \n
    D2   <--> \n
    D3   <--> SLEEP  (RED) Big Easy Driver \n
    D4   <--> BLUEFRUIT_SPI_RST (via header pins) \n
    D5   <--> STP    (GRN) Big Easy Driver \n
    D6   <--> DIR    (YEL) Big Easy Driver \n
    D7   <-->  BLUEFRUIT_SPI_IRQ (via header pins) \n
    D8   <-->  BLUEFRUIT_SPI_CS (via header pins) \n
    D9   <--> \n
    D10  <--> \n
    D11  <-->  (pin shared with SPI_MOSI via jumper) \n
    D12  <-->  (pin shared with SPI_MISO via jumper) \n
    D13  <-->  (pin shared with SPI_SCK via jumper) \n
    GND  <--> \n
    AREF <--> \n
    D18  <--> \n
    D19  <--> \n

    A0   <--> \n
    A1   <--> \n
    A2   <--> \n
    A3   <--> \n
    A4   <--> \n
    A5   <--> \n

    GND  <--> GND    (BLK) Big Easy Driver

    **ISP 2x3 Connector on Red Board**

    D11  <-->  (BLUEFRUIT_SPI_MOSI with jumper on board) \n
    D12  <-->  (BLUEFRUIT_SPI_MISO with jumper on board) \n
    D13  <-->  (BLUEFRUIT_SPI_SCK with jumper on board)

    **Inventory the Blynk Virtual Pins**

    V0 Button - switch, Move \n
    V1 Button - switch, Direction \n
    V2 Button - push, Slew, motor set amount \n
    V3 Button - push, Nudge Down \n
    V4 Button - push, Nudge Up \n
    V5 Button - switch, SLEEP \n
    V6 Button - push, Speed SLOWER \n
    V7 Button - push, Speed FASTER \n
    V8 Value  - capPosition - current position of motor
    V9 \n
    V10 LED - motor is MOVING \n
    V11 LED - direction is UP \n
    V12 LED - direction is DOWN \n
    V13 Value - intervalIndexPin \n
    V14 LED - SLEEP \n
    V15 Button - Set Zero capacitor position

*************************************************************************/
#define DEBUG

#include "private_tokens.h" // Store AUTH_TOKEN outside of the sketch
// #include "StepperControl.h"

#ifdef DEBUG
  #include <SoftwareSerial.h>
  // BLERG BLYNK_PRINT needs to be at the top of the sketch!
  #define BLYNK_PRINT Serial
  // #define BLYNK_DEBUG        // Mor detailed prints
#endif


// Include the Blynk BLE stuff
#include <BlynkSimpleSerialBLE.h>

// // Libraries needed for Bluefruit LE Shield which uses SPI
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <SPI.h>

// Include timer functions
#include <SimpleTimer.h>
SimpleTimer timer; // Use simple time since I had trouble writing my own

// Define pins to be used with the BLE object
#define BLUEFRUIT_SPI_CS 8
#define BLUEFRUIT_SPI_IRQ 7
#define BLUEFRUIT_SPI_RST 4
#define BLUEFRUIT_VERBOSE_MODE true
// #define BLYNK_USE_DIRECT_CONNECT // This doesn't seem to work at the moment


// Declare a BLE object and define the parameters
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Local Constants
const int STP     = 5;  //  Pin to stepper driver to step the motor
const int DIR     = 6;  //  Pin to stepper driver to set motor direction
const int SLEEP   = 3;  //  Pin to stepper driver for LOW power.
                        //  mode delay >= 1ms on waking

// Used to slew and determine current position of the capacitor in
// number of microsteps
const int CAP_MIN  = 0;     //< Position of capacitor at its lowest position
const int CAP_MAX  = 4600;  //< Position of capacitor at its highest postion
int capPosition;            // Holds the current position of the capacitor

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"
#define BLYNK_GRAY      "#D3D3D3"

// Seems like SimpleTimer need to be declared outside of setup()
int moveMotor;
int printPosTimer;

int sleep;    // declared up here, not in setup()
int interval; // interval between each motor step


const int INTERVAL_MAX = 10;  // Define the number of possible motor speeds
int intervalIndex = 2;        //  Index for intervalArray[]

// Set up an array of possible intervals between each motor step
int intervalArray[INTERVAL_MAX] = {1000, 500, 200, 100, 50, 20, 10, 4, 2, 1};

// Set up some Blynk virtual LEDs
WidgetLED ledMove(V10);     //  Energized if motor is moving
WidgetLED ledUp(V11);       //  Energized if motordirection is up
WidgetLED ledDown(V12);     //  Energized if motor direction is down
WidgetLED ledSleep(V14);    //  Energized if motor is sleeping
int intervalIndexPin = 13;  //  Pin V13 for Value Widget
int capPositionPin   = 8;   //  Send posiion to app
WidgetLED ledDUMMY(V20);    //  test latency problems


/***********************************************************************
*
* SETUP()
*
*************************************************************************/

void setup() {

  //* Initialize Arduino pins  for Big Easy Driver
  pinMode(STP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(SLEEP, OUTPUT);

  #ifdef DEBUG
    // Blynk.begin(auth, Serial); // Use for USB serial connection
    Serial.begin(9600);
    ble.begin(BLUEFRUIT_VERBOSE_MODE);
    ble.factoryReset(); // Optional
  #endif

  // Blynk.begin(auth, Serial); // Use for USB serial connection
  ble.setMode(BLUEFRUIT_MODE_DATA);
  Blynk.begin(AUTH_TOKEN, ble);

  while (!Blynk.connect() && !ble.isConnected()) {
    #ifdef DEBUG
      Serial.println( F("Waiting!"));
    #endif
   }

   //* Set state for  Big Easy Driver
   digitalWrite(STP, LOW);
   digitalWrite(DIR, HIGH);   //  HIGH == Increase Resonate freq
   digitalWrite(SLEEP, LOW);  //  HIGH  = DEFAULT = energized
                              //  LOW   = SLEEP   = disable all
  // setState();

  // Set up timers
  moveMotor = timer.setInterval(interval, stepMotor);
  timer.disable(moveMotor);
  printPosTimer = timer.setInterval(1000, reportCapPosition );
  timer.disable(printPosTimer);

  #ifdef DEBUG
    // Serial.println( capPosition );
    Serial.println( F("setup() is done!"));
  #endif
}


void loop() {
  Blynk.run();
  timer.run();
}

//////////////////////////////////////////////////////////////////////////
//
// FUNCTIONS BELOW
//
/////////////////////////////////////////////////////////////////////////


/*********************************************
*
* SETUP initial state of Blynk widgets
*
* Call from BLYNK_CONNECTED() below.
*/

void setState() {
  #ifdef DEBUG
    Serial.println( F( "Enter setState()" ) );
  #endif

  // V0 Move Button
  Blynk.virtualWrite(V0, 0);
  // Blynk.setProperty(V0, "onBackColor", BLYNK_GRAY);
  Blynk.setProperty(V0, "offBackColor", BLYNK_GRAY);
  ledMove.off();

  // V5 SLEEP Button
  Blynk.virtualWrite(V5, 0);
  // Blynk.setProperty(V0, "onBackColor", BLYNK_GREEN);
  Blynk.setProperty(V0, "offBackColor", BLYNK_RED);
  ledSleep.on();

  Blynk.virtualWrite(V13, intervalIndex);
  // ledDown.off();
  // ledUp.off();
  // ledMove.off();
  // ledSleep.off();
  // ledDUMMY.off();
  // Blynk.syncAll();

}


////////////////////////////////////////////
//
//  SET CAPACITOR POSITION
//
//   = least capacitance
//

void updateCapPosition() {
  // #ifdef DEBUG
  //   Serial.println( F("Enter updateCapPosition()") );
  // #endif

  if ( digitalRead(DIR) == 0  ) {
    capPosition++;
  }
  else if ( digitalRead(DIR) == 1 ) {
    capPosition--;
  }
}


///////////////////////////////////////////////
//
// REPORT CAPACITOR POSITION
//

void reportCapPosition() {
  #ifdef DEBUG
    Serial.println( F("Enter reportCapPosition()."));
  #endif

  Blynk.virtualWrite( capPositionPin, capPosition );

  // #ifdef DEBUG
  //   Serial.print( F("Cap position: "));
  //   Serial.println( capPosition );
  // #endif
}

/***********************************
*
* TEST for CAP End of travel
*
*/

int testCapEnd () {
  // #ifdef DEBUG
  //   Serial.println( F("Enter testCapEnd()"));
  // #endif

  if ( capPosition == CAP_MIN ) {
    // CAP_MIN = least capacitance
    return 0;
  }
  else if ( capPosition ==  CAP_MAX ) {
    // CAP_MAX = greatest capacitance
    return 1;
  }
  else {
    return 2;
  }
}

///////////////////////////////////////////////
//
// GET DIRECTION
//

bool getMotorDir() {
  bool dir = digitalRead(DIR);

  // #ifdef DEBUG
  //   Serial.print( F("Direction: "));
  //   Serial.println( dir );
  // #endif

  return dir;
}

//////////////////////////////////////////////
//
//  CHANGE DIRECTION
//
//  0 = DOWN
//  1 = UP
//

void setDirection (bool dirNew) {
  #ifdef DEBUG
    Serial.println( F("Enter setDirection()") );
  #endif

  if (dirNew != digitalRead(DIR))
  {
    #ifdef DEBUG
      Serial.println( F("Change motor DIRECTION") );
    #endif

    digitalWrite(DIR, dirNew);
    Blynk.virtualWrite(V1, dirNew );
    #ifdef DEBUG
      Serial.println( F("") );
    #endif
  }
  // Soome VLEDs to help see what is going one
  if ( dirNew )
  {
    #ifdef DEBUG
      Serial.println( F("New direction is UP") );
    #endif

    ledUp.on();
    ledDown.off();
    ledDUMMY.on(); // Used to overcome latency problem
  }
  else if (!dirNew)
  {
    #ifdef DEBUG
      Serial.println( F("New direction is DOWN") );
    #endif

    ledUp.off();
    ledDown.on();
    ledDUMMY.on(); // Used to overcome latency problem
  }
}


/*****************************************

 START / STOP MOTOR

 Called by BLYNK_READ(V0)

****************************************/

bool startStopMotor(bool x = 0) {
  /*******************************
    x = param.asInit()
      = 0 = STOP
      = 1 = MOVE

    SLEEP = LOW   = 0
          = HIGH  = 1, Energized
  *******************************/
  #ifdef DEBUG
    Serial.println( F("Enter startStopMotor().") );
    // Serial.print( F("Cap state is:") );
    // Serial.println( testCapEnd() );
  #endif

  if ( !digitalRead(SLEEP) ) {
    #ifdef DEBUG
      Serial.println( F("Shhhhhh, sleeping!") );
    #endif
    return 0;
  }
  if ( x ) {
    #ifdef DEBUG
      Serial.println( F("START moving the motor.") );
    #endif

    // moveMotor calls stepMotor()
    timer.enable(moveMotor);
    timer.enable(printPosTimer);

    //  Send state to Blynk app
    //  V0 - MOVE button
    Blynk.virtualWrite(V0, 1 );
    Blynk.setProperty(V0, "color", BLYNK_GREEN);
    ledMove.on();

    // printPosTimer = timer.setInterval(1000, reportCapPosition );
    // timer.enable(printPosTimer);
    // ledDUMMY.on(); // Used to overcome latency problem
  }
  else if ( !x ) {
    #ifdef DEBUG
      Serial.println( F("STOP moving the motor") );
    #endif

    timer.disable(moveMotor);
    timer.disable(printPosTimer);

    ledMove.off();

    //  V0 - MOVE button
    Blynk.virtualWrite(V0, 0 );
    Blynk.setProperty(V0, "color", BLYNK_GRAY);

    reportCapPosition();
    ledDUMMY.on(); // Used to overcome latency problem
  }
}


//////////////////////////////////////////////
//
// AT END CHANGE DIRECTION
//

void changeDirAtEnd () {
  #ifdef DEBUG
    Serial.println( F("Enter changeDirAtEnd()") );
  #endif

  if ( capPosition == CAP_MIN && digitalRead(DIR) == 1 ) {
    #ifdef DEBUG
      Serial.println( F("FEED ME I'm at the bottom") );
    #endif

    startStopMotor(0);

  }
  else if ( capPosition == CAP_MAX && digitalRead(DIR) == 0 ) {
    #ifdef DEBUG
      Serial.println( F("FEED ME I'm at the top") );
    #endif

    startStopMotor(0);
  }
}


/*********************************************

  STEP MOTOR

  Toggle stepper motor state with each timer cycle
  Two cycles of the time = one complete step of the motor

  Stop the mptor if it reaches the end

*/

void stepMotor() {
  // #ifdef DEBUG
  //   Serial.println( F("Enter stepMotor()") );
  // #endif

  if (  ( capPosition >= CAP_MIN+1 && digitalRead(DIR) == 1 ) ||
        ( capPosition <= CAP_MAX-1 && digitalRead(DIR) == 0 )
      ) {

    digitalWrite(STP, !digitalRead(STP));
    //* Yes, we're using a delay here but it's only 10 uSec
    delayMicroseconds(10);
    digitalWrite(STP, !digitalRead(STP));
    updateCapPosition();
  }
  else {
    // Serial.println( F("I won't step the motor") );
    timer.disable(printPosTimer);
    reportCapPosition();
    startStopMotor(0);
  }
}


/****************************
*
*  FASTER or SLOWER
*
*  Function to change the interval between each
*  step of the stepper motor
*/

void changeInterval(bool x) {
  #ifdef DEBUG
    Serial.println( F("Enter changeInterval()") );
    // Serial.print( F("Current interval is ") );
    // Serial.println( intervalIndex );
  #endif

  bool newInterval = 0;
  bool enabled = timer.isEnabled(moveMotor);
  if ( !x  && intervalIndex > 0)
  {
    intervalIndex--;
    newInterval = 1;
  }
  else if ( x  && intervalIndex < INTERVAL_MAX-1 )
  {
    intervalIndex++;
    newInterval = 1;
  }

  if ( newInterval ) {
    #ifdef DEBUG
      // Serial.print( F("New interval is ") );
      // Serial.println( intervalIndex );
    #endif

    timer.deleteTimer(moveMotor);
    moveMotor = timer.setInterval(intervalArray[intervalIndex], stepMotor);
    Blynk.virtualWrite( V13, intervalIndex );

    if ( enabled ) {
      timer.enable(moveMotor);
    }
  }
}


/************************************

    SLEW MOTOR

    Slew the motor a set amount at the indicated speed

    i = milliseconds between each step
    x = number of steps to take

*/

void motorSlew(int i, int x) {
  // Use simpleTimer, that's what it's there for
  timer.setTimer(i, stepMotor, x);
  // Serial.println( capPosition );
}


/**************************************************************************
*
*     BLYNK FUNCTIONS BELOW HERE
*
**************************************************************************/


BLYNK_CONNECTED() {
  #ifdef DEBUG
    Serial.println( F("BLYNK__CONNECTED...") );
  #endif

  setState();
}


/***********************************
*
*  V0 MOVE
*  BUTTON, switch
*
*/

BLYNK_WRITE(V0) {

  /*******************************
    HIGH  = 1 = move motor
    LOW   = 0 = stop motor
  *******************************/

  #ifdef DEBUG
    Serial.println( F("App event: V0 MOVE button") );
  #endif

  // Remember 1 = Energized
  if ( !digitalRead(SLEEP) ) {
    #ifdef DEBUG
      Serial.println( F("Don't MOVE, I'm sleeping.") );
    #endif

    Blynk.setProperty(V0, "onBackColor", BLYNK_GRAY);

  }
  else if ( digitalRead(SLEEP) ) {
    Blynk.virtualWrite(V0, 1 );
    Blynk.setProperty(V0, "onBackColor", BLYNK_GREEN);
    startStopMotor( param.asInt() );
  }

}


/*******************************************
*
*   V1 DIRECTION
*   BUTTON, switch
*   0 = LOW
*   1 = HIGH
*/

/**********************************************
* Direction Button Widget to change motor direction
*
*  So lets figure out how this works
*
*   Perspective used will be from the behind the stepper motor
*   looking toward the capacitor.
*
*   Butterfly capaitors typically have no stops and can move
*   bidirectionally, we will start with the capacitor set to
*   its lowest Value.
*
*   In a magnetic loop antenna capaciance is inverse to resonant frequency
*     Increase capacitance --> decease resonant frequency
*     Decrease capacitance --> increase resonant frequency
*
*   Stepper motors also have no stops and are bidirectional.
*
*   Where things are now:
*   Blynk Button Widget is set up as a switch:
*     LOW   = 0 (DEFAULT)
*           = 0
*           = DOWN (label)
*           = CLOCKWISE rotation of capacitor
*           = INCREASES capacitance
*           = DECREASES resonate frequency
*     HIGH  = 1
*           = UP (label)
*           = COUNTER-CLOCKWISE rotation of capacitor
*           = DECREASES capacitance
*           = INCREASES resonate frequency
*/

BLYNK_WRITE(V1) {
  #ifdef DEBUG
    Serial.println( F("App event: V1 DIRECTION button") );
  #endif

  setDirection( param.asInt() );
}


/********************************************
*
*   V6 SLOWER
*   BUTTON, push
*   0 = LOW
*   1 = HIGH
*/

BLYNK_WRITE(V6) {
  #ifdef DEBUG
    Serial.println( F("App event: V6 SLOWER INTERVAL button") );
  #endif

  if (param.asInt() )
  {
    changeInterval( 0 );
  }
}


/********************************************
*
*   V6 FASTER
*   BUTTON, push
*   0 = LOW
*   1 = HIGH
*/

BLYNK_WRITE(V7) {
  #ifdef DEBUG
    Serial.println( F("App event: V7 FASTER INTERVAL button") );
  #endif

  if (param.asInt() )
  {
    changeInterval( 1 );
  }
}


/********************************************
*
*   V3 NUDGE DOWN
*   BUTTON, push
*   0 = LOW
*   1 = HIGH
*
*   Step motor once
*/

BLYNK_WRITE(V3) {
  #ifdef DEBUG
    Serial.println( F("App event: V3 NUDE DOWN button") );
  #endif

  if (param.asInt()) {
    if (digitalRead(DIR) != LOW) {
     digitalWrite(DIR, LOW);
     ledDown.on();
     ledUp.off();
     delayMicroseconds(500);
    }
  }
  stepMotor();
  reportCapPosition();
}


/********************************************
*
*   V4 NUDGE UP
*   BUTTON, push
*   0 = LOW
*   1 = HIGH
*
*   Step motor once
*/

BLYNK_WRITE(V4) {
  #ifdef DEBUG
    Serial.println( F("App event: V4 NUDE UP button") );
  #endif

  if (param.asInt())
  {
    if (digitalRead(DIR) != HIGH)
    {
     digitalWrite(DIR, HIGH);
     ledUp.on();
     ledDown.off();
    //  delayMicroseconds(500);
    }
  }
  stepMotor();
  reportCapPosition();
}

/********************************************
*
*   V15 ZERO
*   BUTTON, push
*   0 = LOW
*   1 = HIGH
*
*   Set capPosition to CAP_MIN
*/

BLYNK_WRITE(V15) {
  if ( param.asInt() )
  {
    capPosition = CAP_MIN;
    reportCapPosition();
  }
}


/************************************//*
* V2
* Button widget
* slew motor a set amount
*
*/

BLYNK_WRITE(V2) {
  #ifdef DEBUG
    Serial.println( F("App event: V2 SLEW") );
  #endif

  if (param.asInt()) {
    // Serial.println( F("You slew me!"));
    motorSlew(1, 4600);
    // timer.setTimer(2, stepMotor, 19200);
    reportCapPosition();
  }
}

/************************************************
  V5 -- SLEEP
  Button widget LOW (on) will put motor to SLEEP

  Big Easy Driver SLEEP pin is HIGH as DEFAULT
  HIGH == awake, energized ready to go
  Low == SLEEP low power-mode
*/

BLYNK_WRITE(V5) {
  #ifdef DEBUG
    Serial.println( F( "App event: V5 SLEEP" ) );
    Serial.print( F( "param.asInt = " ) );
    Serial.println( param.asInt() );
  #endif

  // Energize if (param == 1 and SLEEP == LOW )
  if (param.asInt() && !digitalRead(SLEEP)) {
    digitalWrite(SLEEP, HIGH);
    ledSleep.off();

    // Blynk.setProperty(V0, "onBackColor", BLYNK_GREEN);
    Blynk.setProperty(V0, "offBackColor", BLYNK_RED);

    ledDUMMY.on(); // Used to overcome latency problem
  }
  // Go to Sleep if (param == 0 and SLEEP == HIGH )
  else if (!param.asInt() && digitalRead(SLEEP))
  {
    if (timer.isEnabled(moveMotor))
    {
      timer.disable(moveMotor);
    }

    digitalWrite(SLEEP, LOW);

    Blynk.virtualWrite(V0, 0);
    // Blynk.setProperty(V0, "onBackColor", BLYNK_GRAY);
    Blynk.setProperty(V0, "offBackColor", BLYNK_GRAY);

    // ledMove.off();
    ledSleep.on();
    ledDUMMY.on(); // Used to overcome latency problem
  }
}
