#include "LowPower.h"

/*
 * A key holder that shouts at you if you forget to place your keys on it when you get home.
 *
 * Alarm is primed a short period after keys leave it, allowing time to exit without triggering.
 *
 * Upon arrival, the alarm will sound if the keys haven't been placed within the key holder within a grace period.
 *
 * The alarm will time itself out after a few minutes, to prevent running battery & neighbors to the ground in case of false activations.
*/

const int PIN_DOOR = 2;
const int PIN_KEYS = 3;
const int PIN_DEVMODE = 12;

const int LED_DOOR = 4;
const int LED_KEYS = 5;
const int LED_PRIMED = LED_BUILTIN;

const int BUZZER = 11;

// The alarm will prime after keys have been missing for this long (i.e. there's time to leave without triggering the alarm, but it will be active when the door next opens – on arrival).
const unsigned long KEYS_TIMEOUT = 5ul * 60ul * 1000ul;
const unsigned long KEYS_TIMEOUT_DEV = 5ul * 1000ul;

// How much grace there is to deactive the alarm (place keys) after opening the door
const unsigned long ALARM_GRACE_TIMEOUT = 60ul * 1000ul;
const unsigned long ALARM_GRACE_TIMEOUT_DEV = 10ul * 1000ul;

// In case of false activations, switch the alarm off after this long.
const unsigned long ALARM_SOUNDING_TIMEOUT = 2ul * 60ul * 1000ul;
const unsigned long ALARM_SOUNDING_TIMEOUT_DEV = 10ul * 1000ul;

const bool DOOR_OPEN = true;
const bool DOOR_CLOSED = false;
bool door_state = DOOR_CLOSED;
bool last_door_state = DOOR_CLOSED;
unsigned long door_opened_at_ms = 0;

const bool KEYS_PRESENT = true;
const bool KEYS_MISSING = false;
bool keys_state = KEYS_MISSING;
bool last_keys_state = KEYS_MISSING;
unsigned long keys_missing_at_ms = 0;

const byte ALARM_OFF = 0x00;
const byte ALARM_PRIMED = 0x01;
const byte ALARM_TRIGGERED = 0x02;
const byte ALARM_SOUNDING = 0x03;
byte alarm_state = ALARM_OFF;
unsigned long alarm_triggered_at_ms = 0;
unsigned long alarm_sounding_at_ms = 0;
bool alarm_sounded = false;

unsigned long current_ms = 0;
bool dev_mode = false;

void setup()
{
    Serial.begin(9600);

    pinMode(PIN_DOOR, INPUT_PULLUP);
    pinMode(PIN_KEYS, INPUT_PULLUP);
    pinMode(PIN_DEVMODE, INPUT_PULLUP);

    pinMode(LED_DOOR, OUTPUT);
    pinMode(LED_KEYS, OUTPUT);
    pinMode(LED_PRIMED, OUTPUT);

    // Wake up when door opens or keys leave
    attachInterrupt(digitalPinToInterrupt(PIN_DOOR), wakeUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_KEYS), wakeUp, CHANGE);
}

void loop()
{
    current_ms = millis();
    dev_mode = digitalRead(PIN_DEVMODE) == LOW ? true : false;

    // Check for door and keys. INPUT_PULLUP means these will be low when circuit connected.
    door_state = digitalRead(PIN_DOOR) == LOW ? DOOR_CLOSED : DOOR_OPEN;
    keys_state = digitalRead(PIN_KEYS) == LOW ? KEYS_PRESENT : KEYS_MISSING;

    // We want to track when door opens, or keys leave
    if (door_state == DOOR_OPEN && door_state != last_door_state)
    {
        door_opened_at_ms = current_ms;
    }
    if (keys_state == KEYS_MISSING && keys_state != last_keys_state)
    {
        keys_missing_at_ms = current_ms;
    }

    // Finished with historical state, commit new state
    last_door_state = door_state;
    last_keys_state = keys_state;

    // Presence of keys immediately resets the alarm.
    if (alarm_state != ALARM_OFF && keys_state == KEYS_PRESENT)
    {
        alarm_state = ALARM_OFF;
        Serial.print("Alarm dismissed at ");
        Serial.println(current_ms);
    }

    // Otherwise, lets deal with any alarm state transitions
    switch (alarm_state)
    {
    case ALARM_OFF:
        if (keys_state == KEYS_MISSING && current_ms - keys_missing_at_ms > (dev_mode ? KEYS_TIMEOUT_DEV : KEYS_TIMEOUT))
        {
            alarm_state = ALARM_PRIMED;
            Serial.print("Priming alarm at ");
            Serial.println(current_ms);
        }

        alarm_sounded = false;
        noTone(BUZZER);
        break;

    case ALARM_PRIMED:
        // Time check is so we don't trigger the alarm again from the same opening if the first alarm timed out.
        if (door_state == DOOR_OPEN && current_ms - door_opened_at_ms < (dev_mode ? ALARM_SOUNDING_TIMEOUT_DEV : ALARM_SOUNDING_TIMEOUT))
        {
            alarm_state = ALARM_TRIGGERED;
            alarm_triggered_at_ms = current_ms;
            Serial.print("Alarm triggered at ");
            Serial.println(current_ms);
        }
        break;

    case ALARM_TRIGGERED:
        if (current_ms - alarm_triggered_at_ms > (dev_mode ? ALARM_GRACE_TIMEOUT_DEV : ALARM_GRACE_TIMEOUT))
        {
            alarm_state = ALARM_SOUNDING;
            alarm_sounding_at_ms = current_ms;
            Serial.print("Alarm sounding at ");
            Serial.println(current_ms);
        }
        break;

    case ALARM_SOUNDING:
        if (current_ms - alarm_sounding_at_ms > (dev_mode ? ALARM_SOUNDING_TIMEOUT_DEV : ALARM_SOUNDING_TIMEOUT))
        {
            alarm_state = ALARM_OFF;
            Serial.print("Alarm timed out at ");
            Serial.println(current_ms);
        }
        if (!alarm_sounded)
        {
            tone(BUZZER, 1000);
            alarm_sounded = true;
        }
        break;
    }

    // In dev mode, represent current state via LEDs
    if (dev_mode)
    {
        digitalWrite(LED_DOOR, door_state == DOOR_OPEN ? HIGH : LOW);
        digitalWrite(LED_KEYS, keys_state == KEYS_MISSING ? HIGH : LOW);

        switch (alarm_state)
        {
        case ALARM_OFF:
            digitalWrite(LED_PRIMED, LOW);
            break;

        case ALARM_PRIMED:
            digitalWrite(LED_PRIMED, HIGH);
            break;

        case ALARM_TRIGGERED:
            digitalWrite(LED_PRIMED, (current_ms / 250) % 2 ? HIGH : LOW);
            break;

        case ALARM_SOUNDING:
            digitalWrite(LED_PRIMED, (current_ms / 100) % 2 ? HIGH : LOW);
            break;
        }
    }
    else
    {
        digitalWrite(LED_DOOR, LOW);
        digitalWrite(LED_KEYS, LOW);
        digitalWrite(LED_PRIMED, LOW);
    }

    // We can sleep if the alarm's off and keys are present (not counting down to prime)
    // Or the alarm is primed (door opening will wake and immediately transition to ALARM_TRIGGERED)
    if ((alarm_state == ALARM_OFF && keys_state == KEYS_PRESENT) || alarm_state == ALARM_PRIMED)
    {
        Serial.print("Going to sleep at ");
        Serial.println(current_ms);

        // Give the serial logs a chance to send before actual power down
        delay(250);
        LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    }
}

void wakeUp()
{
    Serial.print("Waking up at ");
    Serial.println(current_ms);
}