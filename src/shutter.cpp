#include <Arduino.h>

#include "shutter.h"
#include "hass.h"

void Shutter::execute(command_t command) {
    remote.execute(index, command);
    on_execute(command);
}

void Shutter::on_execute(command_t command) {
    update_position();
    last_command = command;
    last_command_time.reset();
}

void Shutter::update_position() {
    const unsigned long elapsed_millis = last_command_time.elapsed_millis();

    switch (last_command) {
        case COMMAND_UP: {
            const double new_pos = position + (double(elapsed_millis) / double(open_time_ms) * 100);
            if ((elapsed_millis >= open_time_ms) || (new_pos >= 100)) {
                position = 100;
                last_command = COMMAND_STOP;
            } else {
                position = new_pos;
            }
            break;
        }

        case COMMAND_DOWN: {
            const double new_pos = position - (double(elapsed_millis) / double(close_time_ms) * 100);
            if ((elapsed_millis >= close_time_ms) || (new_pos <= 0)) {
                position = 0;
                last_command = COMMAND_STOP;
            } else {
                position = new_pos;
            }
            break;
        }

        default:
            // noop
            ;
    }

    if (!std::isnan(position))
        last_command_time.reset();

    printf("Position of shutter #%u: %.1f\n", index, position);
}

void Shutter::periodic_proc() {
    update_position();
    HomeAssistant::notify_state(index, last_command);
    HomeAssistant::notify_position(index, position);
}
