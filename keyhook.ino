#include <LowPower.h>

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
const unsigned long ALARM_SOUNDING_TIMEOUT_DEV = 30ul * 1000ul;

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

unsigned long current_ms = 0;
bool dev_mode = false;

// Melody logic derived from code in https://github.com/robsoncouto/arduino-songs
// But adapted to be non-blocking

/* 
  Keyboard cat
  Connect a piezo buzzer or speaker to pin 11 or select a new pin.
  More songs available at https://github.com/robsoncouto/arduino-songs                                            
                                              
                                              Robson Couto, 2019
*/
#define NOTE_B0 31
#define NOTE_C1 33
#define NOTE_CS1 35
#define NOTE_D1 37
#define NOTE_DS1 39
#define NOTE_E1 41
#define NOTE_F1 44
#define NOTE_FS1 46
#define NOTE_G1 49
#define NOTE_GS1 52
#define NOTE_A1 55
#define NOTE_AS1 58
#define NOTE_B1 62
#define NOTE_C2 65
#define NOTE_CS2 69
#define NOTE_D2 73
#define NOTE_DS2 78
#define NOTE_E2 82
#define NOTE_F2 87
#define NOTE_FS2 93
#define NOTE_G2 98
#define NOTE_GS2 104
#define NOTE_A2 110
#define NOTE_AS2 117
#define NOTE_B2 123
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
#define NOTE_C6 1047
#define NOTE_CS6 1109
#define NOTE_D6 1175
#define NOTE_DS6 1245
#define NOTE_E6 1319
#define NOTE_F6 1397
#define NOTE_FS6 1480
#define NOTE_G6 1568
#define NOTE_GS6 1661
#define NOTE_A6 1760
#define NOTE_AS6 1865
#define NOTE_B6 1976
#define NOTE_C7 2093
#define NOTE_CS7 2217
#define NOTE_D7 2349
#define NOTE_DS7 2489
#define NOTE_E7 2637
#define NOTE_F7 2794
#define NOTE_FS7 2960
#define NOTE_G7 3136
#define NOTE_GS7 3322
#define NOTE_A7 3520
#define NOTE_AS7 3729
#define NOTE_B7 3951
#define NOTE_C8 4186
#define NOTE_CS8 4435
#define NOTE_D8 4699
#define NOTE_DS8 4978
#define REST 0

// change this to make the song slower or faster
int tempo = 160;

// notes of the moledy followed by the duration.
// a 4 means a quarter note, 8 an eighteenth , 16 sixteenth, so on
// !!negative numbers are used to represent dotted notes,
// so -4 means a dotted quarter note, that is, a quarter plus an eighteenth!!
int melody[] = {

    // Keyboard cat
    // Score available at https://musescore.com/user/142788/scores/147371

    REST,
    1,
    REST,
    1,
    NOTE_C4,
    4,
    NOTE_E4,
    4,
    NOTE_G4,
    4,
    NOTE_E4,
    4,
    NOTE_C4,
    4,
    NOTE_E4,
    8,
    NOTE_G4,
    -4,
    NOTE_E4,
    4,
    NOTE_A3,
    4,
    NOTE_C4,
    4,
    NOTE_E4,
    4,
    NOTE_C4,
    4,
    NOTE_A3,
    4,
    NOTE_C4,
    8,
    NOTE_E4,
    -4,
    NOTE_C4,
    4,
    NOTE_G3,
    4,
    NOTE_B3,
    4,
    NOTE_D4,
    4,
    NOTE_B3,
    4,
    NOTE_G3,
    4,
    NOTE_B3,
    8,
    NOTE_D4,
    -4,
    NOTE_B3,
    4,

    NOTE_G3,
    4,
    NOTE_G3,
    8,
    NOTE_G3,
    -4,
    NOTE_G3,
    8,
    NOTE_G3,
    4,
    NOTE_G3,
    4,
    NOTE_G3,
    4,
    NOTE_G3,
    8,
    NOTE_G3,
    4,
    NOTE_C4,
    4,
    NOTE_E4,
    4,
    NOTE_G4,
    4,
    NOTE_E4,
    4,
    NOTE_C4,
    4,
    NOTE_E4,
    8,
    NOTE_G4,
    -4,
    NOTE_E4,
    4,
    NOTE_A3,
    4,
    NOTE_C4,
    4,
    NOTE_E4,
    4,
    NOTE_C4,
    4,
    NOTE_A3,
    4,
    NOTE_C4,
    8,
    NOTE_E4,
    -4,
    NOTE_C4,
    4,
    NOTE_G3,
    4,
    NOTE_B3,
    4,
    NOTE_D4,
    4,
    NOTE_B3,
    4,
    NOTE_G3,
    4,
    NOTE_B3,
    8,
    NOTE_D4,
    -4,
    NOTE_B3,
    4,

    NOTE_G3,
    -1,

};

// sizeof gives the number of bytes, each int value is composed of two bytes (16 bits)
// there are two values per note (pitch and duration), so for each note there are four bytes
int notes = sizeof(melody) / sizeof(melody[0]) / 2;

// this calculates the duration of a whole note in ms
int wholenote = (60000 * 4) / tempo;

boolean buzzerActive = false;
int currentNote = 0;
unsigned long nextNoteAt = 0;
int divider;
int noteDuration;

void setup()
{
    CLKPR = 0x80; // (1000 0000) enable change in clock frequency
    CLKPR = 0x01; // (0000 0001) use clock division factor 2 to reduce the frequency from 16 MHz to 8 MHz

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

        if (buzzerActive)
        {
            stopMelody();
        }
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
        playMelody();
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
    if ((alarm_state == ALARM_OFF && keys_state == KEYS_PRESENT) || (alarm_state == ALARM_PRIMED && door_state == DOOR_CLOSED))
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

void playMelody()
{
    buzzerActive = true;

    if (nextNoteAt != 0 && current_ms < nextNoteAt)
    {
        return;
    }

    if (currentNote > notes * 2)
    {
        currentNote = 0;
    }

    // calculates the duration of each note
    divider = melody[currentNote + 1];
    if (divider > 0)
    {
        // regular note, just proceed
        noteDuration = (wholenote) / divider;
    }
    else if (divider < 0)
    {
        // dotted notes are represented with negative durations!!
        noteDuration = (wholenote) / abs(divider);
        noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(BUZZER, melody[currentNote], noteDuration * 0.9);

    nextNoteAt = current_ms + noteDuration;
    currentNote += 2;
}

void stopMelody()
{
    noTone(BUZZER);
    buzzerActive = false;
    currentNote = 0;
    nextNoteAt = 0;
}