#include <Arduino.h>

#include <map>

#include <ArduinoJson.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>

#include "hass.h"
#include "shutter.h"

extern PicoMQTT::Client mqtt;
extern PicoSyslog::Logger syslog;
extern String hass_autodiscovery_topic;
extern std::map<std::string, Shutter> blinds;

namespace {

const String board_id(ESP.getChipId(), HEX);

std::list<PicoUtils::Watch<double>> position_watches;
std::list<PicoUtils::Watch<command_t>> state_watches;

}

namespace HomeAssistant {

void notify_state(const Shutter & shutter) {
    const auto index = shutter.index;
    const auto command = shutter.get_state();
    const auto topic = "rolek/" + board_id + "/" + String(index) + "/state";
    switch (command) {
        case COMMAND_STOP:
            {
                const auto position = shutter.get_position();
                if (position <= 0)
                    mqtt.publish(topic, "closed", 0, true);
                else if (position >= 100)
                    mqtt.publish(topic, "open", 0, true);
                else
                    mqtt.publish(topic, "stopped", 0, true);
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

    const String board_unique_id = "rolek-" + board_id;


    for (const auto & kv : blinds) {
        const auto unique_id = board_unique_id + "-" + String(kv.second.index);
        const auto name = kv.first;
        const String topic = hass_autodiscovery_topic + "/cover/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["command_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/command";
        json["state_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/state";
        json["position_topic"] = "rolek/" + board_id + "/" + String(kv.second.index) + "/position";
        json["availability_topic"] = "rolek/" + board_id + "/availability";
        json["device_class"] = "shutter";  // TODO: are these shutters, shades or blinds?
        json["name"] = "Roleta " + name;

        auto device = json["device"];
        device["name"] = "Roleta " + name;
        device["suggested_area"] = name.substr(0, name.find(' '));
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
        {"reset-remote", "Reset remote", "RESET", "mdi:lightning-bolt" },
        {"up", "Open all shutters", "UP", "mdi:arrow-up-bold" },
        {"down", "Close all shutters", "DOWN", "mdi:arrow-down-bold" },
        {"stop", "Stop all shutters", "STOP", "mdi:stop"},
    };

    for (const auto & button : buttons) {
        const String unique_id = board_unique_id + "-" + button.name;
        const String topic = hass_autodiscovery_topic + "/button/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["object_id"] = String("rolek-") + button.name;
        json["command_topic"] = "rolek/" + board_id + "/command";
        json["availability_topic"] = "rolek/" + board_id + "/availability";
        json["name"] = button.friendly_name;
        json["payload_press"] = button.payload;
        json["icon"] = button.icon;

        auto device = json["device"];
        device["name"] = "Rolek";
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

        for (auto & kv : blinds) {
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

        // notify about the state of blinds
        for (const auto & watch: position_watches) watch.fire();
        for (const auto & watch: state_watches) watch.fire();

        // notify about availability
        mqtt.publish(mqtt.will.topic, "online", 0, true);
    };

    for (const auto & kv : blinds) {
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
    for (const auto & watch: position_watches) watch.tick();
    for (const auto & watch: state_watches) watch.tick();
}

}
