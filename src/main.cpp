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
#define NUM_LEDS NUM_LEDS_STRIP_3M * 2 + NUM_LEDS_STRIP_5M * 2
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

long long timer_comet;

// Sensor config
#define NUM_SENSORS 5
#define ANALOG_SENSOR_INPUT_PIN 27
#define THRESHOLD 1000

uint8_t sensors[NUM_SENSORS];   // Sensors
int sensor_values[NUM_SENSORS]; // Values
int sensor_adresses[NUM_SENSORS];
movingAvg piezoSensor(10);

// Timer to stabilize sensor input after its been hit
unsigned long hit_timer;

// Helemaal crazy
CRGB leds[NUM_LEDS * 3] = {0};
CHSV HSV_leds[NUM_LEDS * 3] = {CHSV(0, 0, 0)};
int hue_comet[NUM_LEDS * 3] = {0};
int value_comet[COMET_SIZE] = {0};
CRGB *led_head = &leds[0];

uint32_t virtual_beam_size = NUM_LEDS + COMET_SIZE + 1;

int rawValue = 0;

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

    FastLED.setBrightness(255);
    FastLED.addLeds<NEOPIXEL, LED_RING_DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_2>(leds, led_strip_offset[1], NUM_LEDS_STRIP_3M);

    // tell FastLED there's 60 NEOPIXEL leds on pin 3, starting at index 60 in the led array
    FastLED.addLeds<NEOPIXEL, DATA_PIN_3>(leds, led_strip_offset[2], NUM_LEDS_STRIP_5M);
    // tell FastLED there's 60 NEOPIXEL leds on pin 3, starting at index 60 in the led array
    FastLED.addLeds<NEOPIXEL, DATA_PIN_4>(leds, led_strip_offset[3], NUM_LEDS_STRIP_5M);
    timer_comet = millis();
}

void loop()
{
    // // Sensor lezen
    rawValue = analogRead(ANALOG_SENSOR_INPUT_PIN);
    Serial.println(rawValue);

    // if (timer_comet + 1000 + random(5000) < millis()) // 500 is de threshold
    // {
    //     timer_comet = millis();
    //     hue_comet[0] = random(256);
    // }

    if (rawValue > THRESHOLD)
    { // Choose custom threshold per ESP!!!!
        hue_comet[0] = random(256);
    }

    comet_ledstrip();
    FastLED.show();
}
