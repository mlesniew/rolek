#include <Arduino.h>

#include <ArduinoJson.h>

#include "hass.h"
#include "shutter.h"

namespace {

PicoMQTT::Client * mqtt;
const String board_id(ESP.getChipId(), HEX);

}

namespace HomeAssistant {

void init(PicoMQTT::Client & mqtt_, const String & hass_autodiscovery_topic) {
    mqtt = &mqtt_;

    mqtt->subscribe("rolek/" + board_id + "/+/command", [](const char * topic, const char * payload) {

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

        const auto index = mqtt->get_topic_element(topic, 2).toInt();

        for (auto & kv : blinds) {
            if (long(kv.second.index) == index) {
                kv.second.execute(command);
            }
        }

    });

    mqtt->subscribe("rolek/" + board_id + "/command", [](const char * payload) {
        if (strcmp(payload, "RESET") == 0) {
            remote.reset();
        }
    });

    mqtt->will.topic = "rolek/" + board_id + "/availability";
    mqtt->will.payload = "offline";
    mqtt->will.retain = true;

    mqtt->connected_callback = [&hass_autodiscovery_topic] {
        // send autodiscovery messages
        autodiscover(hass_autodiscovery_topic);

        // notify about the state of blinds
        for (const auto & kv : blinds) {
            notify_state(kv.second.index, kv.second.get_state());
            notify_position(kv.second.index, kv.second.get_position());
        }

        // notify about availability
        mqtt->publish(mqtt->will.topic, "online", 0, true);
    };
}

void autodiscover(const String & hass_autodiscovery_topic) {
    if (!mqtt || (hass_autodiscovery_topic.length() == 0)) {
        Serial.println("Home Assistant autodiscovery disabled.");
        return;
    }

    Serial.println("Home Assistant autodiscovery messages...");

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

        auto publish = mqtt->begin_publish(topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }

    {
        const String unique_id = board_unique_id + "-remote-reset";
        const String topic = hass_autodiscovery_topic + "/button/" + unique_id + "/config";

        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["command_topic"] = "rolek/" + board_id + "/command";
        json["availability_topic"] = "rolek/" + board_id + "/availability";
        json["name"] = "Reset";
        json["payload_press"] = "RESET";

        auto device = json["device"];
        device["name"] = "Rolek";
        device["manufacturer"] = "mlesniew";
        device["sw_version"] = __DATE__ " " __TIME__;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
        device["identifiers"][0] = board_unique_id;

        auto publish = mqtt->begin_publish(topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }

    Serial.println("Home Assistant autodiscovery announcement complete.");
}

void notify_state(unsigned int index, command_t command) {
    if (mqtt) {
        const auto topic = "rolek/" + board_id + "/" + String(index) + "/state";
        switch (command) {
            case COMMAND_STOP:
                mqtt->publish(topic, "stopped", 0, true);
                break;
            case COMMAND_UP:
                mqtt->publish(topic, "opening", 0, true);
                break;
            case COMMAND_DOWN:
                mqtt->publish(topic, "closing", 0, true);
                break;
        }
    }
}

void notify_position(unsigned int index, double position) {
    if (mqtt && !std::isnan(position)) {
        mqtt->publish("rolek/" + board_id + "/" + String(index) + "/position", String(position),
                      0, true);
    }
}

}
