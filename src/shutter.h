#pragma once

#include "remote.h"

extern Remote remote;

class Shutter {
    public:
        Shutter(unsigned int index, unsigned long open_time_ms, unsigned long close_time_ms)
            : index(index), open_time_ms(open_time_ms), close_time_ms(close_time_ms) {
        }

        Shutter(unsigned int index, unsigned long open_close_time_ms = 10 * 1000)
            : Shutter(index, open_close_time_ms, open_close_time_ms) {
        }

        void execute(command_t command) { remote.execute(index, command); }

        const unsigned int index;
        const unsigned long open_time_ms, close_time_ms;
};

