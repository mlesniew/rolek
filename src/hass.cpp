#include <Arduino.h>

#include <map>

#include <ArduinoJson.h>
#include <PicoMQTT.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>

#include "hass.h"
#include "shutter.h"

extern PicoMQTT::Client mqtt;
extern PicoSyslog::Logger syslog;
extern String hass_autodiscovery_topic;
extern std::map<String, Shutter> shutters;
extern String hostname;

namespace {

const String board_id(ESP.getChipId(), HEX);

std::list<PicoUtils::Watch<double>> position_watches;
std::list<PicoUtils::Watch<command_t>> state_watches;

String get_first_word(const String & s) {
    auto space_idx = s.indexOf(' ');
    return space_idx <= 0 ? s : s.substring(0, space_idx);
}

}

namespace HomeAssistant {

void notify_state(const Shutter & shutter) {
    const auto index = shutter.index;
    const auto command = shutter.get_state();
    const auto topic = "rolek/" + board_id + "/" + String(index) + "/state";
    switch (command) {
        case COMMAND_STOP: {
            const auto position = shutter.get_position();
            if (position <= 0) {
                mqtt.publish(topic, "closed", 0, true);
            } else if (position >= 100) {
                mqtt.publish(topic, "open", 0, true);
            } else {
                mqtt.publish(topic, "stopped", 0, true);
            }
        }
        break;
        case COMMAND_UP:
            mqtt.publish(topic, "opening", 0, true);
            break;
        case COMMAND_DOWN:
            mqtt.publish(topic, "closing", 0, true);
            break;
    }
}

void notify_position(const Shutter & shutter) {
    const auto index = shutter.index;
    const auto position = shutter.get_position();
    mqtt.publish(
        "rolek/" + board_id + "/" + String(index) + "/position",
        String(std::isnan(position) ? 50 : position), 0, true);
}

void autodiscover() {
    if (hass_autodiscovery_topic.length() == 0) {
        syslog.println("Home Assistant autodiscovery disabled.");
        return;
    }

    syslog.println("Home Assistant autodiscovery messages...");

    const String board_unique_id = "rolek_" + board_id;
    String friendly_hostname = hostname;
    hostname[0] = hostname[0] ^ ' ';

    for (const auto & kv : shutters) {
        const auto unique_id = board_unique_id + "_" + String(kv.second.index);
        const auto name = kv.first;
        const String topic = hass_autodiscovery_topic + "/cover/" + unique_id + "/config";

        JsonDocument json;
        json["unique_id"] = unique_id;
        json["name"] = friendly_hostname + " " + name;
        json["object_id"] = hostname + "_" + name;
        json["command_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/command";
        json["state_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/state";
        json["position_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/position";
        json["availability_topic"] = "rolek/" + board_id + "/availability";
        json["device_class"] = "shutter";

        auto device = json["device"];
        device["name"] = "Rolek controller " + name;
        device["suggested_area"] = get_first_word(name);
        device["identifiers"][0] = unique_id;
        device["via_device"] = board_unique_id;

        auto publish = mqtt.begin_publish(topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }

    struct Button {
        const char * name;
        const char * friendly_name;
        const char * payload;
        const char * icon;
    };

    static const Button buttons[] = {
        {"reset_remote", "Reset remote", "RESET", "mdi:lightning-bolt" },
        {"up", "Open all shutters", "UP", "mdi:arrow-up-bold" },
        {"down", "Close all shutters", "DOWN", "mdi:arrow-down-bold" },
        {"stop", "Stop all shutters", "STOP", "mdi:stop"},
    };

    for (const auto & button : buttons) {
        const String unique_id = board_unique_id + "_" + button.name;
        const String topic = hass_autodiscovery_topic + "/button/" + unique_id + "/config";

        JsonDocument json;
        json["unique_id"] = unique_id;
        json["object_id"] = hostname + "_" + button.name;
        json["command_topic"] = "rolek/" + board_id + "/command";
        json["availability_topic"] = "rolek/" + board_id + "/availability";
        json["name"] = friendly_hostname + " " + button.friendly_name;
        json["payload_press"] = button.payload;
        json["icon"] = button.icon;

        auto device = json["device"];
        device["name"] = hostname;
        device["manufacturer"] = "mlesniew";
        device["sw_version"] = __DATE__ " " __TIME__;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
        device["identifiers"][0] = board_unique_id;

        auto publish = mqtt.begin_publish(topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }

    syslog.println("Home Assistant autodiscovery announcement complete.");
}

void init() {
    mqtt.subscribe("rolek/" + board_id + "/+/command", [](const char * topic, const char * payload) {

        command_t command;
        if (strcmp(payload, "STOP") == 0) {
            command = COMMAND_STOP;
        } else if (strcmp(payload, "OPEN") == 0) {
            command = COMMAND_UP;
        } else if (strcmp(payload, "CLOSE") == 0) {
            command = COMMAND_DOWN;
        } else {
            // invalid command
            return;
        }

        const auto index = mqtt.get_topic_element(topic, 2).toInt();

        for (auto & kv : shutters) {
            if (long(kv.second.index) == index) {
                kv.second.execute(command);
            }
        }
    });

    mqtt.subscribe("rolek/" + board_id + "/command", [](const char * payload) {
        if (strcmp(payload, "RESET") == 0) {
            remote.reset();
        }
    });

    mqtt.will.topic = "rolek/" + board_id + "/availability";
    mqtt.will.payload = "offline";
    mqtt.will.retain = true;

    mqtt.connected_callback = [] {
        // send autodiscovery messages
        autodiscover();

        // notify about the state of shutters
        for (auto & watch : position_watches) { watch.fire(); }
        for (auto & watch : state_watches) { watch.fire(); }

        // notify about availability
        mqtt.publish(mqtt.will.topic, "online", 0, true);
    };

    for (const auto & kv : shutters) {
        const auto & shutter = kv.second;
        position_watches.push_back(PicoUtils::Watch<double>(
        [&shutter] {
            const auto position = shutter.get_position();
            return std::isnan(position) ? 50 : position;
        },
        [&shutter] { notify_position(shutter); }));
        state_watches.push_back(PicoUtils::Watch<command_t>([&shutter] { return shutter.get_state(); },
                                [&shutter] { notify_state(shutter); }));
    }
}

void tick() {
    for (auto & watch : position_watches) { watch.tick(); }
    for (auto & watch : state_watches) { watch.tick(); }
}

}
