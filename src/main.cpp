#include <Arduino.h>
#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <movingAvg.h>

#define SWITCH_PIN 5 // TODO: Choose switch pin

// Game variables
#define MAX_COMETS 2 // ?
#define LED_SKIPS 3  // ?
#define COMET_SIZE 20
#define SENSOR_THRESHOLD 400

// LED beam config
#define DATA_PIN 33
#define DATA_PIN_2 32
#define DATA_PIN_3 25
#define DATA_PIN_4 23

#define NUM_LEDS_STRIP_3M 180
#define NUM_LEDS_STRIP_5M 300
#define NUM_LEDS NUM_LEDS_STRIP_3M*2 + NUM_LEDS_STRIP_5M*2
#define NUM_STRIPS 4
int led_strip_length[] = {NUM_LEDS_STRIP_3M, NUM_LEDS_STRIP_3M, NUM_LEDS_STRIP_5M, NUM_LEDS_STRIP_5M};
int led_strip_offset[] = {0, NUM_LEDS_STRIP_3M, NUM_LEDS_STRIP_3M * 2, NUM_LEDS_STRIP_3M * 2 + NUM_LEDS_STRIP_5M};
uint32_t led_strip_data_pins[] = {DATA_PIN, DATA_PIN_2, DATA_PIN_3, DATA_PIN_4};
CLEDController *led_strip_controllers[NUM_STRIPS];

#define COLOR_ORDER GRB
#define CHIPSET WS2812B

// LED ring config
#define LED_RING_DATA_PIN 26
#define NUM_LED_RING_PIXELS 60
CLEDController *led_ring_controller;
CRGB led_ring_array[3 * NUM_LED_RING_PIXELS];
CRGB *led_ring_array_head;

// Neopixel variables for led ring animations
#define BRIGHTNESS_RING 255
Adafruit_NeoPixel ring(NUM_LED_RING_PIXELS, LED_RING_DATA_PIN, NEO_GRB + NEO_KHZ800);
unsigned long led_ring_timer = 0;
int shutoff_delay = 150;
int ledPos = 0;
int red = 255;
int green = 255;
int blue = 0;
int rounds = 0;
int led_ring_on = 0;

long long timer_comet;

// Sensor config
#define NUM_SENSORS 5
#define ANALOG_SENSOR_INPUT_PIN 27
uint8_t sensors[NUM_SENSORS];   // Sensors
int sensor_values[NUM_SENSORS]; // Values
int sensor_adresses[NUM_SENSORS];
movingAvg piezoSensor(10);

// Simon says config
#define MAX_PATTERN_LENGTH 200
uint8_t simon_says[MAX_PATTERN_LENGTH];
uint8_t simon_increment = 0;
uint8_t simon_pattern_length = 0;
int mole_is_hit; // storing if the mole has been hit

// Timer to reset to idle
unsigned long reset_timer = 0;
unsigned long reset_delay = 30000; // 30 sec

// Timer to stabilize sensor input after its been hit
unsigned long hit_timer;

// Timer to show simon says pattern
unsigned long show_timer = 0;
unsigned long show_delay = 3000; // 3 sec

unsigned long idle_timer = 0;
unsigned long idle_delay = 60000;

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
uint32_t virtual_beam_size = NUM_LEDS + COMET_SIZE +1;

// SLAVE variables
int hit_detected;
char message;
int message_read;

// Mole popping
#define PARTITIONS 7
#define NUM_LEDS_SEGMENT (NUM_LEDS / PARTITIONS)

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
    WAITING
};
STATE state;

int rawValue = 0;

//--------------------
// Led ring functions
//--------------------
// Helper function for idle led ring
void fadeToBlack(int ledNum, byte fadeValue)
{
    uint32_t oldColor;
    uint8_t r, g, b;

    oldColor = ring.getPixelColor(ledNum);

    r = (oldColor & 0x00ff0000UL) >> 16;
    g = (oldColor & 0x0000ff00UL) >> 8;
    b = (oldColor & 0x000000ffUL);

    r = (r <= 10) ? 0 : (int)r - (r * fadeValue / 256);
    g = (g <= 10) ? 0 : (int)g - (g * fadeValue / 256);
    b = (b <= 10) ? 0 : (int)b - (b * fadeValue / 256);

    ring.setPixelColor(ledNum, r, g, b);
}

// Idle animation (behoorlijk lelijke code wel)
void idleRingAnimation(byte meteorSize, byte meteorTrail, boolean randomTrail, int speedDelay, int r, int g, int b)
{
    // meteorSize in amount of LEDs
    // meteorTrail determines how fast the trail dissappears (higher is faster)
    // randomTrail enables or disables random trail sharts
    // speedDelay is the delay (sync to BPM)
    // r g b are the color values

    // Guard for timing
    if (speedDelay > millis() - led_ring_timer)
    {
        return;
    }

    led_ring_timer = millis(); // time is rewritten to current millis starting a new interval

    // Position is updated
    if (ledPos < NUM_LED_RING_PIXELS)
    {
        ledPos++;
    }
    else
    {
        ledPos = 0;
        red = 0;
        green = 0;
        blue = 0;
        rounds++;
    }

    if (rounds == 2)
    {
        blue = 255;
        rounds = 0;
        hue_comet[0] = random(256);
    }

    // Fade LEDs
    for (int j = 0; j <= NUM_LED_RING_PIXELS; j++)
    {
        if ((!randomTrail) || (random(10) > 5))
        {
            fadeToBlack(j, meteorTrail);
        }
    }
    // Draw meteor
    for (int j = 0; j < meteorSize; j++)
    {
        if (ledPos >= j)
        {
            ring.setPixelColor(ledPos - j, r, g, b);
        }
    }
    // Show LEDs
    ring.show();
}

// animation if correct mole is hit
void led_ring_animation_hit(int r, int g, int b)
{
    // For every LED
    for (int i = 0; i < NUM_LED_RING_PIXELS; i++)
    {
        ring.setPixelColor(i, 0, random(0, 255), random(0, 255));
    }

    // Show LEDs
    ring.show();

    // Reset dim timer
    led_ring_timer = millis();
}

// Turn led ring off
// This function should always run to turn led ring off automatically when timer has not been reset
void dimLedring()
{
    // Timer for led shut-off
    if (led_ring_timer + shutoff_delay < millis())
    {
        // Reset timer
        led_ring_timer = millis();

        // For every LED
        for (int i = 0; i < NUM_LED_RING_PIXELS; i++)
        {
            ring.setPixelColor(i, 0, 0, 0);
        }

        // Show LEDs
        ring.show();
    }
}

//---------------------
// Led strip functions
//---------------------

// void led_strip_setLeds()
// {
//     for (int i = 0; i < NUM_STRIPS; i++)
//     {
//         led_strip_controllers[i]->setLeds(led_head + led_strip_offset[i], led_strip_length[i]);
//     }
// }

void updateLedstrip()
{
    // led_strip_setLeds();
    // FastLED.show();
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


void setup()
{
    Serial.begin(115200);
    Serial.println("SYSTEM BOOTED");

    // // setup moving average
    // piezoSensor.begin();
    // for (int i = 0; i < 2000; i++)
    // {
    //     piezoSensor.reading(analogRead(ANALOG_SENSOR_INPUT_PIN));
    // }
    
    FastLED.setBrightness(255);
    FastLED.addLeds<NEOPIXEL, LED_RING_DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_2>(leds, led_strip_offset[1], NUM_LEDS_STRIP_3M);

    
    // tell FastLED there's 60 NEOPIXEL leds on pin 3, starting at index 60 in the led array
    FastLED.addLeds<NEOPIXEL, DATA_PIN_3>(leds, led_strip_offset[2], NUM_LEDS_STRIP_5M);
    // tell FastLED there's 60 NEOPIXEL leds on pin 3, starting at index 60 in the led array
    FastLED.addLeds<NEOPIXEL, DATA_PIN_4>(leds, led_strip_offset[3], NUM_LEDS_STRIP_5M);
    timer_comet = millis();    
    ring.begin(); // INITIALIZE NeoPixel strip object
    // ring.show();  // Turn OFF all pixels ASAP
    // ring.setBrightness(BRIGHTNESS_RING);
 
}

    // void setup() {


    // // // led_ring_controller = &FastLED.addLeds<CHIPSET, LED_RING_DATA_PIN, COLOR_ORDER>(leds, 0, NUM_LED_RING_PIXELS).setCorrection(TypicalLEDStrip);

    // // hit_timer = 0;
    // // state = IDLE;
    // }

void loop()
{
    // // Sensor lezen
    rawValue = analogRead(27);
    Serial.println(rawValue);

    // if (timer_comet + 1000 + random(5000) < millis()) // 500 is de threshold
    // {
    //     timer_comet = millis();
    //     hue_comet[0] = random(256);
    // }

    if (rawValue > 500) { // 500 threshold
        hue_comet[0] = random(256);
    }
    
    comet_ledstrip();
    FastLED.show();
}
