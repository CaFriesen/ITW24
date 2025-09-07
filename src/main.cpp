#include <Arduino.h>
#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// LED beam config
#define DATA_PIN 33
#define DATA_PIN_2 32
#define DATA_PIN_3 25
#define DATA_PIN_4 23

#define NUM_LEDS_STRIP_3M 180
#define NUM_LEDS_STRIP_5M 300
#define NUM_LEDS NUM_LEDS_STRIP_3M * 2 + NUM_LEDS_STRIP_5M * 2 // Incorrect with LED strips defined!!!
#define NUM_STRIPS 4
int led_strip_offset[] = {0, NUM_LEDS_STRIP_3M, NUM_LEDS_STRIP_3M + NUM_LEDS_STRIP_5M, NUM_LEDS_STRIP_3M + NUM_LEDS_STRIP_5M * 2};
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
unsigned long persist_stop = 20000;

// Sensor config
#define NUM_DRUMS 5
#define ANALOG_SENSOR_INPUT_PIN 27
#define THRESHOLD 1400

// Helemaal crazy
#define LED_SKIPS 3
#define COMET_SIZE 20
CRGB leds[NUM_LEDS * 3] = {0};
CHSV HSV_leds[NUM_LEDS * 3] = {CHSV(0, 0, 0)};
int hue_comet[NUM_LEDS * 3] = {0};
int value_comet[COMET_SIZE] = {0};
CRGB *led_head = &leds[0];
uint32_t virtual_beam_size = NUM_LEDS + COMET_SIZE + 1;

// Starting LED of comet (remains on for time interval)
bool persistent_led[NUM_LEDS] = {false};
int persistent_start[NUM_LEDS] = {0};

bool implode_active = false;
int implode_index = 0;
uint32_t implode_start = 0;

int double_drum_hit = 0;
unsigned long double_timer = 0;
unsigned long double_interval = 100; // Window to register double drum hit

// RS458 config
#define RS485_TX_PIN 17 // connect to MAX485 DI
#define RS485_RX_PIN 16 // connect to MAX485 RO
#define RS485_DE_RE 4   // connect to MAX485 DE+RE (tied together)

HardwareSerial RS485Serial(1); // use UART1

// Enable transmit mode
void rs485Transmit()
{
    digitalWrite(RS485_DE_RE, HIGH);
}

// Enable receive mode
void rs485Receive()
{
    digitalWrite(RS485_DE_RE, LOW);
}

// Send an integer
void rs485SendInt(int value)
{
    rs485Transmit();
    RS485Serial.println(value); // send as text + newline
    RS485Serial.flush();
    rs485Receive();
}

// Read an integer
int rs485ReadInt()
{
    if (RS485Serial.available())
    {
        return RS485Serial.parseInt(); // waits until it gets a number (non-blocking if no digits available)
    }
    return -1; // no data
}

void startImplode(int i)
{
    implode_active = true;
    implode_index = i;
    implode_start = millis();
}

void updateImplode()
{
    if (!implode_active)
        return;

    uint32_t now = millis();
    uint32_t elapsed = now - implode_start;
    int flash_duration = 100;
    int flicker_duration = 400;

    if (elapsed < flash_duration)
    {
        leds[implode_index] = CHSV(255, 255, 255); // bright flash
    }
    else if (elapsed < flash_duration + flicker_duration)
    {
        uint8_t progress = map(elapsed - flash_duration, 0, flicker_duration, 255, 0);
        uint8_t flicker = random8(20);
        leds[implode_index] = CHSV(255, 200, max<int>(0, progress - flicker));
    }
    else
    {
        leds[implode_index] = CRGB::Black; // off
        implode_active = false;
    }
}

void doubleHitAnimation()
{
    if (double_timer + millis() > double_interval) // If double interval exceeded
    {
        double_drum_hit = 0; // Reset double hit counter
        return;
    }

    if (double_drum_hit < 2) // If no double hit registered
        return;

    // Send 3 animation triggers to all drums over rs458
    for (int i = 0; i < 2; i++)
    {
        rs485SendInt(1);
        delay(10);
    }
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

            // TODO: Comet down the ledstrip from start led (not only up)

            if (i - COMET_SIZE >= 0)
            {
                for (int j = 0; j < LED_SKIPS; j++)
                {
                    if (!persistent_led[j]) // TODO: Check if correct led is now targeted
                    {
                        leds[i - COMET_SIZE - j] = CRGB::Black;
                    }
                }
            }
        }
    }
}

int randomStartLed()
{
    int start = random(0, NUM_LEDS);
    int attempts = 0;
    while (persistent_led[start] && attempts < NUM_LEDS) // If led is already on
    {
        start = random(0, NUM_LEDS); // Continue searching
        attempts++;
    }
    persistent_led[start] = true;       // Set persistent status
    persistent_start[start] = millis(); // Set timestamp
    return start;
}

void updatePersistentLeds()
{
    uint32_t now = millis();
    for (int i = 0; i < NUM_LEDS; i++) // For all leds
    {
        if (persistent_led[i]) // If persistent
        {
            // Keep LED on
            leds[i] = CHSV(hue_comet[i] == 0 ? 100 : hue_comet[i], 255, 255);

            // Timeout check
            if (now - persistent_start[i] > persist_stop) // If on for interval time
            {
                persistent_led[i] = false; // Remove persistent status
                startImplode(i);           // Implode LED
            }
        }
    }
}

void triggerLedstripAnimation()
{
    int value = analogRead(ANALOG_SENSOR_INPUT_PIN);
    Serial.println(value);

    if (value > THRESHOLD) // Choose custom threshold per ESP!!!!
    {
        hue_comet[randomStartLed()] = random(256); // Trigger comet
        timer_comet = millis();                    // Reset idle timer
        double_drum_hit++;
        double_timer = millis(); // Start double interval
    }

    if (rs485ReadInt() == 1) // Trigger from double hit
    {
        hue_comet[randomStartLed()] = random(256); // Trigger comet
        timer_comet = millis();                    // Reset idle timer
        double_drum_hit = 0;
    }
}

void randomTrigger()
{
    int min_interval = 60000;  // 1 minute
    int max_interval = 600000; // 10 minutes
    int interval = 0;          // Start interval = 0

    if (timer_comet + interval < millis())
    {
        interval = random(min_interval, max_interval); // Calculate new random interval in range
        hue_comet[randomStartLed()] = random(256);     // Trigger comet
        timer_comet = millis();                        // Reset timer
    }
}

void setup()
{
    pinMode(RS485_DE_RE, OUTPUT);
    rs485Receive(); // default to listen mode

    RS485Serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    Serial.begin(115200);
    Serial.println("SYSTEM BOOTED");

    FastLED.setBrightness(255);
    // Ring is part of LED strip
    FastLED.addLeds<NEOPIXEL, LED_RING_DATA_PIN>(leds, 0, NUM_LED_RING_PIXELS);

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_2>(leds, led_strip_offset[1], NUM_LEDS_STRIP_5M);

    FastLED.addLeds<NEOPIXEL, DATA_PIN_3>(leds, led_strip_offset[2], NUM_LEDS_STRIP_5M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_4>(leds, led_strip_offset[3], NUM_LEDS_STRIP_5M);
    timer_comet = millis();
}

void loop()
{
    triggerLedstripAnimation();
    randomTrigger();
    comet_ledstrip();
    updatePersistentLeds();
    updateImplode();
    doubleHitAnimation();
    FastLED.show();
}
