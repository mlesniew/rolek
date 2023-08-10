#pragma once

#include <PicoUtils.h>

#include "remote.h"

extern Remote remote;

class Shutter: public PicoUtils::Periodic {
    public:
        Shutter(unsigned int index, unsigned long open_time_ms, unsigned long close_time_ms)
            : PicoUtils::Periodic(3), index(index),
              open_time_ms(open_time_ms), close_time_ms(close_time_ms),
              position(std::numeric_limits<double>::quiet_NaN()), last_command(COMMAND_STOP) {
        }

        Shutter(unsigned int index, unsigned long open_close_time_ms = 10 * 1000)
            : Shutter(index, open_close_time_ms, open_close_time_ms) {
        }

        void execute(command_t command);
        void on_execute(command_t command);
        void periodic_proc() override;

        const unsigned int index;
        const unsigned long open_time_ms, close_time_ms;

    protected:
        void update_position();

        double position;  // 0 == closed, 100 == open
        command_t last_command;
        PicoUtils::Stopwatch last_command_time;
};
