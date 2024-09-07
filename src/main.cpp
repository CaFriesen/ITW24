#include <Arduino.h>
#include <FastLED.h>

// LED beam config
#define NUM_LEDS_STRIP_3M 180
#define NUM_LEDS_STRIP_5M 300
#define NUM_STRIPS 4
#define NUM_LEDS 960
#define DATA_PIN 33
#define DATA_PIN_2 32
#define DATA_PIN_3 25
#define DATA_PIN_4
#define DATA_PIN_5
#define COLOR_ORDER GRB
#define CHIPSET WS2812B

// Inputs and outputs
#define NUM_BEAMS 5
#define NUM_SENSORS 5
uint8_t beams[NUM_BEAMS];
uint8_t sensors[NUM_SENSORS];   // Sensors
int sensor_values[NUM_SENSORS]; // Values

// Game variables
#define MAX_COMETS 2 // ?
#define LED_SKIPS 3  // ?
#define COMET_SIZE 20
#define SENSOR_THRESHOLD 400
#define MAX_PATTERN_LENGTH 200
uint8_t simon_says[MAX_PATTERN_LENGTH];
uint8_t simon_increment = 0;
uint8_t simon_pattern_length = 0;

// Timer to reset to idle
unsigned long reset_timer = 0;
unsigned long reset_delay = 30000; // 30 sec

// Timer to show simon says pattern
unsigned long show_timer = 0;
unsigned long show_delay = 3000; // 3 sec

int moleActive;
int notPopped;
long long popUpTimeStamp;
long long popTime;
long long cooldown_timer_mole;
int first_time;

CRGB leds[NUM_LEDS * 3] = {0};
CHSV HSV_leds[NUM_LEDS * 3] = {CHSV(0, 0, 0)};
int hue_comet[NUM_LEDS * 3] = {0};
int value_comet[COMET_SIZE] = {0};

CLEDController *led_controller;

enum STATE
{
    IDLE,
    ACTIVE,
    GAME_START,
    GAME_OVER,
    PATTERN_SHOW,
    BASIC_INTERACTION
};
STATE state;

// Helper function for LED animation
int value_array_construction()
{
    for (int i = 0; i < COMET_SIZE; i++)
    {
        value_comet[i] = COMET_SIZE - map(i, COMET_SIZE, 0, 255, 0);
    }
}

// Helper function for LED animation
int animation_array_construction(CRGB *led_array)
{

    for (int i = 0; i < NUM_LEDS; i++)
    {
        led_array[NUM_LEDS + i] = CRGB::Red;
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

void showSimonSays()
{
    if (show_delay < millis() - show_timer)
    {
        // Reset timer
        show_timer = millis();
        // Turn LED on position "simon_increment"
        // Possibly LED beam + ring??

        // Increment counter
        Serial.printf("%d ", simon_says[simon_increment]);
        simon_increment++;
        // If entire pattern is shown
        if (simon_increment >= simon_pattern_length)
        {
            Serial.printf(", length: %d\n", simon_pattern_length);
            // Reset counter
            simon_increment = 0;
            // Become active
            state = ACTIVE;
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

int getInputData()
{
    return 99;
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
        // TODO: Start LED ring and strip animation for correct hit

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
        }
    }
    else if (current_value != 99)
    {
        Serial.printf("Sensor %d hit, incorrect\n", current_value);
        // TODO: Communicate to the rest of the system that game is over

        // Reset simon says
        resetSimonSays();
        // Reset counter
        simon_increment = 0;
        // End game
        state = GAME_OVER;
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

int gameStart()
{
    // zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle
    return 0;
}

// void basicInteraction() // vervangen / weghalen
// {
//     if (readIfMoleHit(SENSOR_THRESHOLD) && first_time)
//     {
//         hue_comet[0] = random(256);
//         first_time = false;
//     }
//     else
//     {
//         if (!readIfMoleHit(SENSOR_THRESHOLD))
//         {
//             first_time = true;
//         }
//     }
// }

void updateLedstrip()
{
    CRGB *led_head = &leds[0];

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
    break;

    default:
        // normal whac a mole operation
        break;
    }
    led_controller->setLeds(led_head, NUM_LEDS);
    FastLED.show();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("SYSTEM BOOTED");
    state = IDLE;
    FastLED.setBrightness(100);
    setCpuFrequencyMhz(240);
    animation_array_construction(leds);
    // value_array_construction();
    // led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    led_controller = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, 0, NUM_LEDS_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<CHIPSET, DATA_PIN_2, COLOR_ORDER>(leds, 180, NUM_LEDS_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<CHIPSET, DATA_PIN_3, COLOR_ORDER>(leds, 360, NUM_LEDS_STRIP).setCorrection(TypicalLEDStrip);

    popTime = 0;
    first_time = true;
}

void loop()
{

    switch (state)
    {
    case IDLE:
        if (getInputData() != 99)
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
        //      if (timeIsUp())
        //      {
        //        state = GAME_OVER;
        //      }
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

    updateLedstrip();
}
