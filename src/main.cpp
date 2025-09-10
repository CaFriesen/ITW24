#include <Arduino.h>
#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// LED beam config
#define DATA_PIN 33
#define DATA_PIN_2 32
#define DATA_PIN_3 25
#define DATA_PIN_4 23

// LED ring config
#define LED_RING_DATA_PIN 26
#define NUM_LED_RING_PIXELS 60
CLEDController *led_ring_controller;
CRGB led_ring_array[3 * NUM_LED_RING_PIXELS];
CRGB *led_ring_array_head;

#define NUM_LEDS_STRIP_3M 180
#define NUM_LEDS_STRIP_5M 300
#define NUM_LEDS NUM_LEDS_STRIP_3M * 2 + NUM_LEDS_STRIP_5M * 2
#define NUM_STRIPS 4
int led_strip_offset[] = {0, NUM_LEDS_STRIP_3M, NUM_LEDS_STRIP_3M + NUM_LEDS_STRIP_5M, NUM_LEDS_STRIP_3M + NUM_LEDS_STRIP_5M * 2};
uint32_t led_strip_data_pins[] = {DATA_PIN, DATA_PIN_2, DATA_PIN_3, DATA_PIN_4};
CLEDController *led_strip_controllers[NUM_STRIPS];

#define COLOR_ORDER GRB
#define CHIPSET WS2812B

long long timer_comet;
unsigned long persist_stop = 20000;

// Sensor config
#define NUM_DRUMS 5
#define ANALOG_SENSOR_INPUT_PIN 27
#define THRESHOLD 1400

// Helemaal crazy
#define LED_SKIPS 3
#define COMET_SIZE 20
// CRGB leds[NUM_LEDS] = {0};
CRGB leds[NUM_LEDS * 3] = {0};
CHSV HSV_leds[NUM_LEDS * 3] = {CHSV(0, 0, 0)};
int hue_comet[NUM_LEDS * 3] = {0};
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
        leds[implode_index] = CRGB::White; // bright flash
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

// ----------------- Comet manager (non-blocking, mirrored comets) -----------------
#define MAX_COMETS 12
const unsigned long COMET_MOVE_INTERVAL_MS = 20; // <- smaller = faster animation (was 40). Tweak this.
unsigned long cometMoveInterval = COMET_MOVE_INTERVAL_MS; // ms between steps

// Color strength constants (tweak for more or less vivid colors)
const int BASE_SAT = 220; // base saturation (higher -> more color, 0 = grayscale)
const int SAT_DECAY_PER_STEP = 10;
const int BASE_BRI = 255; // base brightness
const int BRI_DECAY_PER_STEP = 18;

struct Comet {
  int head;            // current head index (physical LED index)
  int startPos;        // original start index (used for deterministic tail noise)
  int dir;             // +1 = moving toward larger indices, -1 = toward 0
  uint8_t hue;         // hue
  bool active;         // active flag
  unsigned long lastMove; // last move time
};

Comet comets[MAX_COMETS];

// Initialize comet pool
void initComets() {
  for (int i = 0; i < MAX_COMETS; i++) {
    comets[i].active = false;
  }
}

// Start two symmetric comets from startIndex with same hue (one left, one right)
void startDualComet(int startIndex, uint8_t hue) {
  // find up to two free slots and start them with dir -1 and +1
  int started = 0;
  for (int dir = -1; dir <= 1 && started < 2; dir += 2) {
    for (int s = 0; s < MAX_COMETS; s++) {
      if (!comets[s].active) {
        comets[s].active = true;
        comets[s].head = startIndex;
        comets[s].startPos = startIndex;
        comets[s].dir = dir;
        comets[s].hue = hue;
        comets[s].lastMove = millis();
        started++;
        break;
      }
    }
  }
}

// Deterministic tiny "noise" to keep mirrored tails identical
static inline uint8_t comet_noise(uint8_t hue, int startPos, int k) {
  // cheap hash -> deterministic across both comets started from same startPos
  uint32_t v = ((uint32_t)hue * 47u) + ((uint32_t)startPos * 13u) + ((uint32_t)k * 29u);
  // mix bits a bit:
  v ^= (v >> 8);
  v *= 199873u;
  return (uint8_t)(v & 0xFF); // 0..255
}

// Call this each loop to update & draw active comets
void updateComets() {
  unsigned long now = millis();

  // Clear non-persistent LED layer so comet pixels do not accumulate.
  // Persistent LEDs get redrawn by updatePersistentLeds() after this.
  for (int i = 0; i < NUM_LEDS; i++) {
    if (!persistent_led[i]) leds[i] = CRGB::Black;
  }

  for (int c = 0; c < MAX_COMETS; c++) {
    if (!comets[c].active) continue;

    // Move the comet head when interval elapsed
    if (now - comets[c].lastMove >= cometMoveInterval) {
      comets[c].lastMove = now;
      comets[c].head += comets[c].dir * LED_SKIPS;

      // Deactivate when head leaves physical LED range
      if (comets[c].head < 0 || comets[c].head >= NUM_LEDS) {
        comets[c].active = false;
        continue;
      }
    }

    // Draw the tail *behind* the head. Use formula: pos = head - dir * k
    // That makes the tail lie opposite the direction of motion.
    for (int k = 0; k < COMET_SIZE; k++) {
      int pos = comets[c].head - comets[c].dir * k;
      if (pos < 0 || pos >= NUM_LEDS) continue;
      if (persistent_led[pos]) continue; // don't overwrite persistent pixels

      // deterministic "noise" so both comets look symmetric
      uint8_t noise = comet_noise(comets[c].hue, comets[c].startPos, k);

      // Saturation: start high, decay with k, small noise
      int sat = BASE_SAT - k * SAT_DECAY_PER_STEP - (noise & 0x1F);
      if (sat < 20) sat = 20;    // ensure at least a little color
      if (sat > 255) sat = 255;

      // Brightness: start high, decay with k, influenced by noise
      int bri = BASE_BRI - k * BRI_DECAY_PER_STEP - (noise & 0x3F);
      if (bri < 0) bri = 0;
      if (bri > 255) bri = 255;

      leds[pos] = CHSV(comets[c].hue, (uint8_t)sat, (uint8_t)bri);
    }
  }
}
// -------------------------------------------------------------------------------

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
            // (left as original logic; you can change to CHSV for more consistent color behavior)
            leds[i] = CRGB(hue_comet[i] == 0 ? 100 : hue_comet[i], 255, 255);

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
        // START MIRRORED COLORED COMETS
        startDualComet(randomStartLed(), random(256)); // random hue
        timer_comet = millis();                    // Reset idle timer
        double_drum_hit++;
        double_timer = millis(); // Start double interval
    }

    if (rs485ReadInt() == 1) // Trigger from double hit
    {
        startDualComet(randomStartLed(), random(256)); // Trigger comet with color
        timer_comet = millis();                    // Reset idle timer
        double_drum_hit = 0;
    }
}

void randomTrigger()
{
    int min_interval = 5000;  // 1 minute
    int max_interval = 10000; // 10 minutes
    int interval = 5000;          // Start interval = 0

    if (timer_comet + interval < millis())
    {
        // interval = random(min_interval, max_interval); // Calculate new random interval in range
        startDualComet(randomStartLed(), random(256));     // Trigger comet with random color
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
    // FastLED.addLeds<NEOPIXEL, LED_RING_DATA_PIN>(leds, 0, NUM_LED_RING_PIXELS);

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 0, NUM_LEDS_STRIP_3M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_2>(leds, led_strip_offset[1], NUM_LEDS_STRIP_3M);

    FastLED.addLeds<NEOPIXEL, DATA_PIN_3>(leds, led_strip_offset[2], NUM_LEDS_STRIP_5M);
    FastLED.addLeds<NEOPIXEL, DATA_PIN_4>(leds, led_strip_offset[3], NUM_LEDS_STRIP_5M);

    initComets();            // initialize comet pool
    timer_comet = millis();
}

void loop()
{
    // triggerLedstripAnimation();
    randomTrigger();

    // NEW: updateComets (non-blocking, draws both directions)
    updateComets();

    updatePersistentLeds();
    updateImplode();
    // doubleHitAnimation();
    FastLED.show();
}
