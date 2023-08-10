#pragma once

#include <PicoMQTT.h>

namespace HomeAssistant {

void subscribe(PicoMQTT::Client & mqtt);
void autodiscover(PicoMQTT::Client & mqtt, const String & hass_autodiscovery_topic);

}
