#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 180
#define DATA_PIN 16
#define COLOR_ORDER GRB
#define CHIPSET  WS2811

CRGB leds[NUM_LEDS*3] = {0};
CRGB* led_head;

enum STATE {IDLE, GAME_START, POPPING_MOLES, ADD_SCORE, LOSING_HEALTH};
STATE state;

int animation_array_construction(CRGB *led_array){
	
	for (int i = 0; i < NUM_LEDS; i++){
		led_array[NUM_LEDS+i] = CRGB::Red;
	}
  return 0;
}

int idle()
{
	for(int i = 0; i < NUM_LEDS; i++){
		led_head = led_head + i;
    FastLED.show();
		delay(10);
	}
  while(true);
  return 0;
	//mooie idle animatie die checkt op activiteit
}

int gameStart()
{
	//zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle
}

int popingMoles()
{
	//hoofdstate van het spel, mollen komen omhoog en wachten om gesmackt te worden.
}

int addScore()
{
	//als de speler succesvol is in het smacken van de mole wordt deze score toegevoegd aan de score verwerkt in de ledstrips
}

int losingHealth()
{
	//Als de speler te lang wacht met het slaan van de mol verliest deze een leven. Dit wordt gevisualiseerd op de ledstrips
}

void setup() {
  Serial.begin(9600);
  Serial.println("We started");
  state = IDLE;
  animation_array_construction(leds);
  led_head = &leds[NUM_LEDS];
  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(led_head, NUM_LEDS);
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
  
  case LOSING_HEALTH:
    
    losingHealth();

    break;
  
  default:
    break;
  }
}
