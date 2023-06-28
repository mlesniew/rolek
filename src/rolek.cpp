#include <map>

#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>
#include <LittleFS.h>

#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <PicoUtils.h>
#include <PicoMQTT.h>

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_ST D7
#define PIN_EN D2
#define PIN_LED D4

#define DEFAULT_INDEX 1

String hostname;
String hass_autodiscovery_topic;
PicoMQTT::Client mqtt;

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

PicoUtils::PinInput<0, true> flash_button;
PicoUtils::PinOutput<D4, true> wifi_led;
PicoUtils::Blink led_blinker(wifi_led, 0, 91);
PicoUtils::RestfulServer<ESP8266WebServer> server;

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

void init_output(unsigned int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void navigate_to(unsigned int index) {
    printf("Going from index %u to index %u\n", current_index, index);

    while (current_index != index) {
        if (current_index < index) {
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

        String name = server.decodedPathArg(0);
        const char direction = server.decodedPathArg(1).c_str()[0];

        if (name.length()) {
            // name must start with slash if present
            if (name[0] != '/') {
                server.send(404);
                return;
            }
            name = name.substring(1);
        }

        const bool success = process(name.c_str(), command_t(direction));
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

        server.sendJson(json);
    });

    server.on("/reset", [] {
        reset_remote();
        server.send(200, F("text/plain"), F("OK"));
    });

    server.serveStatic("/", LittleFS, "/ui/");
}

namespace network_config {

const char CONFIG_PATH[] PROGMEM = "/network.json";

void load() {
    PicoUtils::JsonConfigFile<StaticJsonDocument<256>> config(LittleFS, FPSTR(CONFIG_PATH));
    hostname = config["hostname"] | "rolek";
    hass_autodiscovery_topic = config["hass_autodiscovery_topic"] | "homeassistant";
    mqtt.host = config["mqtt"]["host"] | "";
    mqtt.port = config["mqtt"]["port"] | 1883;
    mqtt.username = config["mqtt"]["username"] | "";
    mqtt.password = config["mqtt"]["password"] | "";
}

DynamicJsonDocument get() {
    DynamicJsonDocument config(1024);
    config["hostname"] = hostname;
    config["hass_autodiscovery_topic"] = hass_autodiscovery_topic;
    config["mqtt"]["host"] = mqtt.host;
    config["mqtt"]["port"] = mqtt.port;
    config["mqtt"]["username"] = mqtt.username;
    config["mqtt"]["password"] = mqtt.password;
    return config;
}

void save() {
    auto file = LittleFS.open(FPSTR(CONFIG_PATH), "w");
    if (file) {
        serializeJson(get(), file);
        file.close();
    }
}

}

void config_mode() {
    led_blinker.set_pattern(0b100100100 << 9);

    WiFiManagerParameter param_hostname("hostname", "Hostname", hostname.c_str(), 64);
    WiFiManagerParameter param_mqtt_server("mqtt_server", "MQTT Server", mqtt.host.c_str(), 64);
    WiFiManagerParameter param_mqtt_port("mqtt_port", "MQTT Port", String(mqtt.port).c_str(), 64);
    WiFiManagerParameter param_mqtt_username("mqtt_user", "MQTT Username", mqtt.username.c_str(), 64);
    WiFiManagerParameter param_mqtt_password("mqtt_pass", "MQTT Password", mqtt.password.c_str(), 64);
    WiFiManagerParameter param_hass_topic("hass_autodiscovery_topic", "Home Assistant autodiscovery topic",
                                          hass_autodiscovery_topic.c_str(), 64);

    WiFiManager wifi_manager;

    wifi_manager.addParameter(&param_hostname);
    wifi_manager.addParameter(&param_mqtt_server);
    wifi_manager.addParameter(&param_mqtt_port);
    wifi_manager.addParameter(&param_mqtt_username);
    wifi_manager.addParameter(&param_mqtt_password);
    wifi_manager.addParameter(&param_hass_topic);

    wifi_manager.startConfigPortal("Rolek");

    hostname = param_hostname.getValue();
    mqtt.host = param_mqtt_server.getValue();
    mqtt.port = String(param_mqtt_port.getValue()).toInt();
    mqtt.username = param_mqtt_username.getValue();
    mqtt.password = param_mqtt_password.getValue();
    hass_autodiscovery_topic = param_hass_topic.getValue();

    network_config::save();
}

void setup() {
    wifi_led.init();
    led_blinker.set_pattern(0b10);
    PicoUtils::BackgroundBlinker bb(led_blinker);
    flash_button.init();

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
                     "Rolek " __DATE__ " " __TIME__ "\n"
                     "https://github.com/mlesniew/rolek\n"
                     "\n"));

    Serial.println(F("Initializing outputs..."));
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(PIN_ST);

    Serial.println(F("Initializing file system..."));
    LittleFS.begin();

    Serial.println(F("Load configuration..."));
    network_config::load();
    serializeJsonPretty(network_config::get(), Serial);

    Serial.println("\nPress and hold button now to enter WiFi setup.");
    delay(3000);
    if (flash_button) {
        config_mode();
    }

    WiFi.hostname(hostname);
    WiFi.setAutoReconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.begin();
    MDNS.begin(hostname);

    Serial.println(F("Setting up endpoints..."));
    setup_endpoints();

    // do an initial remote reset
    reset_remote();

    Serial.println(F("Starting up server..."));
    server.begin();

    Serial.println(F("Starting up MQTT..."));
    mqtt.subscribe("rolek/" + hostname + "/+", [](const char * topic, const char * payload) {
        const String name = mqtt.get_topic_element(topic, 2);

        if (strcmp(payload, "STOP") == 0) {
            process(name.c_str(), COMMAND_STOP);
        } else if (strcmp(payload, "OPEN") == 0) {
            process(name.c_str(), COMMAND_UP);
        } else if (strcmp(payload, "CLOSE") == 0) {
            process(name.c_str(), COMMAND_DOWN);
        }

    });

    mqtt.subscribe("rolek/" + hostname, [](const char * payload) {
        if (strcmp(payload, "reset") == 0) {
            reset_remote();
        }
    });

    mqtt.begin();

    Serial.println(F("Setup complete."));
}

PicoUtils::PeriodicRun hass_autodiscovery(300, 30, [] {
    if (hass_autodiscovery_topic.length() == 0) {
        return;
    }

    Serial.println("Home Assistant autodiscovery announcement...");

    const String mac = "rolek-" + String(ESP.getChipId(), HEX);

    for (const auto & kv : blinds) {
        const auto device_id = mac + "-" + String(kv.second);
        const auto unique_id = device_id;
        const auto name = kv.first;
        const String topic = hass_autodiscovery_topic + "/cover/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["command_topic"] = "rolek/" + hostname + "/" + kv.first.c_str();
        json["device_class"] = "shutter";  // TODO: are these shutters, shades or blinds?
        json["name"] = "Roleta";

        auto device = json["device"];
        device["name"] = "Roleta " + name;
        device["suggested_area"] = name.substr(0, name.find(' '));
        device["identifiers"][0] = device_id;
        device["via_device"] = mac;

        serializeJsonPretty(json, Serial);

        auto publish = mqtt.begin_publish(topic, measureJson(json));
        serializeJson(json, publish);
        publish.send();
    }

    {
        const String topic = hass_autodiscovery_topic + "/button/" + mac + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = mac;
        json["command_topic"] = "rolek/" + hostname;
        json["name"] = "Reset";
        json["payload_press"] = "reset";

        auto device = json["device"];
        device["name"] = "Rolek";
        device["manufacturer"] = "mlesniew";
        device["sw_version"] = __DATE__ " " __TIME__;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
        device["identifiers"][0] = mac;

        serializeJsonPretty(json, Serial);

        auto publish = mqtt.begin_publish(topic, measureJson(json));
        serializeJson(json, publish);
        publish.send();
    }

    Serial.println("Home Assistant autodiscovery announcement complete.");
});

void update_status_led() {
    if (WiFi.status() == WL_CONNECTED) {
        if (mqtt.connected()) {
            led_blinker.set_pattern(uint64_t(0b101) << 60);
        } else {
            led_blinker.set_pattern(uint64_t(0b1) << 60);
        }
    } else {
        led_blinker.set_pattern(0b1100);
    }
    led_blinker.tick();
};

void loop() {
    update_status_led();
    server.handleClient();
    mqtt.loop();
    MDNS.update();
    hass_autodiscovery.tick();
}
