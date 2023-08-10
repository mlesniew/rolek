#include <Arduino.h>

#include "remote.h"

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_ST D7
#define PIN_EN D2

#define DEFAULT_INDEX 1

static void init_output(unsigned int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void Remote::init() {
    Serial.println(F("Initializing outputs..."));
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(PIN_ST);
    reset();
}

void Remote::reset() {
    Serial.println(F("Resetting remote..."));
    digitalWrite(PIN_EN, LOW);
    delay(5000);
    digitalWrite(PIN_EN, HIGH);
    delay(1000);
    current_index = DEFAULT_INDEX;
    Serial.println(F("Reset complete."));
}

void Remote::push(button_t button, unsigned long time) {
    const char * desc;
    unsigned int pin = 0;
    switch (button) {
        case BUTTON_DOWN:
            desc = "DOWN";
            pin = PIN_DN;
            break;
        case BUTTON_UP:
            desc = "UP";
            pin = PIN_UP;
            break;
        case BUTTON_LEFT:
            desc = "LEFT";
            pin = PIN_LT;
            break;
        case BUTTON_RIGHT:
            desc = "RIGHT";
            pin = PIN_RT;
            break;
        case BUTTON_STOP:
            desc = "STOP";
            pin = PIN_ST;
            break;
        default:
            return;
    }

    printf("Pressing %s button (pin %u) for %lu ms.\n", desc, pin, time);

    digitalWrite(pin, HIGH);
    delay(time);
    digitalWrite(pin, LOW);
    delay(time);
}

void Remote::navigate_to(unsigned int index) {
    printf("Going from index %u to index %u\n", current_index, index);

    while (current_index != index) {
        if (current_index < index) {
            push(BUTTON_RIGHT, 100);
            current_index++;
        } else {
            push(BUTTON_LEFT, 100);
            current_index--;
        }
    }
}

void Remote::execute_command(const command_t command) {
    switch (command) {
        case COMMAND_UP:
            printf("  Opening %u\n", current_index);
            push(BUTTON_UP, 250);
            break;
        case COMMAND_DOWN:
            printf("  Closing %u\n", current_index);
            push(BUTTON_DOWN, 250);
            break;
        case COMMAND_STOP:
        default:
            printf("  Stopping %u\n", current_index);
            push(BUTTON_STOP, 250);
            break;
    }
}

void Remote::execute(unsigned int index, const command_t command) {
    navigate_to(index);
    execute_command(command);
}
