#include <Arduino.h>
#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

#define SWITCH_PIN 5 // TODO: Choose switch pin

// Game variables
#define MAX_COMETS 2 // ?
#define LED_SKIPS 3  // ?
#define COMET_SIZE 20
#define SENSOR_THRESHOLD 400

// LED beam config
#define NUM_LEDS_STRIP_3M 180
#define NUM_LEDS_STRIP_5M 300
#define NUM_STRIPS 4
#define NUM_LEDS 960
#define NUM_BEAMS 5
uint8_t beams[NUM_BEAMS];

#define DATA_PIN 33
#define DATA_PIN_2 32
#define DATA_PIN_3 25
#define DATA_PIN_4

#define COLOR_ORDER GRB
#define CHIPSET WS2812B

// LED ring config
#define LED_RING_DATA_PIN 26
#define NUM_LED_RING_PIXELS 60
Adafruit_NeoPixel led_ring(NUM_LED_RING_PIXELS, LED_RING_DATA_PIN, NEO_GRB + NEO_KHZ800);

// Sensor config
#define NUM_SENSORS 5
#define ANALOG_SENSOR_INPUT_PIN 27
uint8_t sensors[NUM_SENSORS];   // Sensors
int sensor_values[NUM_SENSORS]; // Values
int sensor_adresses[NUM_SENSORS];

// Simon says config
#define MAX_PATTERN_LENGTH 200
uint8_t simon_says[MAX_PATTERN_LENGTH];
uint8_t simon_increment = 0;
uint8_t simon_pattern_length = 0;

// Timer to reset to idle
unsigned long reset_timer = 0;
unsigned long reset_delay = 30000; // 30 sec

// Timer to stabilize sensor input after its been hit
unsigned long hit_timer;

// Timer to show simon says pattern
unsigned long show_timer = 0;
unsigned long show_delay = 3000; // 3 sec

// Timer gameStart
#define TOTAL_GAME_START_ANIMATION_TIME_MS 3000
unsigned long game_start_timer;

// Outdated?
int moleActive;
int notPopped;
long long popUpTimeStamp;
long long cooldown_timer_mole;

// Helemaal crazy
CRGB leds[NUM_LEDS * 3] = {0};
CHSV HSV_leds[NUM_LEDS * 3] = {CHSV(0, 0, 0)};
int hue_comet[NUM_LEDS * 3] = {0};
int value_comet[COMET_SIZE] = {0};
CRGB *led_head = &leds[0];
uint32_t virtual_beam_size = NUM_LEDS + COMET_SIZE;

//SLAVE variables
int hit_detected;
enum SLAVESTATE
{
    OFF,
    START,
    STOP,
    IN_GAME
};
SLAVESTATE slave_state;

CLEDController *led_controller;

enum ROLE
{
    MASTER,
    SLAVE,
    UNDETERMINED
};
ROLE role;

enum STATE
{
    IDLE,
    ACTIVE,
    GAME_START,
    GAME_OVER,
    PATTERN_SHOW,
    BASIC_INTERACTION,
};
STATE state;

//[General function]
//Determines I2C adress (master/slave) of the device
//Input: none
//Output: returns its adress
int determineI2CAdress()
{
  if(digitalRead(15))
  {
    return 0;
  }
  if(digitalRead(5))
  {
    return 5;
  }
  if(digitalRead(18))
  {
    return 6;
  }
  if(digitalRead(19))
  {
    return 7;
  }
  if(digitalRead(2))
  {
    return 8;
  }
  return -1;
}

int update_slave_state(STATE s)
{
    for(int i = 0; i < 4; i++)
    {
        Wire.beginTransmission(i+5);
        Wire.write(s); //send state variable to slaves
        byte error = Wire.endTransmission();
        if (error)
        {
            error_handler_I2C_bus(error);
        }
    }
    
    return 0;
}

void set_total_ring_color(int r, int g, int b)
{
    for (int i = 0; i < NUM_LED_RING_PIXELS; i++)
    {
        led_ring.setPixelColor(i, led_ring.Color(r,g,b));
    }
}

//[General function]
//Determines role (master/slave) of the device and provides the option to determine ring color
//Input: none
//Output: changes role variable (master/slave), returns 0 if behavior was as expected
int determineRole()
{
  if(digitalRead(15))
  {
    role = MASTER;
    set_total_ring_color(0,255,0);
    return 0;
  }
  if(digitalRead(5))
  {
    role = SLAVE;
    set_total_ring_color(0,255,0);
    return 0;
  }
  if(digitalRead(18))
  {
    role = SLAVE;
    set_total_ring_color(0,255,0);
    return 0;
  }
  if(digitalRead(19))
  {
    role = SLAVE;
    set_total_ring_color(0,255,0);
    return 0;
  }
  if(digitalRead(2))
  {
    role = SLAVE;
    set_total_ring_color(0,255,0);
    return 0;
  }
  return -1;
}

// Helper function for LED animation
int value_array_construction()
{
    for (int i = 0; i < COMET_SIZE; i++)
    {
        value_comet[i] = COMET_SIZE - map(i, COMET_SIZE, 0, 255, 0);
    }
}

// [DEMO function, legacy]
// Constructs a red led section as big as the physical led strips itself
int animation_array_construction(CRGB *led_array, int r, int g, int b)
{

    for (int i = 0; i < NUM_LEDS; i++)
    {
        led_array[NUM_LEDS + i] = CRGB(r,g,b);
    }
    return 0;
}

int idleAnimation()
{
    // Wat doet dit allemaal? Is dit nodig voor idle?
    CRGB *led_head = &leds[NUM_LEDS];
    long long led_timer = micros();

    for (int i = 0; i < NUM_LEDS; i++)
    {

        led_head += 1;
        led_controller->setLeds(led_head, NUM_LEDS);
        FastLED.show();
        Serial.println(micros() - led_timer);
        led_timer = micros();
    }
    return 0;
    // mooie idle animatie die checkt op activiteit
}

void message_slave(int slave_address)
{
    Wire.beginTransmission(slave_address);
    Wire.write('s'); //s = show
    byte error = Wire.endTransmission();
    if (error)
    {
        error_handler_I2C_bus(error);
    }
    
}

void message_slave(int slave_address, uint8_t message)
{
    Wire.beginTransmission(slave_address);
    Wire.write(message);
    byte error = Wire.endTransmission();
    if (error)
    {
        error_handler_I2C_bus(error);
    }
    
}

void message_slave(int slave_address, char message)
{
    Wire.beginTransmission(slave_address);
    Wire.write((uint8_t)message);
    byte error = Wire.endTransmission();
    if (error)
    {
        error_handler_I2C_bus(error);
    }
    
}

void showSimonSays()
{
    if (show_delay < millis() - show_timer)
    {
        // Reset timer
        show_timer = millis();
        // TODO: Communicate with other systems to turn LED ring (& beam?) on in the correct system

        // Increment counter
        Serial.printf("%d ", simon_says[simon_increment]);
        message_slave(sensor_adresses[simon_says[simon_increment]]);

        simon_increment++;
        // If entire pattern is shown
        if (simon_increment >= simon_pattern_length)
        {
            Serial.printf(", length: %d\n", simon_pattern_length);
            // Reset counter
            simon_increment = 0;
            // Become active
            state = ACTIVE;
            update_slave_state(ACTIVE);
        }
    }
}

void updateSimonSays()
{
    // New random value for the array
    simon_says[simon_pattern_length] = random(0, NUM_SENSORS);
    // Update length
    simon_pattern_length++;
}

void resetSimonSays()
{
    // Reset values
    for (int i = 0; i < MAX_PATTERN_LENGTH; i++)
    {
        simon_says[i] = 0;
    }
    Serial.println("Pattern reset");
    // Reset length
    simon_pattern_length = 0;
}

int is_mole_hit()
{
    if (analogRead(ANALOG_SENSOR_INPUT_PIN) > 1000 && millis() - hit_timer < 300)
    {
        hit_timer = millis();
        return 0;
    }
    return -1;    
}

//[Master function]
// Requests input data from slaves
// Input: none
// Output: returns the drum that first has been hit value 0 to 4 including 4
int getInputData()
{
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        if (i == 0)
        {
            if (is_mole_hit())
            {
                return 0; // 0 is the number assigned to this sensor
            }
        }
        else
        {
            Wire.requestFrom(i+4,1);
            while (Wire.available())
            {
                if (Wire.read() == 1)
                {
                    return i;
                }
            }
        }
        return -1;
    }
}

void led_ring_animation_wrong_mole()
{
    set_total_ring_color(255,0,0);
    for (int i = 0; i < 3; i++)
    {
        led_ring.setBrightness(255);
        delay(500);                
        led_ring.setBrightness(0);
        delay(500);                
    }
}

// Compare actual drum hit to expected drum hit for simon says mechanics
void checkCorrectHit()
{
    // Get data
    int current_value = getInputData();

    // If correct mole is hit
    if (current_value == simon_says[simon_increment])
    {
        Serial.printf("Sensor %d hit, correct\n", current_value);
        // TODO: LED ring animation
        if (sensor_adresses[current_value] == 0) //simpele plaats vervanger
        {
                led_ring.setBrightness(255);
                delay(500);
        }

        message_slave(sensor_adresses[current_value]);
        // LED beam animation
        hue_comet[0] = random(256);

        // Increment through simon says array
        simon_increment++;
        // If end is reached (current pattern is replicated)
        if (simon_increment >= simon_pattern_length)
        {
            Serial.printf("Complete, updated pattern: ");
            // Update simon says pattern
            updateSimonSays();
            // Reset counter
            simon_increment = 0;
            // Show new pattern
            state = PATTERN_SHOW;
            update_slave_state(PATTERN_SHOW);
        }
    }
    else if (current_value != -1)
    {
        Serial.printf("Sensor %d hit, incorrect\n", current_value);
        if (sensor_adresses[current_value] == 0)
        {
            led_ring_animation_wrong_mole();
        }
        else
        {
            message_slave(sensor_adresses[current_value],'w');
        }

        // Reset simon says
        resetSimonSays();
        // Reset counter
        simon_increment = 0;
        // End game
        state = GAME_OVER;
        update_slave_state(GAME_OVER);
    }
    else
    {
    }
}

boolean timeIsUp()
{
    if (reset_delay < millis() - reset_timer)
    {
        reset_timer = millis();
        return true;
    }
    return false;
}

void error_handler_I2C_bus(byte I2C_error)
{
    switch (I2C_error)
    {
      case (1):
        Serial.println("I2C bus error: data too long to fit in transmit buffer.");
        Serial.println("Check I2C bus on pins 21 and 22");
        break;

      case (2):
        Serial.println("I2C bus error: received NACK on transmit of address.");
        Serial.println("Check I2C bus on pins 21 and 22");
        break;

      case (3):
        Serial.println("I2C bus error: received NACK on transmit of data.");
        Serial.println("Check I2C bus on pins 21 and 22");
        break;

      case (4):
        Serial.println("I2C bus error: unknown error.");
        Serial.println("Check I2C bus on pins 21 and 22");
        break;

      case (5):
        Serial.println("I2C bus error: timeout.");
        Serial.println("Check I2C bus on pins 21 and 22");
        break;

      default:
        break;
    }
}

int gameStart()
{
    animation_array_construction(&leds[NUM_LEDS],0,255,0);
    
    
    int index = (2*NUM_LEDS/TOTAL_GAME_START_ANIMATION_TIME_MS)*(millis()-game_start_timer);//weet niet of dit werkt, berekent eerst een factor uit daarna vermenigvuldigd deze met de verstreken tijd. Zo zou de animatie even lang als de "total game start animation time" moeten duren
    
    if (index < NUM_LEDS*2) //update until the leds are done
    {
        led_head = &led_head[index];
    }
    
    if (index >= NUM_LEDS*2)
    {
        delay(500);
        // Update pattern
        updateSimonSays();
        // Start game
        Serial.print("Pattern: ");
        state = PATTERN_SHOW;
        update_slave_state(PATTERN_SHOW);
    }
    

    // zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle
    
    return 0;
}

void comet_ledstrip()
{


    for (int32_t i = virtual_beam_size; i >= 0; i--)
    {
        if (hue_comet[i] != 0)
        {
            int current_hue_comet = hue_comet[i];
            hue_comet[i + LED_SKIPS] = current_hue_comet;
            hue_comet[i] = 0;

            for (int j = i; (j > i - COMET_SIZE) && (j >= 0); j--)
            {
                leds[j] = CHSV(current_hue_comet, min(COMET_SIZE - ((i - j) * 12) - (int)random(50), 50), max(200 - (int)random(40), 0));
            }

            if (i - COMET_SIZE >= 0)
            {
                for (int j = 0; j < LED_SKIPS; j++)
                {
                    leds[i - COMET_SIZE - j] = CRGB::Black;
                }
            }
        }
    }
}

void updateLedstrip()
{
    led_controller->setLeds(led_head, NUM_LEDS);
    FastLED.show();
}

void updateLedring()
{
    int mapped_analog_value = map(analogRead(ANALOG_SENSOR_INPUT_PIN),0,4095,0,255);
    led_ring.setBrightness(mapped_analog_value);
    led_ring.show();
}

//[Slave function]
// Sends the value of hit_detected variable to let the master know if a hit has been detected. After sending the value the hit_detected variable is reset to 0
// Input: none
// Output: none
void sendIfHitDetected()
{
    Serial.println(hit_detected);
    Wire.write(hit_detected);
    hit_detected = 0;
}

//[Slave function]
// 
// Input: none
// Output: none
void receiveState(int len)
{
    int state;
    while (Wire.available())
    {
        state = Wire.read();
        Serial.println(state);
    }
    
    switch (state)
    {
    case 0:
        state = OFF;
        break;
    case 1:
        state = START;
        game_start_timer = millis();
        break;
    case 2:
        state = STOP;
        break;
    case 3:
        state = IN_GAME;
        break;
    default:
        break;
    }
}
//stores adresses in sensor_adresses array dynamically
void store_sensor_adresses()
{
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        if (i == 0)
        {
            sensor_adresses[i] = 0;
        }
        else
        {
            sensor_adresses[i] = 4 + i ;
        }
    }
}

//{Setup functions}

void master_setup()
{
    store_sensor_adresses(); //fills the sensor_adresses array with correct adresses

    Wire.begin();

    state = IDLE;
}

void slave_setup(int I2C_adress)
{
    Wire.begin(I2C_adress);
    Wire.onReceive(receiveState);
    Wire.onRequest(sendIfHitDetected);
    
}

//{Loop functions}

// general behavior
void general_loop()
{
    updateLedstrip();
    updateLedring();
}

// Slave behavior
void slave_loop()
{
    general_loop();
}

// Master behavior
void master_loop()
{
    switch (state)
    {
        case IDLE:
            if (getInputData() != -1)
            {
                Serial.println("GAME START");
                state = GAME_START;
                update_slave_state(GAME_START); // update slaves with current state on the master
            }
            //Willen we een wacht animatie?
            break;

        case GAME_START:
            gameStart();
            break;

        case ACTIVE:
            checkCorrectHit();
            if (timeIsUp())
            {
                state = GAME_OVER;
                update_slave_state(GAME_OVER);
            }
            break;

        case PATTERN_SHOW:
            // Don't read inputs here
            showSimonSays();
            break;

        case GAME_OVER:
            // Show game over animation > go to idle
            Serial.println("GAME OVER");
            state = IDLE;
            break;

        default:
            break;
    }
    general_loop();
}

void setup()
{

    Serial.begin(115200);
    Serial.println("SYSTEM BOOTED");

    //setup led strips 
    FastLED.setBrightness(100);
    led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, 0, NUM_LEDS_STRIP_3M).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<CHIPSET, DATA_PIN_2, COLOR_ORDER>(leds, 180, NUM_LEDS_STRIP_3M).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<CHIPSET, DATA_PIN_3, COLOR_ORDER>(leds, 360, NUM_LEDS_STRIP_5M).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<CHIPSET, DATA_PIN_3, COLOR_ORDER>(leds, 660, NUM_LEDS_STRIP_5M).setCorrection(TypicalLEDStrip);

    hit_timer = 0;
    role = UNDETERMINED;

    if(determineRole())
    {
        Serial.println("Role undetermined: check wiring on pins 15, 5, 18, 19 and 21");
        for(;;);
    }

    switch (role)
    {
    case MASTER:
        master_setup();
        master_loop();
        break;

    case SLAVE:
        slave_setup(determineI2CAdress());
        slave_loop();
        break;

    default:
        Serial.println("Role undetermined: check wiring on pins 15, 5, 18, 19 and 21");
        for(;;);
        break;
    }
}

void loop()
{
    switch (role)
    {
    case MASTER:

        // Master behavior
        switch (state)
        {
        case IDLE:
            if (getInputData() != -1)
            {
                Serial.println("GAME START");
                // Update pattern
                updateSimonSays();
                // Start game
                Serial.print("Pattern: ");
                state = PATTERN_SHOW;
            }
            break;

        case GAME_START:
            // Some start animation > go to active
            break;

        case ACTIVE:
            checkCorrectHit();
            if (timeIsUp())
            {
                state = GAME_OVER;
            }
            break;

        case PATTERN_SHOW:
            // Don't read inputs here
            showSimonSays();
            break;

        case GAME_OVER:
            // Show game over animation > go to idle
            Serial.println("GAME OVER");
            state = IDLE;
            break;

        default:
            break;
        }
        break;

    case SLAVE:

        // Slave behavior
        break;
    }

    updateLedstrip();
    updateLedring();
}
