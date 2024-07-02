#include <Arduino.h>
#include <FastLED.h>
#include <ADS1X15.h>

#define NUM_LEDS 180
#define DATA_PIN 16
#define COLOR_ORDER GRB
#define CHIPSET  WS2811
#define COOLDOWN_TIME_MOLE 1000
#define MAX_COMETS 50
#define COMET_SIZE 40
#define SENSOR_THRESHOLD 400

#define SDA_1 21
#define SCL_1 22


ADS1115 Sensor(0x48);

int sensor_value;
int moleActive;
int notPopped;
long long popUpTimeStamp;
long long popTime;
long long cooldown_timer_mole;
int first_time;


CRGB leds[NUM_LEDS*3] = {0};
CHSV HSV_leds[NUM_LEDS*3];

CLEDController* led_controller; 

enum STATE {IDLE, GAME_START, POPPING_MOLES, ADD_SCORE, BASIC_INTERACTION};
STATE state;

int animation_array_construction(CRGB *led_array){
	
	for (int i = 0; i < NUM_LEDS; i++){
		led_array[NUM_LEDS+i] = CRGB::Red;
	}
  return 0;
}

void resetMole(){
  moleActive = true;
  notPopped = true;
  popUpTimeStamp = millis();
}

int idle()
{
  
  CRGB* led_head = &leds[NUM_LEDS];
	long long led_timer = micros();

  for(int i = 0; i < NUM_LEDS; i++){
    
    led_head += 1;
    led_controller->setLeds(led_head, NUM_LEDS);
    FastLED.show();
    Serial.println(micros() - led_timer);
    led_timer = micros();
	}
  return 0;
	//mooie idle animatie die checkt op activiteit
}

// Reads the sensor value and compares it to a threshold value. When the threshold has been surpassed and the cooldown timer is not active the a cooldown timer will be set to the defined COOLDOWN_TIME. When the cooldown timer is active the function returns true00
int readIfMoleHit(int threshold){
  int ADC_value = abs(Sensor.readADC_Differential_0_1());
  Serial.println(ADC_value);
  if (ADC_value > threshold && cooldown_timer_mole < millis())
  {
    cooldown_timer_mole = millis() + COOLDOWN_TIME_MOLE;
  }

  if (cooldown_timer_mole > millis())
  {
    return true;
  }
  return false;
}

int gameStart()
{
	//zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle
  return 0;
}

int popingMoles()
{
  if(readIfMoleHit(SENSOR_THRESHOLD) && notPopped){
    moleActive = false;
    notPopped = false;
    popTime = millis() - popUpTimeStamp;
  }
  return 0;
  

	//hoofdstate van het spel, mollen komen omhoog en wachten om gesmackt te worden.
}

int addScore()
{
	//als de speler succesvol is in het smacken van de mole wordt deze score toegevoegd aan de score verwerkt in de ledstrips
  return 0;
}

void basicInteraction()
{
  if(readIfMoleHit(SENSOR_THRESHOLD) && first_time)
  {
    HSV_leds[0] = CHSV(random(256),0,200);
    leds[0] = CRGB::White;
    first_time = false;
  }
  else
  {
    first_time = true;
  }
}

void updateLedstrip()
{
  CRGB* led_head = &leds[0];

  switch (state)
  {
  case BASIC_INTERACTION:
    for (int32_t i = 0; i < NUM_LEDS; i++)
    {
      if (leds[i] == CRGB::White)
      {
        leds[i+1] == CRGB::White;

        CHSV main_hsv = HSV_leds[i];
        HSV_leds[i+1] = main_hsv;
        for (int32_t j = i; (j > i - COMET_SIZE) && (j >= 0); j--)
        {
          leds[j] = CHSV(main_hsv.hue, min(main_hsv.saturation + (uint8_t)random(50),255), max(main_hsv.value - (uint8_t)random(40), 0));
        }
      }
    }
    
    break;
  
  default:
    // normal whac a mole operation
    break;
  }

  led_controller->setLeds(led_head, NUM_LEDS);
}

void setup() {
  Serial.begin(9600);
  Serial.println("We started");
  state = BASIC_INTERACTION;
  FastLED.setBrightness(100);
  animation_array_construction(leds);
  led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  if(!Sensor.begin()){
    Serial.println("Ja dat werkt niet; Sensor Fault can't connect");
  }

  popTime = 0;
  first_time = true;
}

void loop() {

  switch (state)
  {
    case IDLE:
      idle();
      break;
    
    case GAME_START:
      
      gameStart();

      break;
    
    case POPPING_MOLES:
      
      popingMoles();

      break;
    
    case ADD_SCORE:
      
      addScore();

      break;
    
    case BASIC_INTERACTION:

      basicInteraction();

      break;

    default:
      break;
    }
  updateLedstrip();
}
