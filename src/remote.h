#pragma once

enum command_t { COMMAND_DOWN = 'd', COMMAND_UP = 'u', COMMAND_STOP = 's' };
enum button_t { BUTTON_DOWN, BUTTON_UP, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_STOP };

class Remote {
    public:
        void init();
        void reset();
        void execute(unsigned int index, const command_t command);

    protected:
        void push(button_t button, unsigned long time);
        void navigate_to(unsigned int index);
        void execute_command(const command_t command);

        unsigned int current_index;
};
