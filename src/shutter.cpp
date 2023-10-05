#include <Arduino.h>

#include <PicoSyslog.h>

#include "shutter.h"
#include "hass.h"

extern PicoSyslog::Logger syslog;

void Shutter::execute(command_t command) {
    remote.execute(index, command);
    on_execute(command);
}

void Shutter::on_execute(command_t command) {
    update_position_and_state();
    state = command;
}

void Shutter::update_position_and_state() {

    const unsigned long elapsed_millis = std::min(state.elapsed_millis(), position.elapsed_millis());

    if (elapsed_millis < 500) {
        // update at most every 500 ms
        return;
    }

    unsigned long total_time_ms = 0;
    double direction = 0;

    switch (state) {
        case COMMAND_UP:
            total_time_ms = open_time_ms;
            direction = 1;
            break;
        case COMMAND_DOWN:
            total_time_ms = close_time_ms;
            direction = -1;
            break;
        default:
            return;
    }

    if (std::isnan(position)) {
        if (elapsed_millis > total_time_ms) {
            position = 50 + 50 * direction;
            state = COMMAND_STOP;
        }
    } else {
        position = position + direction * (double(elapsed_millis) / double(open_time_ms) * 100);
        if (position > 100) {
            position = 100;
            state = COMMAND_STOP;
        }
        if (position < 0) {
            position = 0;
            state = COMMAND_STOP;
        }
    }
}

void Shutter::tick() {
    update_position_and_state();
}
