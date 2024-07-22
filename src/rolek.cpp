#include <map>

#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>
#include <LittleFS.h>

#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PicoUtils.h>
#include <PicoMQTT.h>
#include <PicoSyslog.h>

#include "remote.h"
#include "shutter.h"
#include "hass.h"

String hostname;
String hass_autodiscovery_topic;
String password;

PicoMQTT::Client mqtt;
PicoSyslog::Logger syslog("rolek");

Remote remote;

std::map<String, Shutter> shutters;
std::map<String, std::vector<String>> groups;

PicoUtils::PinInput flash_button(0, true);
PicoUtils::PinOutput wifi_led(D4, true);
PicoUtils::WiFiControlSmartConfig wifi_control(wifi_led);

PicoUtils::RestfulServer<ESP8266WebServer> server;

bool process(const String & name, const command_t command) {
    if (name.isEmpty()) {
        remote.execute(0, command);

        // notify individual shutters manually about the globally executed command
        for (auto & kv : shutters) { kv.second.on_execute(command); }

        return true;
    }

    {
        const auto it = shutters.find(name);
        if (it != shutters.end()) {
            it->second.process(command);
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

bool set_position(const String & name, const double position) {
    if (name.isEmpty()) {
        for (auto & kv : shutters) { kv.second.set_position(position); }
        return true;
    }

    {
        const auto it = shutters.find(name);
        if (it != shutters.end()) {
            it->second.set_position(position);
            return true;
        }
    }

    {
        const auto it = groups.find(name);
        if (it != groups.end()) {
            const auto & group = it->second;
            for (const auto & element : group) {
                set_position(element, position);
            }
            return true;
        }
    }

    return false;
}

void setup_endpoints() {
    server.on(UriRegex("/shutters(.*)/(up|down|stop)"), HTTP_POST, [] {

        syslog.printf("POST %s\n", server.uri().c_str());

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

    server.on(UriRegex("/shutters(.*)/set/([0-9]+)"), HTTP_POST, [] {
        syslog.printf("POST %s\n", server.uri().c_str());

        String name = server.decodedPathArg(0);
        double position = server.decodedPathArg(1).toInt();

        if (name.length()) {
            // name must start with slash if present
            if (name[0] != '/') {
                server.send(404);
                return;
            }
            name = name.substring(1);
        }

        const bool success = set_position(name.c_str(), position);
        server.send(success ? 200 : 404);
    });

    server.on("/shutters", [] {
        JsonDocument json;
        auto blinds_array = json["shutters"].to<JsonArray>();
        for (const auto & kv : shutters) {
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

void setup_shutters() {
    PicoUtils::JsonConfigFile<JsonDocument> config(LittleFS, "/shutters.json");

    for (const auto & kv : config.as<JsonObjectConst>()) {
        const String key = kv.key().c_str();
        if (kv.value().is<unsigned int>()) {
            const unsigned int index = kv.value().as<unsigned int>();
            if (index) {
                shutters.emplace(key, index);
            }
        }

        if (kv.value().is<JsonObjectConst>()) {
            const JsonObjectConst obj = kv.value().as<JsonObjectConst>();
            const unsigned int index = obj["index"] | 0;
            const double open_time = obj["open_time"] | obj["time"] | 30;
            const double close_time = obj["close_time"] | obj["time"] | 30;
            if (index) {
                shutters.try_emplace(key, index, 1000 * open_time, 1000 * close_time);
            }
        }

        if (kv.value().is<JsonArrayConst>()) {
            auto & group = groups[key];
            for (const auto & element : kv.value().as<JsonArrayConst>()) {
                group.push_back(element.as<String>());
            }
        }
    }
}

void setup() {
    wifi_led.init();
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

    wifi_control.get_connectivity_level = [] {
        return mqtt.connected() ? 2 : 1;
    };

    Serial.println(F("Initializing file system..."));
    LittleFS.begin();
    {
        Serial.println(F("Load network configuration..."));
        PicoUtils::JsonConfigFile<JsonDocument> config(LittleFS, F("/network.json"));
        hostname = config["hostname"] | "rolek";
        hass_autodiscovery_topic = config["hass_autodiscovery_topic"] | "homeassistant";
        mqtt.host = config["mqtt"]["host"] | "";
        mqtt.port = config["mqtt"]["port"] | 1883;
        mqtt.username = config["mqtt"]["username"] | "mqtt";
        mqtt.password = config["mqtt"]["password"] | "mosquitto";
        password = config["password"] | "";
        syslog.server = config["syslog"] | "";
    }

    WiFi.hostname(hostname);
    wifi_control.init(flash_button);

    setup_shutters();

    Serial.println(F("Setting up endpoints..."));
    setup_endpoints();

    Serial.println(F("Starting up server..."));
    server.begin();

    Serial.println(F("Starting up MQTT..."));

    HomeAssistant::init();

    mqtt.client_id = "rolek-" + String(ESP.getChipId(), HEX);
    mqtt.begin();

    Serial.println(F("Starting up ArduinoOTA..."));
    ArduinoOTA.setHostname(hostname.c_str());
    if (password.length()) {
        ArduinoOTA.setPassword(password.c_str());
    }
    ArduinoOTA.begin();

    Serial.println(F("Setup complete."));
}

void loop() {
    ArduinoOTA.handle();
    for (auto & kv : shutters) {
        kv.second.tick();
    }
    server.handleClient();
    mqtt.loop();
    HomeAssistant::tick();
    wifi_control.tick();
}
