# Rolek
Control your electric window blinds through WiFi

Rolek is a simple electronic project for controlling electric window blinds through WiFi using a simple web page.


## How

The idea is simple.  The blinds must be remote controlled already using a 433 MHz remote.  The original remote needs
to be sacrificed -- it needs to get disassembled and wires need to get soldered to its buttons.  These wires can then
be connected to the Wemos chip.  This way we can pretend to the remote that the buttons are pressed and make it send
the proper code to the blinds.


## Why

The main reason to use the approach with the remote was that it's the easiest.  Also, I had a broken remote already --
its display broke when I dropped it, but everything else worked fine.  Instead of throwing it away, I decided to use
it for this project.


## Design

### The original remote

In my case the original remote has 6 buttons and can control up to 15 blinds: UP, DOWN, LEFT, RIGHT, STOP and PROGRAM.
LEFT and RIGHT are used to select the blinds (a number indicating which one is selected is displayed on a LCD).  UP,
DOWN and STOP control the currently selected blinds.  The blinds go all the way up or all the way down when the buttons
are pressed unless STOP is pressed to stop them somewhere in the middle.  The PROGRAM button is only used to pair the
remote with the blinds.

When the battery gets removed and reinserted, the remote goes back to blind number 1.  It's also possible to go to
number zero which means that all blinds are controlled at once.

### Electronics

I decided to only connect the 4 direction buttons to the Wemos.  The buttons on the remote are probably connected to
some pin in the remote's main chip through a pull up resistor.  Using a multimeter I figured out that pressing the
buttons causes the voltage on their pins to drop to 0 V.  To simulate button presses artificially I connected the pin
of the buttons to the ground through a NPN transistor, which worked as a switch.  Each transistor's base is connected
to one of the Wemos GPIO through a resistor.

The remote is normally powered by a small 3 V battery.  That's just perfect, because it means that it can be powered
directly from the 3 V pin of the Wemos.  To be able to reset the remote from the Wemos easily,  the ground (battery -)
of the remote is connected to the Wemos ground pin through another NPN transistor, just like the buttons.

### Software

Upon startup the Wemos first resets the remote by cutting its power for a few seconds.  This brings the remote back
to a known state.  Next, it connects to WiFi and sets up a simple web server, which can be used to control the blinds.
The two main endpoints are `/up` and `/down`.  Both accept two parameters in the url:
 * `mask` -- a bitmask, which specifies which blinds should get closed or opened, e.g. 6 means blinds 1 and 2, if not
   specified, it defaults to 1 (which means blind number 0 == all blinds).
 * `count` -- specifies the number of times the up or down button press should be repeated, defaults to 1 (useful in
   case of possible interference

These endpoints can be used directly, e.g. using `curl`, but don't need to be.  The page served by the web server
provides a simple easy to use and human friendly interface.  The labels for the buttons on the page and their
corresponding masks are taken from `config.json`, which can be generated using the `config.py` script.


## Other possible approaches

In theory, a 433 MHz transmitter could be used to directly send the right code to the blinds.  This would be much
cheaper, as the original remote wouldn't be needed.  However, modern blinds use a proprietary rolling code algorithm.
Every time a button is pressed on the remote, the code sent out is different.  To use the approach with the 433 MHz
transmitter, the code generation would have to be reverse engineered or cracked somehow.

Another way to remotely control blinds is to connect Wemos chips with relays directly to the blinds motors.  This would
require one Wemos per window.  Also, this way it wouldn't be possible to use any additional original remotes anymore.

