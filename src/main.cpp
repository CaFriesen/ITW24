#include <Arduino.h>
#include <FastLED.h>
#include <ADS1X15.h>

#define NUM_LEDS 180
#define DATA_PIN 16
#define COLOR_ORDER GRB
#define CHIPSET  WS2811
#define COOLDOWN_TIME_MOLE 10
#define MAX_COMETS 2
#define LED_SKIPS 2
#define COMET_SIZE 20
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
CHSV HSV_leds[NUM_LEDS*3] = {CHSV(0,0,0)};
int hue_comet[NUM_LEDS*3] = {0};

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
  long time_begin = millis();
  int ADC_value = abs(Sensor.readADC_Differential_0_1());
  // Serial.println(ADC_value);
  if (ADC_value > threshold && cooldown_timer_mole < millis())
  {
    cooldown_timer_mole = millis() + COOLDOWN_TIME_MOLE;
  }

  if (cooldown_timer_mole > millis())
  {
  // Serial.println(millis()-time_begin);
    return true;
  }
  // Serial.println(millis()-time_begin);
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
    hue_comet[0] = random(256);
    first_time = false;
  }  
  else
  {
    if (!readIfMoleHit(SENSOR_THRESHOLD))
    {
      first_time = true;
    }   
  }
}

void updateLedstrip()
{
  CRGB* led_head = &leds[0];

  switch (state)
  {
  case BASIC_INTERACTION:
  {
    uint32_t virtual_beam_size = NUM_LEDS + COMET_SIZE;

    for (int32_t i = virtual_beam_size; i >= 0; i--)
    {
      if (hue_comet[i] != 0)
      {
        int current_hue_comet = hue_comet[i];
        hue_comet[i+LED_SKIPS] = current_hue_comet;
        hue_comet[i] = 0;

        for (int j = i; (j > i - COMET_SIZE) && (j >= 0); j--)
        {
          leds[j] = CHSV(current_hue_comet, min(255 - (int)random(50),255), max(200 - (int)random(40), 0));
        }

        if(i - COMET_SIZE >= 0)
        {
          for (int j = 0; j < LED_SKIPS; j++)
          {
            leds[i-COMET_SIZE-j] = CRGB::Black;
          }
          
        }
        
        
      }
    }
    
  }
  break;

  default:
    // normal whac a mole operation
    break;
  }
  led_controller->setLeds(led_head, NUM_LEDS);
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  Serial.println("We started");
  state = BASIC_INTERACTION;
  FastLED.setBrightness(5);
  setCpuFrequencyMhz(240);
  animation_array_construction(leds);
  led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  if(!Sensor.begin()){
    Serial.println("Ja dat werkt niet; Sensor Fault can't connect");
  }
  Sensor.setDataRate(7);

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
