//////////////////////////////////////////////
//
// StepperControl.ino
//
// Control stepper motor with Blynk
//
// Blynk Project is called
//   Magloop Control
//

// Arduino Uno R3 pin mapping
//
//  D0   <-->
//  D1   <-->
//  D2   <-->
//  D3   <--> SLEEP  (RED) Big Easy Driver
//  D4   <-->  BLUEFRUIT_SPI_RST (via header pins)
//  D5   <--> STP    (GRN) Big Easy Driver
//  D6   <--> DIR    (YEL) Big Easy Driver
//  D7   <-->  BLUEFRUIT_SPI_IRQ (via header pins)
//  D8   <-->  BLUEFRUIT_SPI_CS (via header pins)
//  D9   <-->
//  D10  <-->
//  D11  <-->  (pin shared with SPI_MOSI via jumper)
//  D12  <-->  (pin shared with SPI_MISO via jumper)
//  D13  <-->  (pin shared with SPI_SCK via jumper)
//  GND  <-->
//  AREF  <-->
//  D18  <-->
//  D19  <-->
//
//  A0   <-->
//  A1   <-->
//  A2   <-->
//  A3   <-->
//  A4   <-->
//  A5   <-->
//
//  GND <--> GND    (BLK) Big Easy Driver
//
// ISP 2x3 Connector
//  D11  <-->  (BLUEFRUIT_SPI_MOSI with jumper on board)
//  D12  <-->  (BLUEFRUIT_SPI_MISO with jumper on board)
//  D13  <-->  (BLUEFRUIT_SPI_SCK with jumper on board)

// Inventory the Blynk Virtual Pins.
// V0 Button - Move
// V1 Button - Direction
// V2
// V3 Button - Nudge Down
// V4 Button - Nudge Up
// V5 Button - SLEEP
// V6 Button - push Speed SLOWER
// V7 Button - push Speed FASTER
// V8
// V9
// V10 LED - motor is MOVING
// V11 LED - direction is UP
// V12 LED - direction is DOWN
// V13 Value - Step interval
// V14 LED - SLEEP
//////////////////////////////////////////////

#include "private_tokens.h"
// extern char AUTH_TOKEN[]; // Auth Token for "Magloop Control".

//////////////////////////////////////////////
//
// BLERG These lines need to be at the top of the sketch!
//
// Use BLYNK_LOG() to print to serial monitor
// using printf style of formating.
#define BLYNK_PRINT Serial // Defines the object that is used for printing
// #define BLYNK_DEBUG        // Optional, this enables more detailed prints


// Adafruit Bluefruit LE Shield which uses SPI
#include <SPI.h>
#include <SoftwareSerial.h>
#include <BlynkSimpleSerialBLE.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <SimpleTimer.h>

// See above for complete list of pins used
#define BLUEFRUIT_SPI_CS 8
#define BLUEFRUIT_SPI_IRQ 7
#define BLUEFRUIT_SPI_RST 4
#define BLUEFRUIT_VERBOSE_MODE true
// #define BLYNK_USE_DIRECT_CONNECT // This doesn't seem to work at the moment

SimpleTimer timer; // Use simple time since I had trouble writing my own

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Local Constants
const int STP     = 5;  // Arduino --> step the motor
const int DIR     = 6;  // Arduino --> motor direction
const int SLEEP   = 3;   //  Arduino --> SLP = LOW power mode delay >= 1ms on waking

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"

// Set up some global variables
int move;   // Seems like SimpleTimer need to be
int sleep;  // declared up here, not in setup()
int interval = 500; // Step interval for motor

const int stepsMax = 10;
int steps[stepsMax] = {1000, 500, 200, 100, 50, 20, 10, 4, 2, 1};
int stepsCounter = 5;

// Set up some Blynk virtual LEDs
WidgetLED ledMove(V10); //  motor is moving
WidgetLED ledUp(V11);   //  direction is up
WidgetLED ledDown(V12); //  direction is down
WidgetLED ledSleep(V14); //
int stepIntervalPin = 13; // Pin V13 for Value Widget
WidgetLED ledDUMMY(V20); // test latency problems

void setup()
{
  // Initialize Arduino pins and logic states
  // Along with any the state of complementary Blynk widgets
  pinMode(STP, OUTPUT); // BED step stepper motor
  digitalWrite(STP, LOW);
  pinMode(DIR, OUTPUT); // BED stepper motor direction
  digitalWrite(DIR, HIGH); // HIGH == Resonate freq UP
  pinMode(SLEEP, OUTPUT); // BED stepper motor direction
  digitalWrite(SLEEP, LOW); // DEFAULT is HIGH != SLEEP

  Blynk.virtualWrite(stepIntervalPin, 100);


  // Blynk will work through Serial USB
  Serial.begin(9600);

  // Blynk.begin(auth, Serial); // Use for USB serial connection
  Blynk.begin(AUTH_TOKEN, ble);
  ble.begin(BLUEFRUIT_VERBOSE_MODE);
  ble.factoryReset(); // Optional
  ble.setMode(BLUEFRUIT_MODE_DATA);

  while (!Blynk.connect() && !ble.isConnected())
  {
     // Wait until connected
   }
  ledMove.off();
  ledDown.off();
  ledUp.on();
  ledSleep.off();
  ledDUMMY.off();

  // Set up timers
  // setSerial(milliseconds, function)
  move = timer.setInterval(interval, motorTwoStep);
  timer.disable(move);

}

void loop()
{
  Blynk.run();
  timer.run();
}

/*
 Functions below
*/

/////////////////////////////
// motorTwoStep
//
// Toggle stepper motor state with each timer cycle
// Two cycles of the time = one complete step of the motor
//
// Yes, we're using a delay here but it's only 10 uSec
void motorTwoStep()
{
  digitalWrite(STP, !digitalRead(STP));
  delayMicroseconds(10);
  digitalWrite(STP, !digitalRead(STP));
}

void intervalChange(int i)
{
  // Change interval of step
  // BLYNK_LOG("Counter: %i - Interval: %i", stepsCounter, i);
  Blynk.virtualWrite(stepIntervalPin, i);
  ledDUMMY.on(); // Used to overcome latency problem
  bool wasEnabled = timer.isEnabled(move);
  timer.deleteTimer(move);
  move = timer.setInterval(i, motorTwoStep);
  if (!wasEnabled)
  {
    timer.disable(move);
  }
}


//////////////////////////////////
// V0 -- MOVE
// Button widget HIGH (on) will step the notor
//
BLYNK_WRITE(V0)
{
  if (param.asInt())
  {
    timer.enable(move);
    ledMove.on();
    Blynk.setProperty(V0, "color", BLYNK_YELLOW);
    ledDUMMY.on(); // Used to overcome latency problem
  }
  else
  {
    timer.disable(move);
    ledMove.off();
    Blynk.setProperty(V0, "color", "#990000");
    ledDUMMY.on(); // Used to overcome latency problem
  }
  // Blynk.setProperty(0, "color", BLYNK_YELLOW);
}

////////////////////////////////////
// V1 -- Virtual Pin 1
// Button Widget to change direction
//
// LOW  = DEFAULT
//      = CLOCKWISE (relative to the capacitor)
//      = DOWN (relative to resonate frequency)
// HIGH = COUNTER-CLOCKWISE (relative to the capacitor)
//      = UP (relative to resonate frequency)
//
BLYNK_WRITE(V1)
{
  int dirNew = param.asInt(); // assign event value to variable
  if (dirNew != digitalRead(DIR))
  {
   digitalWrite(DIR, dirNew);
  }

  // Do we really need virtualLEDs if things are working?
  // They don't really mean anything at the momnet,
  //  at least in relation to the change i frequency
  // Ideally they would be useful so leave in for now.
  if (dirNew)
  {
    ledUp.on();
    ledDown.off();
  }
  else if (!dirNew)
  {
    ledUp.off();
    ledDown.on();
  }
}

//////////////////////////////////
// V6 -- Speed SLOWER
// Button widget HIGH will slow down motor speed  ONCE
//
BLYNK_WRITE(V6)
{
  if (param.asInt() && stepsCounter > 0)
  {
    intervalChange(steps[--stepsCounter]);
  }
}

//////////////////////////////////
// V7 -- Speed FASTER
// Button widget HIGH will speed up motor speed  ONCE
//
BLYNK_WRITE(V7)
{
  if (param.asInt() && stepsCounter < stepsMax - 1)
  {
    intervalChange(steps[++stepsCounter]);
  }
}

//////////////////////////////////
// V3 -- Nudge Down
// Button widget DOWN will step the notor DOWN ONCE
//
BLYNK_WRITE(V3)
{
  if (param.asInt())
  {
    if (digitalRead(DIR) != LOW)
    {
     digitalWrite(DIR, LOW);
     ledDown.on();
     ledUp.off();
     delayMicroseconds(500);
    }
  }
  motorTwoStep();

}

//////////////////////////////////
// V4 -- Nudge UP
// Button widget HIGH will step the notor UP ONCE
//
BLYNK_WRITE(V4)
{
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
  motorTwoStep();
}

//////////////////////////////////
// V5 -- SLEEP
// Button widget LOW (on) will put motor to SLEEP
//
// Big Easy Driver SLEEP pin is HIGH as DEFAULT
// HIGH == awake, energized ready to go
// Low == SLEEP low power-mode
//
BLYNK_WRITE(V5)
{
  // the LED stuff is screwy

  if (param.asInt())
  {
    // digitalWrite(SLEEP, !digitalRead(SLEEP));
    digitalWrite(SLEEP, HIGH);
    ledSleep.off();
    ledDUMMY.on(); // Used to overcome latency problem
  }
  else
  {
    // digitalWrite(SLEEP, !digitalRead(SLEEP));
    digitalWrite(SLEEP, LOW);
    ledSleep.on();
    ledDUMMY.on(); // Used to overcome latency problem
  }

  // if (digitalRead(SLEEP)) // HIGH = ready to go
  // {
  //   ledSleep.off();
  // }
  // else if (!digitalRead(SLEEP)) // LOW == low power-mode
  // {
  //   ledSleep.on();
  // }
}
