#pragma once

#include <PicoMQTT.h>

#include "remote.h"

namespace HomeAssistant {

void init(PicoMQTT::Client & mqtt);
void autodiscover(const String & hass_autodiscovery_topic);
void notify_state(unsigned int index, command_t command);
void notify_position(unsigned int index, double position);

}
