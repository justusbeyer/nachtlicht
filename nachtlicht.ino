#include "LowPower.h"

// Pins
const int PIN_LED = 3;
const int PIN_LIGHTSENSOR = A0;
const int PIN_LIGHTSENSOR_ACTIVE = 13;

// State
float currentLightLevel = 0;

/* Battery-level-dependent lighting:
 * if battery level >= HIGH: full brightness during darkness
 * if battery level below HIGH but above LOW: dimmed in darkness
 * if battery level LOW: *no* light during darkness but blinking during light.
 */
enum BATTERY_LEVEL_THRESHOLDS {
  BATTERY_HIGH = 3*1260,
  BATTERY_MEDIUM = 3*1210,
  BATTERY_LOW = 3 * 1160
};

// Low light threshold
const int LOW_LIGHT_THRESHOLD = 300;

// the setup function runs once when you press reset or power the board
void setup() {
  //Serial.begin(9600);
  
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LIGHTSENSOR, INPUT);
  pinMode(PIN_LIGHTSENSOR_ACTIVE, OUTPUT);

  // No power to the LED and the light sensor (yet)
  analogWrite(PIN_LED, 0);
  digitalWrite(PIN_LIGHTSENSOR_ACTIVE, LOW);
  
  // Fade LED up and down
  fadeTo(255, 2);
  fadeTo(0,  2);

  // Start/safety delay before using low power sleep
  delay(4000);
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

void fadeTo(int level, int durationInSecs) {
  if (currentLightLevel == level)
    return;
  
  const period_t stepDuration = SLEEP_60MS; // adjust stepCount multiplier accordingly
  int stepCount = durationInSecs * 17 /*cuz 1/4 sec per step */;
  float stepSize = (float)(level - currentLightLevel) / stepCount;

  // (n-1) steps are intermediate steps with delay
  for(int i = 0; i < stepCount - 1; i++) {
    currentLightLevel += stepSize;
    analogWrite(PIN_LED, (int)currentLightLevel);
    LowPower.powerExtStandby(stepDuration, ADC_OFF, BOD_OFF, TIMER2_ON);
  }

  // Last step
  currentLightLevel = level;
  analogWrite(PIN_LED, currentLightLevel);
}

// returns value between 0 and 1023
int measureAmbientLightLevel() {
  // Put a voltage on the light sensor
  digitalWrite(PIN_LIGHTSENSOR_ACTIVE, HIGH);
  
  // Wait 15ms with enabled ADC for the electrons to flow
  LowPower.powerSave(SLEEP_15Ms, ADC_ON, BOD_OFF, TIMER2_ON);

  int ambientLightLevel = analogRead(PIN_LIGHTSENSOR);

  digitalWrite(PIN_LIGHTSENSOR_ACTIVE, LOW);

  return ambientLightLevel;
}

void blink() {
  digitalWrite(PIN_LED, HIGH);
  LowPower.powerDown(SLEEP_15Ms, ADC_OFF, BOD_OFF);
  digitalWrite(PIN_LED, LOW);
}

// the loop function runs over and over again forever
void loop() {
  const long vin = readVcc();
  const int ambientLightLevel = measureAmbientLightLevel();

  if (vin > BATTERY_MEDIUM) {
    // Normal operation
    if (ambientLightLevel < LOW_LIGHT_THRESHOLD) {
      // Darkness
      fadeTo(
        (vin > BATTERY_HIGH) ? 255 : 64,
        30 /* seconds */);
      LowPower.powerExtStandby(SLEEP_8S, ADC_OFF, BOD_OFF, TIMER2_ON);  
    } else {
      // Daylight, we just sleep using as little power as possible:
      if (currentLightLevel > 1)
        fadeTo(0, 2);
      digitalWrite(PIN_LED, LOW);
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }
  else {
    // LOW POWER MODE: Blink during day light
    blink();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}
