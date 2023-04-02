// Released under Unlicense (Public Domain), see LICENSE

#include <Wire.h> // Arduino built-in
#include <LiquidCrystal_I2C.h> // https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
#include <EEPROM.h> // Arduino built-in
#include <ClickEncoder.h> // https://github.com/0xPIT/encoder
#include <TimerOne.h> // https://github.com/PaulStoffregen/TimerOne

// Pin definition
#define PIN_ENC_A A2
#define PIN_ENC_B A1
#define PIN_ENC_BTN A3
// Note that pins 9 and 10 are reserved by Timer1
#define PIN_PWM 11
#define PIN_WATER_SENSOR A0
// Pins A4, A5 are used by the display

#define ENC_HOLDTIME 2000

// Encoder related
ClickEncoder *encoder;
void timerIsr() { encoder->service(); }

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

// Current output percentage
unsigned int output = 0;
// Last written default output percentage
unsigned int eepromWritten = 1000, eepromAddr = 10;
// Use this to schedule a redraw of the display by setting it to millis() + delay
unsigned long redrawTime = 0;
// If true, the display will be redrawn in the main loop
// This is only done when necessary to reduce flickering
bool redraw;
// Safety override for the water sensor
// If true, the water sensor check will be ignored
bool safetyOverride = false;
// If the encoder button is held, toggle the safety override
unsigned int heldTime = 0;
bool held = false;
// Current PWM output voltage
float voltage;

void setup()
{
  Serial.begin(9600);
  Serial.println("* Setup...");

  lcd.init();
  lcd.backlight();

  // Read previous setting from EEPROM
  output = EEPROM.read(eepromAddr);
  eepromWritten = output;

  // Initialize our encoder
  encoder = new ClickEncoder(PIN_ENC_A, PIN_ENC_B, PIN_ENC_BTN, 4); // A, B, button, pulses per turn
  encoder->setAccelerationEnabled(true);
  // NOTE: Timer makes pins 9 and 10 unusable!
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Initialize the Water Sensor pin
  pinMode(PIN_WATER_SENSOR, INPUT_PULLUP);

  // Tell the loop to redraw the display
  redraw = true;
}

void loop()
{
  ClickEncoder::Button button = encoder->getButton();
  // If the encoder button was held, enable the safety override
  // Can be used to override the water sensor check
  if(button == ClickEncoder::Held && heldTime < millis()) {
      safetyOverride = !safetyOverride;
      redraw = true;
      heldTime = millis() + 2000;
   }
  // Pin A1: Water Sensor (basically a switch), connected to pull-up resistor
  if(analogRead(PIN_WATER_SENSOR) > 500 && !safetyOverride) {
    // Safety check failed? No output!
    setOutputPin(0);

    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("! ERROR ! LASER OFF");
    lcd.setCursor(0, 2);
    lcd.print("CHECK WATER COOLING");
    lcd.setCursor(0, 0);
    lcd.print("                   ");
    lcd.setCursor(0, 3);
    lcd.print("                   ");
    
    delay(500);
    lcd.setCursor(0, 0);
    lcd.print("####################");
    lcd.setCursor(0, 3);
    lcd.print("####################");
    delay(500);
    redraw = true;

    // Ignore all key inputs
    encoder->getValue();
    encoder->getButton();
    
    // Exit, do not do anything else until the cooling is fixed
    return;
  }
  
  int16_t encoder_val = encoder->getValue();
  if(encoder_val != 0) {
    output += encoder_val;
    redraw = true;
  }
  
  if(redrawTime != 0 && millis() >= redrawTime) {
    redraw = true;
    redrawTime = 0;
  }
  if(redraw) { // only redraw if something changed
      output = (output > 1000) ? 0 : min(output, 100);
      setOutputPin(output);      
      
      lcd.setCursor(0, 0);
      lcd.print("Laser Output:      %");
      lcd.setCursor(15, 0);
      printNumber(output, 3);
      
      lcd.setCursor(0, 1);
      lcd.print("  PWM Target:      V");
      lcd.setCursor(14, 1);
      lcd.print(voltage, 2);
      
      if(safetyOverride) {
        lcd.setCursor(0, 2);
        lcd.print("! WARNING: SAFETY ! ");
        lcd.setCursor(0, 3);
        lcd.print("! OVERRIDE ACTIVE ! ");
      } else {
        lcd.setCursor(0, 2);
        lcd.print("                   ");
        lcd.setCursor(0, 3);
        lcd.print("                   ");
      }
      
      if(redraw) {
        redraw = false;
      }
  }
  // If the encoder button was clicked, save the percentage to EEPROM,
  // so it will be the default next time the machine is powered on
  if(button == ClickEncoder::Clicked) {
    if(eepromWritten != output) {
      // Only write EEPROM if the value changed, to conserve write cycles
      EEPROM.write(eepromAddr, (byte)(output & 0xFF));
      eepromWritten = output;
    }
    lcd.setCursor(0, 2);
    lcd.print("Saved to EEPROM     ");
    redrawTime = millis() + 1000;
   }
}

// Sets the output PWM voltage based on the given percentage
void setOutputPin(unsigned int percent) {
  voltage = percent == 0 ? 0.0 : // Power set to 0 ? Output 0 volts
                                 (((output - 1) / 99.0) * 4.5 + 0.5); // Otherwise, output 0.5 - 5 V
  analogWrite(PIN_PWM, (voltage / 5.0) * 255);
}

// calculate the number of digits in an integer
byte digits(unsigned int iNum) {
  byte bDigits=0;
  do {
    ++bDigits;
    iNum/=10;
  } while(iNum);
  return bDigits;
}

// print fixed-width uint number
void printNumber(unsigned int number, byte numd) {
  byte d=digits(number);
  for(byte i=d; i<numd; ++i) lcd.print((char)' '); // padding
  lcd.print(number);
}