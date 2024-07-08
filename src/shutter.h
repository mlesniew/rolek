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
              position(std::numeric_limits<double>::quiet_NaN()), state(COMMAND_STOP),
              desired_position(std::numeric_limits<double>::quiet_NaN()) {
        }

        Shutter(unsigned int index, unsigned long open_close_time_ms = 30 * 1000)
            : Shutter(index, open_close_time_ms, open_close_time_ms) {
        }

        void tick();

        void set_position(double position);
        void process(command_t cmd);

        void on_execute(command_t command);

        double get_position() const { return position; }
        command_t get_state() const { return state; }

        const unsigned int index;
        const unsigned long open_time_ms, close_time_ms;

    protected:
        void execute(command_t command);
        void update_position_and_state();

        PicoUtils::TimedValue<double> position;  // 0 == closed, 100 == open
        PicoUtils::TimedValue<command_t> state;

        double desired_position;
};

extern std::map<std::string, Shutter> blinds;
