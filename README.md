# Rolek

![Build Status](https://img.shields.io/github/actions/workflow/status/mlesniew/rolek/ci.yml)
![License](https://img.shields.io/github/license/mlesniew/rolek)

Control your window roller shutters over WiFi.

Rolek is a small ESP8266-based project that lets you control electric roller shutters through a web page.


## How it works

The idea is simple: take an existing 433 MHz remote, open it up, and solder wires to its buttons. The ESP8266 then
"presses" the buttons by shorting them electronically, making the remote send signals to the shutters—just like a
real button press.


### Why do it this way?

This is the easiest way to get WiFi control without messing with the shutters’ internals. Also, I had a broken
remote (the display cracked when I dropped it), but it still worked otherwise, so I decided to reuse it instead of
throwing it away.


## How it's built

### The remote

The remote I used has six buttons and can control up to 15 shutters:
  * `UP`, `DOWN`, `STOP` -- moves the currently selected shutter
  * `LEFT`, `RIGHT` -- selects which shutter to control (shown on an LCD screen)
  * `PROGRAM` -- used only for pairing

When the battery is removed and reinserted, the remote resets to shutter #1. There's also a special "shutter 0" mode
that controls all shutters at once.


### The electronics

I wired up the movement buttons to the ESP8266.  The remote’s buttons work by pulling their pins low (0V) when pressed.
To fake a button press, I connected each button’s pin to ground through an NPN transistor, which acts as a switch. The
ESP8266 controls these switches via its GPIO pins.

The remote runs on a small 3V battery, which is perfect because it can be powered directly from the ESP8266’s 3V pin.
To make resetting easier, I also wired the remote’s ground connection through a transistor, allowing the ESP8266 to
briefly cut the remote's power when needed.


### The software

When powered on, the ESP8266:
  * Resets the remote by cutting its power for a few seconds, so it starts in a known state.
  * Connects to WiFi and starts a web server.

The web server provides an interface to control the shutters via a simple Web UI and via REST API endpoints (see below).

#### Setting a Desired Shutter Position

Since neither the remote nor the ESP8266 knows the exact position of the shutters, it has to estimate it.  This works because
the time needed to fully open or close a shutter is fairly consistent.

Here's how it works:
  * When the ESP8266 starts, it has no idea what position the shutters are in.
  * If a shutter is moved up or down for its full duration without being interrupted, the ESP8266 assumes it is now fully
    open or closed.
  * As subsequent commands are issued, the estimated position is updated based on the movement duration.

With the approach above, the ESP8266 can stop a shutter at a specific position, by calculating the correct timing and
sending a stop command.

Since position tracking isn't always reliable (e.g., if a remote is used manually), a `/sync` endpoint is available.
When called, it:
  * Fully opens or closes each shutter.
  * Waits the necessary time to ensure movement has stopped.
  * Moves the shutter again and stops it after a calculated delay to set it at a known position.


### REST API Endpoints

  * `POST /shutters/up` - Opens all shutters.
  * `POST /shutters/down` - Closes all shutters.
  * `POST /shutters/stop` - Stops all shutters.
  * `POST /shutters/<name>/up` - Opens a specific shutter or group.
  * `POST /shutters/<name>/down` - Closes a specific shutter or group.
  * `POST /shutters/<name>/stop` - Stops a specific shutter or group.
  * `POST /shutters/<name>/set/<position>` - Moves a shutter/group to a specific position (0–100, where 0 = closed, 100 = open).
  * `POST /sync` - Ensures shutters are at their expected positions.
  * `POST /reset` - Resets the remote by cutting power.


### Home Assistant Integration

Rolek now supports automatic Home Assistant integration via MQTT.  It's enabled automatically when MQTT is confitured in
`data/network.json`.  Home Assistant should automatically detect all defined shutters and groups.


### Syslog Support

By default, logs are printed over serial at 115200 baud.  However, for convenience, logs can also be sent to a syslog server.
This is enabled by configuring the syslog server’s IP/hostname in `data/network.json`.

The easiest way to run a compatible syslog server is using this Docker image: [mlesniew/syslog](https://hub.docker.com/r/mlesniew/syslog).


### Configuration

#### Shutters

Shutter settings are stored in `data/shutters.json`.  The file defines which shutter names and their numbers matching those on the remote:

```
{
    "Living room": 1,
    "Kitchen": 2,
    "Bedroom": 3,
    "Bathroom": 4
}
```

If needed, additional details can be specified, such as the time required for a shutter to fully open or close. 

```
{
    "Living room": 1,
    "Kitchen": {
        "index": 2,
        "time": 20
    },
    "Bathroom": {
        "index": 3,
        "open_time": 30,
        "close_time": 25
    },
    "Bedroom": 4
}
```

Groups can also be defined, allowing multiple shutters to be controlled together:

```
{
    "Living room": 1,
    "Kitchen": 2,
    "Bedroom": 3,
    "Bathroom": 4,
    "Downstairs": ["Living room", "Kitchen"],
    "Upstairs": ["Bathroom", "Bedroom"]
}
```

Groups can reference other groups as long as there are no circular dependencies.

#### Extra configuration

Additional settings can be defined in `data/network.json`:

  * `hostname` – The hostname used for DHCP, mDNS, and syslog.
  * `mqtt` – MQTT connection details (`host`, `port`, `username`, `password`).
  * `hass_autodiscovery_topic` – Home Assistant auto-discovery topic (default: `homeassistant`).
  * `password` – OTA update password.
  * `syslog` – IP or hostname of a syslog server for remote logging.
