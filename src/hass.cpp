#include "hass.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PicoHA.h>
#include <PicoMQTT.h>
#include <PicoSlugify.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>

#include <map>

#include "shutter.h"

extern PicoMQTT::Client mqtt;
extern PicoSyslog::Logger syslog;
extern std::map<String, Shutter> shutters;
extern String hostname;

bool process(const String & name, const command_t command);
bool sync();

namespace {

String get_first_word(const String & s) {
    auto space_idx = s.indexOf(' ');
    return space_idx <= 0 ? s : s.substring(0, space_idx);
}

}  // namespace

namespace HomeAssistant {

PicoHA::Device device(mqtt, "Rolek", "mlesniew", "Rolek");

void init() {
    device.name = hostname;

    PicoHA::add_diagnostic_entities(device);

    auto & reset_remote_button =
        device.addButton(F("reset_remote"), F("Reset remote"));
    reset_remote_button.icon = F("lightning-bolt");
    reset_remote_button.on_press = [] { remote.reset(); };

    auto & up_button = device.addButton(F("up"), F("Open all shutters"));
    up_button.icon = F("arrow-up-bold");
    up_button.on_press = [] { process("", COMMAND_UP); };

    auto & down_button = device.addButton(F("down"), F("Close all shutters"));
    down_button.icon = F("arrow-down-bold");
    down_button.on_press = [] { process("", COMMAND_DOWN); };

    auto & stop_button = device.addButton(F("stop"), F("Stop all shutters"));
    stop_button.icon = F("stop");
    stop_button.on_press = [] { process("", COMMAND_STOP); };

    auto & sync_position_button =
        device.addButton(F("sync_position"), F("Sync position"));
    sync_position_button.icon = F("cog-sync");
    sync_position_button.on_press = [] { sync(); };

    for (auto & kv : shutters) {
        auto & shutter_device = device.addChildDevice(
            String(kv.second.index), F("Rolek ") + kv.first, F("mlesniew"),
            F("Rolek Shutter"), get_first_word(kv.first));

        auto & cover = shutter_device.addCover("shutter", nullptr);
        cover.device_class = F("shutter");

        cover.position_getter = PicoCallback<int>(
            [](const Shutter * shutter) {
                const auto position = shutter->get_position();
                return std::isnan(position) ? 50 : int(position);
            },
            &kv.second);

        cover.position_setter = PicoCallback<void, int>(
            [](Shutter * shutter, int position) {
                shutter->set_position(position);
            },
            &kv.second);

        cover.state_getter = PicoCallback<PicoHA::Cover::State>(
            [](const Shutter * shutter) {
                switch (shutter->get_state()) {
                    case COMMAND_UP:
                        return PicoHA::Cover::State::opening;
                    case COMMAND_DOWN:
                        return PicoHA::Cover::State::closing;
                    case COMMAND_STOP:
                    default:
                        if (std::isnan(shutter->get_position())) {
                            return PicoHA::Cover::State::stopped;
                        } else if (shutter->get_position() <= 10) {
                            return PicoHA::Cover::State::closed;
                        } else {
                            return PicoHA::Cover::State::open;
                        }
                }
            },
            &kv.second);

        cover.command_setter = PicoCallback<void, PicoHA::Cover::Command>(
            [](Shutter * shutter, PicoHA::Cover::Command command) {
                switch (command) {
                    case PicoHA::Cover::Command::open:
                        shutter->process(COMMAND_UP);
                        break;
                    case PicoHA::Cover::Command::close:
                        shutter->process(COMMAND_DOWN);
                        break;
                    case PicoHA::Cover::Command::stop:
                        shutter->process(COMMAND_STOP);
                        break;
                }
            },
            &kv.second);
    }

    device.begin();
}

void tick() { device.loop(); }

}  // namespace HomeAssistant
