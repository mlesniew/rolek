#include <Arduino.h>

#include <PicoSyslog.h>
#include <PicoUtils.h>

#include "remote.h"

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_ST D7
#define PIN_EN D2
#define PIN_LED D3

#define DEFAULT_INDEX 1

extern PicoSyslog::Logger syslog;

namespace {

PicoUtils::PinOutput active_led(D3, true);
PicoUtils::Blink blink(active_led, 0b10, 10);

void init_output(unsigned int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

}

void Remote::init() {
    syslog.println(F("Initializing outputs..."));
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(PIN_ST);
    reset();
}

void Remote::reset() {
    {
        PicoUtils::BackgroundBlinker bb(blink);
        blink.set_pattern(0b10);

        syslog.println(F("Resetting remote..."));

        digitalWrite(PIN_EN, LOW);
        delay(5000);
        digitalWrite(PIN_EN, HIGH);
        delay(1000);
        current_index = DEFAULT_INDEX;
        syslog.println(F("Reset complete."));
    }

    blink.set_pattern(0);
    active_led.set(false);
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

    syslog.printf("Pressing %s button (pin %u) for %lu ms.\n", desc, pin, time);

    {
        PicoUtils::BackgroundBlinker bb(blink);
        blink.set_pattern(0b1100);

        digitalWrite(pin, HIGH);
        delay(time);
        digitalWrite(pin, LOW);
        delay(time);
    }

    blink.set_pattern(0);
    active_led.set(false);
}

void Remote::navigate_to(unsigned int index) {
    syslog.printf("Going from index %u to index %u\n", current_index, index);

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
            syslog.printf("  Opening %u\n", current_index);
            push(BUTTON_UP, 250);
            break;
        case COMMAND_DOWN:
            syslog.printf("  Closing %u\n", current_index);
            push(BUTTON_DOWN, 250);
            break;
        case COMMAND_STOP:
        default:
            syslog.printf("  Stopping %u\n", current_index);
            push(BUTTON_STOP, 250);
            break;
    }
}

void Remote::execute(unsigned int index, const command_t command) {
    navigate_to(index);
    execute_command(command);
}
