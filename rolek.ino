#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

void setup_static_endpoints(
        ESP8266WebServer & server,
        std::function<void(void)> before_fn = nullptr,
        std::function<void(void)> after_fn = nullptr);

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_EN D2
#define PIN_LED D4

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
    Serial.println(
            "current index " + String(current_index) +
            ", going to " + String(index));

    while (current_index != index)
    {
        unsigned int pin;
        if (current_index < index)
        {
            pin = PIN_RT;
            current_index++;
            Serial.println("RIGHT");
        } else {
            pin = PIN_LT;
            current_index--;
            Serial.println("LEFT");
        }

        digitalWrite(pin, HIGH);
        delay(100);
        digitalWrite(pin, LOW);
        delay(100);
    }
}

void setup_wifi()
{
    WiFi.hostname("Rolek");

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

unsigned long handler_start;

void before_handler()
{
    digitalWrite(PIN_LED, LOW);
    handler_start = millis();
    Serial.println("Handling endpoint " + server.uri());
    server.sendHeader("Server", "Rolek");
    server.sendHeader("Uptime", uptime());
    server.sendHeader("Remote-Mode", remote_powered ? "Active" : "Sleep");
    if (remote_powered)
        server.sendHeader("Remote-Index", String(current_index));
    // power the remote, even if only a static endpoint was hit, there is a 
    // high chance that the remote will be needed soon
    power(true);
}

void after_handler()
{
    unsigned long elapsed = millis() - handler_start;
    Serial.println("Done handling endpoint " + server.uri() + " -- elapsed time: " + String(elapsed) + " ms");
    digitalWrite(PIN_LED, HIGH);
    // bump standby timer
    standby_timeout = millis() + 30 * 1000;
}

void setup_endpoints()
{
    setup_static_endpoints(server, before_handler, after_handler);

    auto handler = [](bool up){
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

                if (up) {
                    Serial.println("UP");
                } else {
                    Serial.println("DOWN");
                }

                digitalWrite(pin, HIGH);
                delay(250);
                digitalWrite(pin, LOW);
                delay(250);

                mask ^= current;
            }
        }

        server.send(200, "text/plain", "OK");
    };

    server.on("/up", [handler]{
            before_handler();
            handler(true);
            after_handler();
            });
    server.on("/down", [handler]{
            before_handler();
            handler(false);
            after_handler();
            });
}

String uptime()
{
    const auto now = millis();

    const auto seconds = (now / 1000) % 60;
    const auto minutes = (now / (60 * 1000)) % 60;
    const auto hours = (now / (60 * 60 * 1000)) % 24;
    const auto days = now / (24 * 60 * 60 * 1000);

    return String(days) + " days, "
        + String(hours) + ":"
        + (minutes < 10 ? "0" + String(minutes) : String(minutes)) + ":"
        + (seconds < 10 ? "0" + String(seconds) : String(seconds));
}

void setup() {
    Serial.begin(9600);

    // init outputs
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);
    init_output(PIN_LED);

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
