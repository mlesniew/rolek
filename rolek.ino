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


void reset_remote() {
    Serial.println("Resetting remote...");
    digitalWrite(PIN_EN, LOW);
    delay(5000);
    digitalWrite(PIN_EN, HIGH);
    delay(1000);
    current_index = DEFAULT_INDEX;
    Serial.println("Reset complete.");
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
}

void after_handler()
{
    unsigned long elapsed = millis() - handler_start;
    Serial.println("Done handling endpoint " + server.uri() + " -- elapsed time: " + String(elapsed) + " ms");
    digitalWrite(PIN_LED, HIGH);
}

struct HandlerGuard {
    HandlerGuard() { before_handler(); }
    ~HandlerGuard() { after_handler(); }
};

void setup_endpoints()
{
    setup_static_endpoints(
            server,
            before_handler,
            after_handler);

    auto handler = [](bool up){
        HandlerGuard g;

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
            handler(true);
            });

    server.on("/down", [handler]{
            handler(false);
            });

    server.on("/reset", []{
            HandlerGuard g;
            reset_remote();
            server.send(200, "text/plain", "OK");
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

    // do an initial remote reset
    reset_remote();

    setup_wifi();

    Serial.println("Setting up endpoints...");
    setup_endpoints();

    Serial.println("Starting up server...");
    server.begin();
    Serial.println("Setup complete");
}

void loop() {
    {
        // blink the builtin led to indicate that the board is alive
        const auto blink_phase = (millis() >> 10) & 1;
        digitalWrite(PIN_LED, blink_phase ? LOW : HIGH);
    }

    server.handleClient();
}
