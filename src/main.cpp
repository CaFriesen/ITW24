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
#define NUM_STRIPS 4
#define NUM_LEDS 960
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
#define BRIGHTNESS_RING 10
Adafruit_NeoPixel ring(NUM_LED_RING_PIXELS, LED_RING_DATA_PIN, NEO_GRB + NEO_KHZ800);
unsigned long led_ring_timer = 0;
int shutoff_delay = 150;
int ledPos = 0;
int red = 255;
int green = 255;
int blue = 0;
int rounds = 0;

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
};
STATE state;

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

void led_strip_setLeds()
{
    for (int i = 0; i < NUM_STRIPS; i++)
    {
        led_strip_controllers[i]->setLeds(led_head + led_strip_offset[i], led_strip_length[i]);
    }
}

void updateLedstrip()
{
    led_strip_setLeds();
    FastLED.show();
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

// Helper function for LED animation
int value_array_construction()
{
    for (int i = 0; i < COMET_SIZE; i++)
    {
        value_comet[i] = COMET_SIZE - (int)map(i, COMET_SIZE, 0, 255, 0);
    }
    return 0;
}

// [DEMO function, legacy]
// Constructs a red led section as big as the physical led strips itself
int animation_array_construction(CRGB *led_array, int r, int g, int b)
{

    for (int i = 0; i < NUM_LEDS; i++)
    {
        led_array[NUM_LEDS + i] = CRGB(r, g, b);
    }
    return 0;
}

// [DEMO function, legacy]
// Constructs a led section as big as the parameter l, h sets the hue
int animation_array_construction(CRGB *led_array, int l, int h)
{

    for (int i = 0; i < l; i++)
    {
        led_array[2 * NUM_LEDS - i].setHSV(h, 255, map(i, 0, l, 255, 0));
    }
    return 0;
}

void game_over_animation()
{
    led_head = &leds[NUM_LEDS];
    animation_array_construction(&leds[0], 255, 0, 0);
    updateLedstrip();
    delay(3000);
}

int idleAnimation()
{
    // Wat doet dit allemaal? Is dit nodig voor idle?
    CRGB *led_head = &leds[NUM_LEDS];
    long long led_timer = micros();

    for (int i = 0; i < NUM_LEDS; i++)
    {

        led_head += 1;
        led_strip_setLeds();
        FastLED.show();
        Serial.println(micros() - led_timer);
        led_timer = micros();
    }
    return 0;
    // mooie idle animatie die checkt op activiteit
}

//------------------------
// Setup helper functions
//------------------------

//[General function]
// Determines role (master/slave) of the device and provides the option to determine ring color
// Input: none
// Output: changes role variable (master/slave), returns 0 if behavior was as expected
int determineRole()
{
    if (digitalRead(15))
    {
        role = MASTER;
        set_total_ring_color(0, 255, 0);
        Serial.println("Master online");
        return 0;
    }
    if (digitalRead(5))
    {
        role = SLAVE;
        set_total_ring_color(0, 255, 0);
        Serial.println("Slave 5 online");
        return 0;
    }
    if (digitalRead(18))
    {
        role = SLAVE;
        set_total_ring_color(0, 255, 0);
        Serial.println("Slave 6 online");
        return 0;
    }
    if (digitalRead(19))
    {
        role = SLAVE;
        set_total_ring_color(0, 255, 0);
        Serial.println("Slave 7 online");
        return 0;
    }
    if (digitalRead(2))
    {
        role = SLAVE;
        set_total_ring_color(0, 255, 0);
        Serial.println("Slave 8 online");
        return 0;
    }
    return -1;
}

// stores adresses in sensor_adresses array dynamically
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
            sensor_adresses[i] = 4 + i;
        }
    }
}

//--------------
// I2C funtions
//--------------

//[General function]
// Determines I2C adress (master/slave) of the device
// Input: none
// Output: returns its adress
int determineI2CAdress()
{
    if (digitalRead(15))
    {
        return 0;
    }
    if (digitalRead(5))
    {
        return 5;
    }
    if (digitalRead(18))
    {
        return 6;
    }
    if (digitalRead(19))
    {
        return 7;
    }
    if (digitalRead(2))
    {
        return 8;
    }
    return -1;
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

int update_slave_state(STATE s)
{
    for (int i = 0; i < 4; i++)
    {
        Wire.beginTransmission(i + 5);
        Wire.write((uint8_t)s); // send state variable to slaves
        byte error = Wire.endTransmission();
        if (error)
        {
            error_handler_I2C_bus(error);
        }
    }

    return 0;
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
    message_read = 0;
    while (Wire.available())
    {
        message = Wire.read();
    }
}

//[Master function]
// send the slave a message
void message_slave(int slave_address)
{
    Wire.beginTransmission(slave_address);
    Wire.write('s'); // s = show
    byte error = Wire.endTransmission();
    if (error)
    {
        error_handler_I2C_bus(error);
    }
}

//[Master function]
// send the slave a message
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

//[Master function]
// send the slave a message
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

//----------------------
// Simon says functions
//----------------------

void showSimonSays()
{
    if (show_delay < millis() - show_timer)
    {
        // Reset timer
        show_timer = millis();
        // TODO: Communicate with other systems to turn LED ring (& beam?) on in the correct system

        // Increment counter
        Serial.printf("%d ", simon_says[simon_increment]);

        if (simon_says[simon_increment] == 0) // if-statement to decide who has to take action master/slave
        {
            animation_show_mole();
        }
        else
        {
            message_slave(sensor_adresses[simon_says[simon_increment]]);
        }
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
    int piezoMeasurement = analogRead(ANALOG_SENSOR_INPUT_PIN);
    piezoSensor.reading(piezoMeasurement);
    if (piezoMeasurement - piezoSensor.getAvg() > 2000 && millis() - hit_timer < 300)
    {
        Serial.print(piezoMeasurement);
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
            if (!is_mole_hit())
            {
                return 0; // 0 is the number assigned to this sensor
            }
        }
        else
        {
            Wire.requestFrom(i + 4, 1);
            while (Wire.available())
            {
                if (Wire.read() == 1)
                {
                    return i;
                }
            }
        }
    }
    return -1;
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
        if (sensor_adresses[current_value] == 0) // simpele plaats vervanger
        {
            led_ring_animation_correct_mole();
        }
        else
        {
            message_slave(sensor_adresses[current_value], 'g'); // g voor het engelse "good"
        }

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
            show_timer = millis();
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
            message_slave(sensor_adresses[current_value], 'w');
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

int gameStart()
{
    // insert start animation

    if (3 * NUM_LEDS >= NUM_LEDS * 2)
    {
        delay(500);
        // Update pattern
        updateSimonSays();
        // Start game
        Serial.print("Pattern: ");
        state = PATTERN_SHOW;
        update_slave_state(PATTERN_SHOW);
        show_timer = millis();
    }

    // zet 1 licht aan en wacht op de gebruiker, als dit te lang duurt terug naar idle

    return 0;
}
//{Setup functions}

void master_setup()
{
    store_sensor_adresses(); // fills the sensor_adresses array with correct adresses

    Wire.begin();
    Wire.setClock(10000);

    state = IDLE;
}

void slave_setup(int I2C_adress)
{
    Wire.begin(I2C_adress);
    Wire.setClock(10000);
    Wire.onReceive(receiveState);
    Wire.onRequest(sendIfHitDetected);
    message_read = 1;
}

//{Loop functions}

// general behavior
void general_loop()
{
    Serial.println(ESP.getFreeHeap());
    updateLedstrip();
    updateLedring();
}

// Slave behavior
void slave_loop()
{
    while (1)
    {
        if (!message_read)
        {
            message_read = 1;
            switch (message)
            {
            case '0':
                state = IDLE;
                Serial.println("IDLE");
                break;
            case '1':
                state = ACTIVE;
                Serial.println("ACTIVE");
                break;
            case '2':
                state = GAME_START;
                Serial.println("GAME_START");
                break;
            case '3':
                state = GAME_OVER;
                Serial.println("GAME_OVER");
                game_over_animation();
                break;
            case '4':
                state = PATTERN_SHOW;
                Serial.println("PATTERN_SHOW");
                break;
            case '5':
                state = BASIC_INTERACTION;
                Serial.println("BASIC_INTERACTION");
                break;
            case 's':
                Serial.println("Showing mole");
                animation_show_mole();
                break;
            case 'g':
                Serial.println("Correct mole was hit");
                led_ring_animation_correct_mole();
                break;
            case 'w':
                Serial.println("Wrong mole was hit");
                led_ring_animation_wrong_mole();
                break;
            default:
                break;
            }
        }

        int piezoMeasurement = analogRead(ANALOG_SENSOR_INPUT_PIN);
        piezoSensor.reading(piezoMeasurement);
        if (piezoMeasurement - piezoSensor.getAvg() > 2000 && millis() - hit_timer < 300 && state == ACTIVE)
        {
            hit_timer = millis();
            mole_is_hit = 1;
        }
        general_loop();
    }
}

// Master behavior
void master_loop()
{
    while (1)
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
            // Willen we een wacht animatie?
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
            game_over_animation();
            state = IDLE;
            break;

        default:
            break;
        }
        general_loop();
    }
}

void setup()
{
    led_ring_array_head = &led_ring_array[0];
    CRGB colorxj;
    colorxj.setRGB(255, 0, 0);
    Serial.begin(115200);
    Serial.println("SYSTEM BOOTED");

    // setup moving average
    piezoSensor.begin();
    for (int i = 0; i < 2000; i++)
    {
        piezoSensor.reading(analogRead(ANALOG_SENSOR_INPUT_PIN));
    }

    // setup led strips
    FastLED.setBrightness(100);
    led_strip_controllers[0] = &FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, 0, NUM_LEDS_STRIP_3M).setCorrection(TypicalLEDStrip);
    led_strip_controllers[1] = &FastLED.addLeds<CHIPSET, DATA_PIN_2, COLOR_ORDER>(leds, 180, NUM_LEDS_STRIP_3M).setCorrection(TypicalLEDStrip);
    led_strip_controllers[2] = &FastLED.addLeds<CHIPSET, DATA_PIN_3, COLOR_ORDER>(leds, 360, NUM_LEDS_STRIP_5M).setCorrection(TypicalLEDStrip);
    led_strip_controllers[3] = &FastLED.addLeds<CHIPSET, DATA_PIN_4, COLOR_ORDER>(leds, 660, NUM_LEDS_STRIP_5M).setCorrection(TypicalLEDStrip);

    led_ring_controller = &FastLED.addLeds<CHIPSET, LED_RING_DATA_PIN, COLOR_ORDER>(leds, 0, NUM_LED_RING_PIXELS).setCorrection(TypicalLEDStrip);
    hit_timer = 0;

    animation_array_construction(&leds[0], 100, 0, 0);
}

void hit_to_height(int height)
{
    for (int i = 0; i < height; i += 3)
    {
        led_head = &leds[NUM_LEDS * 2 - i];
        updateLedstrip();
        int x = map(i, 0, NUM_LEDS, 0, 10);
        delayMicroseconds(x * x);
        // Serial.println(x*x);
    }

    for (int i = height; i > 0; i--)
    {
        led_head = &leds[NUM_LEDS * 2 - i];
        updateLedstrip();
        int x = map(i, 0, NUM_LEDS, 0, 10);
        delayMicroseconds(x * x);
        // Serial.println(x*x);
    }
}

void loop()
{
    int rawValue = 0;
    while (rawValue < 50)
    {
        rawValue = analogRead(27);
        Serial.println(rawValue);
    }
    int mappedValue;
    mappedValue = map(rawValue, 0, 150, 0, 6);

    if (mappedValue > 6)
    {
        // Zet dit ergens neer voor een hit
        led_ring_animation_hit(random(255), random(255), random(255));
        mappedValue = 6;
    }

    Serial.print(mappedValue);
    Serial.print(" ");
    int destination = NUM_LEDS_SEGMENT * mappedValue + random(NUM_LEDS_SEGMENT / 2);
    Serial.println(destination);
    animation_array_construction(&leds[0], destination, random(255));
    hit_to_height(destination);

    // Zet dit in een idle state
    idleRingAnimation(20, 60, true, 20, 0, random(0, 255), random(0, 255));
    // Run dit constant in de loop
    dimLedring();
}
