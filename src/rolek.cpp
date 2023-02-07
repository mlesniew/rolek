#include <map>

#include <uri/UriRegex.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <utils/io.h>
#include <utils/wifi_control.h>

#include "utils.h"

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

#if __has_include("customize.h")
#include "customize.h"
#else
#warning "Using default config, create customize.h to customize."

const std::map<std::string, unsigned int> blinds = {
    {"Living room", 1},
    {"Kitchen", 2},
    {"Bedroom", 3},
    {"Attic", 4},
};

const std::map<std::string, std::vector<std::string>> groups = {
    {"Downstairs", {"Living room", "Kitchen"}},
    {"Upstairs", {"Bedroom", "Attic"}},
};

#endif

PinOutput<D4, true> wifi_led;
WiFiControl wifi_control(wifi_led);
ESP8266WebServer server{80};

unsigned int current_index{DEFAULT_INDEX};

enum command_t { COMMAND_DOWN = 'd', COMMAND_UP = 'u', COMMAND_STOP = 's' };
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

void init_output(unsigned int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
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

void execute_command(const command_t command) {
    switch (command) {
        case COMMAND_UP:
            printf("  Opening %u\n", current_index);
            push(BUTTON_UP, 250);
            break;
        case COMMAND_DOWN:
            printf("  Closing %u\n", current_index);
            push(BUTTON_DOWN, 250);
            break;
        case COMMAND_STOP:
        default:
            printf("  Stopping %u\n", current_index);
            push(BUTTON_STOP, 250);
            break;
    }
}

bool process(const std::string & name, const command_t command) {
    if (name.empty()) {
        navigate_to(0);
        execute_command(command);
        return true;
    }

    {
        const auto it = blinds.find(name);
        if (it != blinds.end()) {
            const unsigned int position = it->second;
            navigate_to(position);
            execute_command(command);
            return true;
        }
    }

    {
        const auto it = groups.find(name);
        if (it != groups.end()) {
            const auto & group = it->second;
            for (const auto & element : group) {
                process(element, command);
            }
            return true;
        }
    }

    return false;
}

void setup_endpoints() {
    server.on(UriRegex("/blinds(.*)/(up|down|stop)"), HTTP_POST, [] {

        printf("POST %s\n", server.uri().c_str());

        std::string name = uri_unquote(server.pathArg(0).c_str());
        const char direction = server.pathArg(1).c_str()[0];

        if (!name.empty()) {
            // name must start with slash if present
            if (name[0] != '/') {
                server.send(404);
                return;
            }
            name = name.substr(1);
        }

        const bool success = process(name, command_t(direction));
        server.send(success ? 200 : 404);
    });

    server.on("/blinds", [] {
        StaticJsonDocument<1024> json;
        auto blinds_array = json["blinds"].to<JsonArray>();
        for (const auto & kv : blinds) {
            blinds_array.add(kv.first);
        }

        auto groups_array = json["groups"].to<JsonArray>();
        for (const auto & kv : groups) {
            groups_array.add(kv.first);
        }

        String output;
        serializeJson(json, output);
        server.send(200, F("application/json"), output);
    });

    server.on("/reset", []{
            reset_remote();
            server.send(200, F("text/plain"), F("OK"));
            });

    server.serveStatic("/", LittleFS, "/");
}

void setup() {
    Serial.begin(115200);

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
        ESP.restart();
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
