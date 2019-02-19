#ifndef STATIC_H
#define STATIC_H

#include <Arduino.h>

struct StaticEndpoint {
    const char * name;
    const char * mime;
    const unsigned char * data;
    unsigned long size;
};

class ESP8266WebServer;

extern const StaticEndpoint static_endpoints[];

template <typename Guard>
void setup_static_endpoints(ESP8266WebServer & server) {
    Serial.println("Setting up static endpoints...");
    for (const StaticEndpoint * e = static_endpoints; e->name; ++e) {
            Serial.println(e->name);
            server.on(e->name, HTTP_GET, [&server, e]{
                Guard g;
                server.sendHeader("Content-Encoding", "gzip");
                server.send_P(200, e->mime, (PGM_P)(e->data), e->size);
        });
    }
    Serial.println("Setting up static complete.");
}

#endif
