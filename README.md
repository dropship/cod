## Ca$h on Delivery - Escudo Dos Version

Consumes dropship events from [5-by-5](dropship/5-by-5) and produces
lighting effects using NeoPixel lighting strips.

## Using the `ino` compiler

The `ino` compiler allows you to avoid the shit Arduino IDE.

```
$ brew install pip
$ brew install picocom
$ pip install ino
```

`ino` depends on your existing Arduino installation.  

With the version of Arduino that I have, I was getting errors compiling the Robot_Control library so fuck it:

```
$ rm -rf /Applications/Arduino.app/Contents/Resources/Java/libraries/Robot_Control
```

Setup up the submodule for the arduino library:

```
$ git submodule init
$ git submodule update
```

Now move your source code into the expected directory structure.  Use the [udp_server](https://github.com/dropship/arduino_networking/tree/682f2f1cec39b4a23a53c396c152430ce056fd03/arduino/udp_server) as an example.

```
$ ino build && ino upload && ino serial
$ ino build -m mega2560 && ino upload -m mega2560 && ino serial
```


And you're off to the :horse: races

- After importing any library in `vendor` using the Arduino app, it will copied to and compiled from
`Documents/Ardunio/libraries/<library>`, not `vendor`.

- Exit `ino serial` - `cntl-A cntl-X`

## Portable Hardware

#### Parts List

- Ardunio Uno
- [Adafruit CC3000 WiFi Shield](https://www.adafruit.com/products/1491)
- [Sparkfun Escudo Dos El Wire Shield](https://www.sparkfun.com/products/10878)
- [3V El Wire Inverter](https://www.sparkfun.com/products/10201)
- [El Wire](https://www.sparkfun.com/products/10197) (up to 5 strands, theoretically)
- Battery pack - This [9V pack](https://www.adafruit.com/products/248) plugs right into the Arduino

#### Instructions

- Attach WiFi Shield to Arduino
- Stack Escudo Dos Shield on top of WiFi Shield
- Plug in inverter according to [Escudo Dos tutorial](https://www.sparkfun.com/tutorials/353)
- Plug in el wire strands into proper channels

#### Notes

- This version only supports flashing Strand A with the kick drum. Flashing other
  strands in response to other messages would happen in the `handleEvent()` function.
  Pull requests accepted.
- The fading is flickery because the Escudo Dos doesn't support full fading.
- The El Wire shield has some high voltages on it! Be careful.



#### Reference

- [Adafruit NeoPixel Uberguide](https://learn.adafruit.com/adafruit-neopixel-uberguide/overview)
