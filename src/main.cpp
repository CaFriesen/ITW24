#include <Arduino.h>

enum STATE {IDLE, GAME_START, POPPING_MOLES, ADD_SCORE, LOSING_HEALTH};
STATE state;

void setup() {
  state = IDLE;
}

void loop() {
  switch (STATE)
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

int idle()
{
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
