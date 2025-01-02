#include <Arduino.h>

#include <PicoSyslog.h>

#include "shutter.h"
#include "hass.h"

extern PicoSyslog::Logger syslog;

void Shutter::set_position(double new_position) {
    if (new_position >= 100) {
        desired_position = std::numeric_limits<double>::quiet_NaN();
        execute(COMMAND_UP);
    } else if (new_position <= 0) {
        desired_position = std::numeric_limits<double>::quiet_NaN();
        execute(COMMAND_DOWN);
    } else {
        desired_position = new_position;

        if (std::isnan(position)) {
            // position currently unknown
            syslog.printf("Shutter %i position unknown, %sing first...\n", index, desired_position > 50 ? "open" : "clos");
            execute(desired_position > 50 ? COMMAND_UP : COMMAND_DOWN);
        } else {
            syslog.printf("Shutter %i %sing to reach position %i.\n", index, desired_position > position ? "open" : "clos",
                          int(desired_position));
            execute(desired_position > position ? COMMAND_UP : COMMAND_DOWN);
        }
    }
}

void Shutter::sync() {
    double new_desired_position = std::isnan(desired_position);
    process(COMMAND_STOP);
    if (std::isnan(new_desired_position)) {
        new_desired_position = std::isnan(position) ? 50.0 : double(position);
    }
    position = std::numeric_limits<double>::quiet_NaN();
    set_position(new_desired_position);
}

void Shutter::process(command_t command) {
    desired_position = std::numeric_limits<double>::quiet_NaN();
    execute(command);
}

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

    if (elapsed_millis < 100) {
        // update at most every 100 ms
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
        if (elapsed_millis >= total_time_ms) {
            position = 50 + 50 * direction;
            state = COMMAND_STOP;
        }
    } else {
        position = position + direction * (double(elapsed_millis) / double(open_time_ms) * 100);
        if (position >= 100) {
            position = 100;
            state = COMMAND_STOP;
        }
        if (position <= 0) {
            position = 0;
            state = COMMAND_STOP;
        }
    }
}

void Shutter::tick() {
    update_position_and_state();
    if (!std::isnan(position) && !std::isnan(desired_position)) {

        if (
            ((state == COMMAND_UP) && (position >= desired_position)) ||
            ((state == COMMAND_DOWN) && (position <= desired_position))
        ) {
            execute(COMMAND_STOP);
            syslog.printf("Shutter %i reached desired position.\n", index);
            desired_position = std::numeric_limits<double>::quiet_NaN();
        } else if (state == COMMAND_STOP) {
            execute(desired_position > position ? COMMAND_UP : COMMAND_DOWN);
        }

    }
}
