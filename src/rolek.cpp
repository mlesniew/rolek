#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include <utils/led.h>
#include <utils/reset.h>
#include <utils/wifi_control.h>

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_ST D7
#define PIN_EN D2
#define PIN_LED D4

#define DEFAULT_INDEX 1

#define HOSTNAME "Rolek"
#ifndef PASSWORD
#define PASSWORD "password"
#endif

BlinkingLed wifi_led(PIN_LED, 0, 91, true);
WiFiControl wifi_control(wifi_led);
ESP8266WebServer server{80};

unsigned int current_index{DEFAULT_INDEX};

enum command_t { COMMAND_DOWN, COMMAND_UP, COMMAND_STOP };
enum button_t { BUTTON_DOWN, BUTTON_UP, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_STOP };

void reset_remote() {
    Serial.println(F("Resetting remote..."));
    digitalWrite(PIN_EN, LOW);
    delay(5000);
    digitalWrite(PIN_EN, HIGH);
    delay(1000);
    current_index = DEFAULT_INDEX;
    Serial.println(F("Reset complete."));
}

void push(button_t button, unsigned long time) {
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

const char * uptime()
{
    static char buf[100];
    const auto now = millis();

    const unsigned long seconds = (now / 1000) % 60;
    const unsigned long minutes = (now / (60 * 1000)) % 60;
    const unsigned long hours = (now / (60 * 60 * 1000)) % 24;
    const unsigned long days = now / (24 * 60 * 60 * 1000);

    snprintf(buf, 100, "%lu day%s, %lu:%02lu:%02lu", days, days == 1 ? "" : "s", hours, minutes, seconds);
    return buf;
}

void navigate_to(unsigned int index)
{
    printf("Going from index %u to index %u\n", current_index, index);

    while (current_index != index)
    {
        if (current_index < index)
        {
            push(BUTTON_RIGHT, 100);
            current_index++;
        } else {
            push(BUTTON_LEFT, 100);
            current_index--;
        }
    }
}

void init_output(unsigned int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void process_command(command_t command, unsigned int mask)
{
    printf("Iterating through mask 0x%02x...\n", mask);
    for(unsigned int idx = 0; mask && (idx < 8); ++idx)
    {
        unsigned int current = 1 << idx;
        if (mask & current)
        {
            navigate_to(idx);
            switch (command) {
                case COMMAND_UP:
                    printf("  Opening %u\n", idx);
                    push(BUTTON_UP, 250);
                    break;
                case COMMAND_DOWN:
                    printf("  Closing %u\n", idx);
                    push(BUTTON_DOWN, 250);
                    break;
                case COMMAND_STOP:
                default:
                    printf("  Stopping %u\n", idx);
                    push(BUTTON_STOP, 250);
                    break;
            }
            mask ^= current;
        }
    }
}

int mask_from_comma_separated_list(const String & str) {
    int ret = 0;
    int start_idx = 0;

    printf("Decoding mask %s\n", str.c_str());

    while (1) {
        int comma_idx = str.indexOf(',', start_idx);

        const long value = (comma_idx < 0 ? str.substring(start_idx) : str.substring(start_idx, comma_idx)).toInt();
        if ((value < 1) || (value > 8)) {
            // error, invalid value or error converting
            printf("Error at char %i.\n", start_idx);
            return 0;
        }

        printf(" * %li\n", value);
        ret |= 1 << (value - 1);

        if (comma_idx < 0)
            break;

        // next time start searching after the comma
        start_idx = comma_idx + 1;
    }
    printf("Parsing complete, mask: %i\n", ret);

    return ret;
}

void setup_endpoints()
{
    auto handler = [](command_t command) {
        unsigned int mask = 0;
        unsigned int count = 1;

        /* extract blinds parameter */
        if (!server.hasArg("blinds")) {
            // no argument, use all blinds
            mask = 1;
        } else {
            mask = mask_from_comma_separated_list(server.arg("blinds")) << 1;
            if (mask <= 0)
            {
                server.send(400, F("text/plain"), F("Invalid blinds argument"));
                return;
            }
        }

        /* extract count parameter */
        if (!server.hasArg("count"))
            count = 1;
        else {
            count = server.arg("count").toInt();
            if ((count < 1) || (count > 5))
            {
                server.send(400, F("text/plain"), F("Invalid count argument"));
                return;
            }
        }

        do {
            process_command(command, mask);
            delay(500);
        } while (--count > 0);

        server.send(200, F("text/plain"), F("OK"));
    };

    server.on("/up", [handler]{ handler(COMMAND_UP); });
    server.on("/down", [handler]{ handler(COMMAND_DOWN); });
    server.on("/stop", [handler]{ handler(COMMAND_STOP); });

    server.on("/reset", []{
            reset_remote();
            server.send(200, F("text/plain"), F("OK"));
            });

    server.on("/alive", []{
            server.send(200, F("text/plain"), F(HOSTNAME " is alive"));
            });

    server.on("/version", []{
            server.send(200, F("text/plain"), F(__DATE__ " " __TIME__));
            });

    server.on("/uptime", []{
            server.send(200, F("text/plain"), uptime());
            });

    server.serveStatic("/", LittleFS, "/index.html");

    server.serveStatic("/", LittleFS, "/");
}

void setup() {
    Serial.begin(9600);

    Serial.print(F(
        "\n\n"
        "8888888b.          888          888     \n"
        "888   Y88b         888          888     \n"
        "888    888         888          888     \n"
        "888   d88P .d88b.  888  .d88b.  888  888\n"
        "8888888P^ d88^^88b 888 d8P  Y8b 888 .88P\n"
        "888 T88b  888  888 888 88888888 888888K \n"
        "888  T88b Y88..88P 888 Y8b.     888 ^88b\n"
        "888   T88b ^Y88P^  888  ^Y8888  888  888\n"
        "\n"
        "https://github.com/mlesniew/rolek\n"
        "\n"
        "Built on: " __DATE__ " " __TIME__ "\n\n"));

    Serial.println(F("Initializing outputs..."));
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(PIN_ST);

    if (!wifi_control.init(WiFiInitMode::automatic, HOSTNAME, PASSWORD, 5 * 60)) {
        reset();
    }

    Serial.println(F("Initializing file system..."));
    LittleFS.begin();

    Serial.println(F("Setting up endpoints..."));
    setup_endpoints();

    // do an initial remote reset
    reset_remote();

    Serial.println(F("Starting up server..."));
    server.begin();

    Serial.println(F("Setup complete."));
}

void loop() {
    wifi_control.tick();
    server.handleClient();
}
