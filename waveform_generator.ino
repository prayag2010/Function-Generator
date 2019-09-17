/*
   mode select
   freq and duty cycle change
   function gen integrate
   timer integrate
   proper ui
   1 khz output at boot!
   change globalfreq according
   implement max value constraint
*/

#include <Ticker.h>
#include <LiquidCrystal.h>
#include <Rotary.h>
#include <TimerOne.h>
#include <SPI.h>
#include <SparkFun_MiniGen.h>

#define button A0

#define selectDigitMode 0
#define changeDigitMode 1

#define CW 1
#define CCW 0

#define sineMode 0
#define squareMode 1
#define triangleMode 2

#define buttonHoldDelay 500

#define globalArraySize 17

MiniGen gen;

int currentMode = sineMode;

const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = A1; // d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int mode = selectDigitMode;
int digitSelected = 0;

//7 - minigen, 7 - timer, 3 - duty cycle
//0 - 6, 7 - 13, 14 - 16
static int globalArray[globalArraySize] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 5, 0};
static boolean globalArrayBlink[globalArraySize];

//100 Hz initial
static float mgFreqDigits[7];
static int timerFreqDigits[7];
static int dutyCycleDigits[3];

static float mgFreq;
static long timerFreq;
static int dutyCycle;

boolean blinkBool = false;

long freq = 0;

Rotary r = Rotary(2, 3);
boolean encoderDir = CW;
boolean encoderChangeAddressed = true;

void blinkDigit(void);
Ticker digitBlinkTicker(blinkDigit, 500);

unsigned long riseButtonMillis = 0;
unsigned long fallButtonMillis = 0;
boolean encoderPressed = false;

const int timerPin = 9;

void setup() {
  Serial.begin(115200);

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();

  lcd.begin(20, 4);

  lcd.setCursor(5, 0);
  lcd.print("Function");
  lcd.setCursor(5, 1);
  lcd.print("Generator");
  lcd.setCursor(1, 2);
  lcd.print("3 SIN 0.15 SQ MHz");
  lcd.setCursor(0, 3);
  lcd.print("0.5 Tri | 1 Mhz SQ");

  delay(3000);
  lcd.clear();

  lcd.setCursor(4, 0);
  lcd.print("Hello world!");
  lcd.setCursor(0, 1);
  lcd.print("T.E ET-1 Atharva COE");
  lcd.setCursor(3, 2);
  lcd.print("Mini Project 2");
  lcd.setCursor(3, 3);
  lcd.print("Sem 6, 2017-18");

  delay(3000);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Group Members:");
  lcd.setCursor(0, 1);
  lcd.print("Shubham Ambokar - 1");
  lcd.setCursor(0, 2);
  lcd.print("Shweta Bhale - 5");
  lcd.setCursor(0, 3);
  lcd.print("Prayag Desai - 16");

  delay(3000);
  lcd.clear();

  pinMode(button, INPUT_PULLUP);

  digitBlinkTicker.start();

  lcd.setCursor(1, 1);
  lcd.cursor();

  gen.reset();
  delay(200);
  gen.setMode(MiniGen::SINE);
  gen.setFreqAdjustMode(MiniGen::FULL);

  Timer1.initialize(10000);
  Timer1.pwm(timerPin, 512);
}

void loop() {

  constrainDigits();
  if (!digitalRead(button) && !encoderPressed) {
    encoderPressed = true;
    fallButtonMillis = millis();
  }

  if (digitalRead(button) && encoderPressed) {
    encoderPressed = false;
    riseButtonMillis = millis();
    if (riseButtonMillis - fallButtonMillis >= buttonHoldDelay) {
      Serial.println("Hold");
      currentMode++;
      if (currentMode == sineMode)
        gen.setMode(MiniGen::SINE);
      else if (currentMode == squareMode)
        gen.setMode(MiniGen::SQUARE);
      else if (currentMode == triangleMode)
        gen.setMode(MiniGen::TRIANGLE);
      else if (currentMode >= 3) {
        gen.setMode(MiniGen::SINE);
        currentMode = 0;
      }
    }
    else {
      Serial.println("Pressed");
      encoderPressEvent();
    }
  }

  digitBlinkTicker.update();
  digitSelected = constrain(digitSelected, 0, globalArraySize - 1);

  lcdUpdate();
  createDigits();

  if (!encoderChangeAddressed) {
    encoderChangeAddressed = true;
    encoderTurnEvent();
  }

}

void encoderTurnEvent(void)
{
  if (mode == selectDigitMode) {
    if (encoderDir)
      digitSelected++;
    else
      digitSelected--;
  }
  else if (mode == changeDigitMode) {
    if (encoderDir)
      globalArray[digitSelected]++;
    else
      globalArray[digitSelected]--;
  }
  constrainDigits();
}

void encoderPressEvent(void)
{
  if (mode == selectDigitMode) {
    Serial.println("Digit Selected");
    Serial.println(digitSelected);
    mode = changeDigitMode;
    lcd.noCursor();
    for (int i = 0; i < globalArraySize; i++) {
      if (i == digitSelected)
        globalArrayBlink[i] = true;
      else
        globalArrayBlink[i] = false;
    }
  }
  else if (mode == changeDigitMode) {
    globalArrayBlink[digitSelected] = false;

    if (globalArray[0] >= 3)
      for (int i = 1; i < 7; i++)
        globalArray[i] = 0;
    if (globalArray[7] >= 1)
      for (int i = 8; i < 14; i++)
        globalArray[i] = 0;
    if (globalArray[14] >= 1)
      for (int i = 15; i < 17; i++)
        globalArray[i] = 0;

    mode = selectDigitMode;

    unsigned long freqReg = gen.freqCalc(mgFreq);
    gen.adjustFreq(MiniGen::FREQ0, freqReg);


    float timePeriod = 1000000;
    Serial.println(timePeriod);
    timePeriod /= timerFreq;
    Serial.println(timePeriod);
    Timer1.initialize(timePeriod);//in us
    delay(10);
    int temp = map(dutyCycle, 0, 100, 0, 1023);
    Timer1.pwm(timerPin, temp);
    Serial.println(temp);
  }
}

void createDigits(void)
{
  for (int i = 0; i < 7; i++)
    mgFreqDigits[i] = (float)globalArray[i];

  for (int i = 7; i < 14; i++)
    timerFreqDigits[i - 7] = (int)globalArray[i];

  for (int i = 14; i < 17; i++)
    dutyCycleDigits[i - 14] = (int)globalArray[i];

  dutyCycle = 0;
  for (int i = 2; i >= 0; i--) {
    dutyCycle += dutyCycleDigits[i] * (int)ceil(pow(10, 2 - i));
  }
  Serial.println("Duty Cycle:");
  Serial.println(dutyCycle);

  timerFreq = 0;
  for (int i = 6; i >= 0; i--) {
    timerFreq += timerFreqDigits[i] * (long)ceil(pow(10, 6 - i));
  }
  Serial.println("Timer Freq");
  Serial.println(timerFreq);

  mgFreq = 0;
  for (int i = 6; i >= 0; i--) {
    mgFreq += mgFreqDigits[i] * (float)((long)ceil(pow(10, 6 - i)));
  }
  Serial.println("MG Freq");
  Serial.println(mgFreq);
}


void lcdUpdate(void)
{
  //0 - 6, 7 - 13, 14 - 16
  for (int i = 6; i >= 0; i--) {
    if (globalArrayBlink[i]) {
      if (blinkBool) {
        lcd.setCursor(i, 1);
        lcd.print(" ");
      }
      else {
        lcd.setCursor(i, 1);
        lcd.print(globalArray[i]);
      }
    }
    else {
      lcd.setCursor(i, 1);
      lcd.print(globalArray[i]);
    }
  }
  for (int i = 13; i >= 7; i--) {
    if (globalArrayBlink[i]) {
      if (blinkBool) {
        lcd.setCursor(i - 7, 2);
        lcd.print(" ");
      }
      else {
        lcd.setCursor(i - 7, 2);
        lcd.print(globalArray[i]);
      }
    }
    else {
      lcd.setCursor(i - 7, 2);
      lcd.print(globalArray[i]);
    }
  }
  for (int i = 16; i >= 14; i--) {
    if (globalArrayBlink[i]) {
      if (blinkBool) {
        lcd.setCursor(i - 14, 3);
        lcd.print(" ");
      }
      else {
        lcd.setCursor(i - 14, 3);
        lcd.print(globalArray[i]);
      }
    }
    else {
      lcd.setCursor(i - 14, 3);
      lcd.print(globalArray[i]);
    }
  }



  lcd.setCursor(0, 0);
  lcd.print("Waveform:");
  lcd.setCursor(12, 0);
  if (currentMode == 0)
    lcd.print("Sine    ");
  else if (currentMode == 1)
    lcd.print("Square  ");
  else if (currentMode == 2)
    lcd.print("Triangle");

  lcd.setCursor(8, 1);
  lcd.print("Hz");
  lcd.setCursor(11, 1);
  lcd.print("Freq SST");

  //  lcd.setCursor(0, 2);
  //  lcd.print("1234567");
  lcd.setCursor(8, 2);
  lcd.print("Hz Freq SQW");

  lcd.setCursor(7, 3);
  lcd.print("Duty Cycle");
  lcd.setCursor(4, 3);
  lcd.print("%");

  if (mode == selectDigitMode) {
    //    lcd.setCursor(digitSelected, 1);
    if (digitSelected >= 0 && digitSelected < 7)
      lcd.setCursor(digitSelected, 1);
    if (digitSelected >= 7 && digitSelected < 14)
      lcd.setCursor(digitSelected - 7, 2);
    if (digitSelected >= 14 && digitSelected < 17)
      lcd.setCursor(digitSelected - 14, 3);
    lcd.cursor();
    delay(100);
    lcd.noCursor();
  }
}

void blinkDigit(void)
{
  blinkBool = !blinkBool;
}

void constrainDigits(void)
{
  //0 - 6, 7 - 13, 14 - 16
  globalArray[0] = (int)constrain(globalArray[0], 0, 3);
  for (int i = 1; i < 7; i++)
    globalArray[i] = (int)constrain(globalArray[i], 0, 9);

  globalArray[7] = (int)constrain(globalArray[7], 0, 1);
  for (int i = 8; i < 14; i++)
    globalArray[i] = (int)constrain(globalArray[i], 0, 9);

  globalArray[14] = (int)constrain(globalArray[14], 0, 1);
  for (int i = 14; i < 17; i++)
    globalArray[i] = (int)constrain(globalArray[i], 0, 9);
}


ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result == DIR_NONE) {
    // do nothing
  }
  else if (result == DIR_CW) {
    Serial.println("ClockWise");
    encoderChangeAddressed = false;
    encoderDir = CW;
  }
  else if (result == DIR_CCW) {
    Serial.println("CounterClockWise");
    encoderChangeAddressed = false;
    encoderDir = CCW;
  }
}

