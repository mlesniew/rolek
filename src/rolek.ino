#include <ESP8266WebServer.h>
#include <FS.h>
#include <Ticker.h>
#include <WiFiManager.h>

#define PIN_UP D1
#define PIN_DN D0
#define PIN_LT D6
#define PIN_RT D5
#define PIN_EN D2
#define PIN_LED D4

#define DEFAULT_INDEX 1

#define HOSTNAME "Rolek"
#ifndef PASSWORD
#define PASSWORD "password"
#endif

ESP8266WebServer server{80};
unsigned int current_index{DEFAULT_INDEX};

enum direction_t { DIRECTION_DOWN, DIRECTION_UP };
enum button_t { BUTTON_DOWN, BUTTON_UP, BUTTON_LEFT, BUTTON_RIGHT };

void reset_remote() {
    printf("Resetting remote...\n");
    digitalWrite(PIN_EN, LOW);
    delay(5000);
    digitalWrite(PIN_EN, HIGH);
    delay(1000);
    current_index = DEFAULT_INDEX;
    printf("Reset complete.\n");
}

void push(button_t button, unsigned long time) {
    const char * desc;
    unsigned int pin = 0;
    switch (button) {
        case BUTTON_DOWN:
            desc = "DOWN";
            pin = PIN_DN;
            break;
        case BUTTON_UP:
            desc = "UP";
            pin = PIN_DN;
            break;
        case BUTTON_LEFT:
            desc = "LEFT";
            pin = PIN_LT;
            break;
        case BUTTON_RIGHT:
            desc = "RIGHT";
            pin = PIN_RT;
            break;
        default:
            return;
    }

    printf("Pressing %s button (pin %u) for %lu ms.\n", desc, pin, time);

    digitalWrite(pin, HIGH);
    delay(time);
    digitalWrite(pin, LOW);
    delay(time);
}

const char * uptime()
{
    static char buf[100];
    const auto now = millis();

    const unsigned long seconds = (now / 1000) % 60;
    const unsigned long minutes = (now / (60 * 1000)) % 60;
    const unsigned long hours = (now / (60 * 60 * 1000)) % 24;
    const unsigned long days = now / (24 * 60 * 60 * 1000);

    snprintf(buf, 100, "%lu day%s, %lu:%02lu:%02lu", days, days == 1 ? "" : "s", hours, minutes, seconds);
    return buf;
}

void navigate_to(unsigned int index)
{
    printf("Going from index %u to index %u\n", current_index, index);

    while (current_index != index)
    {
        if (current_index < index)
        {
            push(BUTTON_RIGHT, 100);
            current_index++;
        } else {
            push(BUTTON_LEFT, 100);
            current_index--;
        }
    }
}

void reboot()
{
    printf("Reboot...\n");
    while (true)
    {
        ESP.restart();
        delay(10 * 1000);
    }
}

void setup_wifi()
{
    WiFi.hostname(HOSTNAME);
    WiFiManager wifiManager;

    wifiManager.setConfigPortalTimeout(60);
    if (!wifiManager.autoConnect(HOSTNAME, PASSWORD))
    {
        printf("AutoConnect failed, retrying in 15 minutes...\n");
        delay(15 * 60 * 1000);
        reboot();
    }
}

void init_output(unsigned int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void process_command(direction_t direction, unsigned int mask)
{
    printf("Iterating through mask 0x%02x %s selected blinds...\n",
           mask, direction == DIRECTION_UP ? "opening" : "closing");
    for(unsigned int idx = 0; mask && (idx < 8); ++idx)
    {
        unsigned int current = 1 << idx;
        if (mask & current)
        {
            navigate_to(idx);
            push(direction == DIRECTION_UP ? BUTTON_UP : BUTTON_DOWN, 250);
            mask ^= current;
        }
    }
}

void setup_endpoints()
{
    auto handler = [](direction_t direction){
        unsigned int mask = 0;
        unsigned int count = 1;

        /* extract mask parameter */
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

        /* extract count parameter */
        if (!server.hasArg("count"))
            count = 1;
        else {
            count = server.arg("count").toInt();
            if ((count < 1) || (count > 5))
            {
                server.send(400, "text/plain", "Invalid count value");
                return;
            }
        }

        while (true) {
            process_command(direction, mask);
            if (--count == 0)
                break;
            delay(500);
        }

        server.send(200, "text/plain", "OK");
    };

    server.on("/up", [handler]{
            handler(DIRECTION_UP);
            });

    server.on("/down", [handler]{
            handler(DIRECTION_DOWN);
            });

    server.on("/reset", []{
            reset_remote();
            server.send(200, "text/plain", "OK");
            });

    server.on("/alive", []{
            server.send(200, "text/plain", HOSTNAME " is alive");
            });

    server.on("/version", []{
            server.send(200, "text/plain", __DATE__ " " __TIME__);
            });

    server.on("/uptime", []{
            server.send(200, "text/plain", uptime());
            });

    server.serveStatic("/", SPIFFS, "/");
}

void setup() {
    Serial.begin(9600);

    printf("\n\n");
    printf("8888888b.          888          888     \n");
    printf("888   Y88b         888          888     \n");
    printf("888    888         888          888     \n");
    printf("888   d88P .d88b.  888  .d88b.  888  888\n");
    printf("8888888P^ d88^^88b 888 d8P  Y8b 888 .88P\n");
    printf("888 T88b  888  888 888 88888888 888888K \n");
    printf("888  T88b Y88..88P 888 Y8b.     888 ^88b\n");
    printf("888   T88b ^Y88P^  888  ^Y8888  888  888\n\n");
    printf("Built on: " __DATE__ " " __TIME__ "\n\n");

    // blink the diode really fast until setup() exits
    init_output(PIN_LED);
    Ticker ticker;
    ticker.attach_ms(256, []{
        const auto current_state = digitalRead(PIN_LED);
        digitalWrite(PIN_LED, !current_state);
        });

    printf("Initializing outputs...\n");
    init_output(PIN_EN);
    init_output(PIN_UP);
    init_output(PIN_DN);
    init_output(PIN_LT);
    init_output(PIN_RT);

    printf("Initializing file system...\n");
    SPIFFS.begin();

    setup_wifi();

    printf("Setting up endpoints...\n");
    setup_endpoints();

    // do an initial remote reset
    reset_remote();

    printf("Starting up server...\n");
    server.begin();

    printf("Setup complete.\n");
}

void check_wifi()
{
    static unsigned long wifi_last_connected = millis();
    static auto wifi_last_status = WiFi.status();

    const auto wifi_current_status = WiFi.status();
    const bool connected = (wifi_current_status == WL_CONNECTED);

    if (wifi_current_status != wifi_last_status) {
        printf("WiFi %s (status changed to %i)\n", connected ? "connected" : "disconnected", wifi_current_status);
        wifi_last_status = wifi_current_status;
    }

    if (connected) {
        wifi_last_connected = millis();
    } else if (millis() - wifi_last_connected > 2 * 60 * 1000) {
        printf("WiFi has been disconnected for too long.\n");
        reboot();
    }

    {
        // blink the builtin led to indicate WiFi state
        const unsigned int mask = connected ? 0b1111 : 0b100;
        // 1 tick ~= 128ms, 8 ticks ~= 1s
        const auto blink_phase = (millis() >> 8) & mask;
        digitalWrite(PIN_LED, (blink_phase == 0) ? LOW : HIGH);
    }
}

void loop() {
    check_wifi();
    server.handleClient();
}
