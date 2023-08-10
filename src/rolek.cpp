#include <map>

#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>
#include <LittleFS.h>

#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <PicoUtils.h>
#include <PicoMQTT.h>

#include "remote.h"
#include "shutter.h"

const String board_id(ESP.getChipId(), HEX);
String hostname;
String hass_autodiscovery_topic;
PicoMQTT::Client mqtt;

Remote remote;

#if __has_include("customize.h")
#include "customize.h"
#else
#warning "Using default config, create customize.h to customize."

std::map<std::string, Shutter> blinds = {
    {"Living room", 1},
    {"Kitchen", 2},
    {"Bedroom", 3},
    {"Bathroom", 4},
};

const std::map<std::string, std::vector<std::string>> groups = {
    {"Downstairs", {"Living room", "Kitchen"}},
    {"Upstairs", {"Bedroom", "Bathroom"}},
};

#endif

PicoUtils::PinInput<0, true> flash_button;
PicoUtils::PinOutput<D4, true> wifi_led;
PicoUtils::Blink led_blinker(wifi_led, 0, 91);
PicoUtils::RestfulServer<ESP8266WebServer> server;

bool process(const std::string & name, const command_t command) {
    if (name.empty()) {
        remote.execute(0, command);
        return true;
    }

    {
        const auto it = blinds.find(name);
        if (it != blinds.end()) {
            it->second.execute(command);
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
        remote.reset();
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

    remote.init();

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

    Serial.println(F("Starting up server..."));
    server.begin();

    Serial.println(F("Starting up MQTT..."));
    mqtt.subscribe("rolek/" + board_id + "/+", [](const char * topic, const char * payload) {
        const String name = mqtt.get_topic_element(topic, 2);

        if (strcmp(payload, "STOP") == 0) {
            process(name.c_str(), COMMAND_STOP);
        } else if (strcmp(payload, "OPEN") == 0) {
            process(name.c_str(), COMMAND_UP);
        } else if (strcmp(payload, "CLOSE") == 0) {
            process(name.c_str(), COMMAND_DOWN);
        }

    });

    mqtt.subscribe("rolek/" + board_id, [](const char * payload) {
        if (strcmp(payload, "reset") == 0) {
            remote.reset();
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

    const String board_unique_id = "rolek-" + board_id;

    for (const auto & kv : blinds) {
        const auto unique_id = board_unique_id + "-" + String(kv.second.index);
        const auto name = kv.first;
        const String topic = hass_autodiscovery_topic + "/cover/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["command_topic"] = "rolek/" + board_id + "/" + kv.first.c_str();
        json["device_class"] = "shutter";  // TODO: are these shutters, shades or blinds?
        json["name"] = "Roleta " + name;

        auto device = json["device"];
        device["name"] = "Roleta " + name;
        device["suggested_area"] = name.substr(0, name.find(' '));
        device["identifiers"][0] = unique_id;
        device["via_device"] = board_unique_id;

        serializeJsonPretty(json, Serial);

        auto publish = mqtt.begin_publish(topic, measureJson(json));
        serializeJson(json, publish);
        publish.send();
    }

    {
        const String unique_id = board_unique_id + "-remote-reset";
        const String topic = hass_autodiscovery_topic + "/button/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["command_topic"] = "rolek/" + board_id;
        json["name"] = "Reset";
        json["payload_press"] = "reset";

        auto device = json["device"];
        device["name"] = "Rolek";
        device["manufacturer"] = "mlesniew";
        device["sw_version"] = __DATE__ " " __TIME__;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
        device["identifiers"][0] = board_unique_id;

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
