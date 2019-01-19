#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

void setup_static_endpoints(ESP8266WebServer & server);

#define PIN_UP D6
#define PIN_DN D5
#define PIN_LT D1
#define PIN_RT D2
#define PIN_EN D0

#define DEFAULT_INDEX    1
#define TIME_POWER_ON        500
#define TIME_POWER_OFF       100


ESP8266WebServer server(80);
bool remote_powered = false;
unsigned int current_index = DEFAULT_INDEX;
unsigned long standby_timeout = 0;


void power(bool on) {
    if (remote_powered == on)
        return;

    digitalWrite(PIN_EN, on ? HIGH : LOW);
    delay(on ? TIME_POWER_ON : TIME_POWER_OFF);

    remote_powered = on;
}

void navigate_to(unsigned int index)
{
    Serial.println("Selecting index " + String(index));
    while (current_index != index)
    {
        unsigned int pin;
        if (current_index < index)
        {
            pin = PIN_RT;
            current_index++;
        } else {
            pin = PIN_LT;
            current_index--;
        }

        digitalWrite(pin, HIGH);
        delay(50);
        digitalWrite(pin, LOW);
        delay(100);
    }
}

struct EndpointDebugger
{
    EndpointDebugger() : start_time(millis()) {
        Serial.println("Handling endpoint " + server.uri());
        server.sendHeader("Server", "Rolek");
    }

    ~EndpointDebugger() {
        Serial.println("Finished handling endpoint " + server.uri());
        unsigned long elapsed = millis() - start_time;
        Serial.println("Handler took " + String(elapsed) + " ms");
    }
    const unsigned long start_time;
};

void setup_wifi()
{
    WiFiManager wifiManager;

    wifiManager.setConfigPortalTimeout(60);
    if (!wifiManager.autoConnect("voodoo", "password"))
    {
        Serial.println("AutoConnect failed, retrying in 15 minutes");
        delay(15 * 60 * 1000);
        while (true)
        {
            ESP.reset();
            delay(10 * 1000);
        }
    }

    // if we ever get disconnected, reset and try again
    WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected&) {
        Serial.println("WiFi disconnected.  Rebooting...");
        while (true)
        {
            delay(3 * 1000);
            ESP.reset();
            delay(10 * 1000);
        }
    });
}

void init_output(unsigned int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void setup_endpoints()
{
    setup_static_endpoints(server);

    auto handler = [](bool up){
        EndpointDebugger d;
        unsigned int mask = 0;

        /* extract m parameter */
        if (!server.hasArg("mask"))
            mask = 1;
        else {
            mask = server.arg("mask").toInt();
            if (!mask)
            {
                server.send(400, "text/plain", "Malformed mask");
                return;
            }
            mask <<= 1;
        }

        power(true);

        Serial.println("Iterating through mask...");

        for(unsigned int idx = 0; mask && (idx < 8); ++idx)
        {
            unsigned int current = 1 << idx;
            if (mask & current)
            {
                navigate_to(idx);
                const unsigned int pin = up ? PIN_UP : PIN_DN;

                digitalWrite(pin, HIGH);
                delay(250);
                digitalWrite(pin, LOW);
                delay(100);

                mask ^= current;
            }
        }

        standby_timeout = millis() + 30 * 1000;
        server.send(200, "text/plain", "OK");
    };

    server.on("/up", [handler]{ handler(true); });
    server.on("/down", [handler]{ handler(false); });
}

void setup() {
    Serial.begin(9600);

    // init outputs
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(LED_BUILTIN);

    setup_wifi();

    Serial.println("Setting up endpoints...");
    setup_endpoints();

    Serial.println("Starting up server...");
    server.begin();
    Serial.println("Setup complete");
}

void loop() {
    if (remote_powered && (standby_timeout <= millis()))
    {
        Serial.println("Standby timeout elapsed.");
        power(false);
        current_index = DEFAULT_INDEX;
    }

    server.handleClient();
}
