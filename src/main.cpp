#include <Arduino.h>
#include <FastLED.h>
#include <ADS1X15.h>

#define NUM_LEDS 180
#define DATA_PIN 16
#define COLOR_ORDER GRB
#define CHIPSET  WS2811

#define SDA_1 21
#define SCL_1 22

ADS1115 Sensor(0x48);

int sensor_value;
int moleActive;
int notPopped;
long long popUpTimeStamp;
long long popTime;

CRGB leds[NUM_LEDS*3] = {0};

CLEDController* led_controller; 

enum STATE {IDLE, GAME_START, POPPING_MOLES, ADD_SCORE};
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
  
  CRGB* led_head = &leds[NUM_LEDS];;
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

int idle2()
{
  CRGB* led_head;
  if (sensor_value > 100)
  {
    led_head = &leds[NUM_LEDS];
  } 
  else
  {
    led_head = &leds[0];
  }
  
  led_controller->setLeds(led_head, NUM_LEDS);
  FastLED.show();
  delay(100);

  return 0;
}

int readIfMoleHit(){
  sensor_value = abs(Sensor.readADC(0));
  return sensor_value;
}

int gameStart()
{
	//zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle
}

int popingMoles()
{
  if(readIfMoleHit() && notPopped){
    moleActive = false;
    notPopped = false;
    popTime = millis() - popUpTimeStamp;
  }

  

	//hoofdstate van het spel, mollen komen omhoog en wachten om gesmackt te worden.
}

int addScore()
{
	//als de speler succesvol is in het smacken van de mole wordt deze score toegevoegd aan de score verwerkt in de ledstrips
}

void setup() {
  Serial.begin(9600);
  Serial.println("We started");
  state = IDLE;
  FastLED.setBrightness(100);
  animation_array_construction(leds);
  led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  if(!Sensor.begin()){
    Serial.println("Ja dat werkt niet; Sensor Fault can't connect");
  }

  popTime = 0;
}

void loop() {

  switch (state)
  {
    case IDLE:
      idle2();
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
    
    default:
      break;
    }
}
