#pragma once

#include <map>
#include <PicoUtils.h>

#include "remote.h"

extern Remote remote;

class Shutter {
    public:
        Shutter(unsigned int index, unsigned long open_time_ms, unsigned long close_time_ms)
            : index(index),
              open_time_ms(open_time_ms), close_time_ms(close_time_ms),
              position(std::numeric_limits<double>::quiet_NaN()), state(COMMAND_STOP) {
        }

        Shutter(unsigned int index, unsigned long open_close_time_ms = 30 * 1000)
            : Shutter(index, open_close_time_ms, open_close_time_ms) {
        }

        void tick();

        void execute(command_t command);
        void on_execute(command_t command);

        double get_position() const { return position; }
        command_t get_state() const { return state; }

        const unsigned int index;
        const unsigned long open_time_ms, close_time_ms;

    protected:
        void update_position_ans_state();

        PicoUtils::TimedValue<double> position;  // 0 == closed, 100 == open
        PicoUtils::TimedValue<command_t> state;

        PicoUtils::Stopwatch last_hass_update_time;
};

extern std::map<std::string, Shutter> blinds;
