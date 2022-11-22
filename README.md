ESP32 Propane Sensor for Genmon & Hubitat using parts from a TankUtility unit
=======
The goal of this project is to take a water-logged TankUtility unit, rip the
board out, throw a ESP32 Firebeetle + Solar charging board (optional), and
re-use the R3D sensor (~$50 by itself) to measure my propane tank. This arduino
code can optionally send information to Genmon (using the Genmon API) to
Hubitat (using a custom driver), or simply transmit the information over BTLE.  

<img src="https://bdwilson.github.io/images/IMG_6744.jpeg" width=400px>
<img src="https://bdwilson.github.io/images/IMG_6748.jpeg" width=400px>

With optional Solar Panel & Solar charger:
<img src="https://bdwilson.github.io/images/IMG_0276.jpeg" width=400px>
<img src="https://bdwilson.github.io/images/IMG_0275.jpeg" width=400px>

Requirements
------------
To get started you'll need:
- R3D Hall sensor from a dead TankUtility device (or other Hall sensor). [This
  Thread](https://forums.homeseer.com/forum/legacy-software-plug-ins/legacy-plug-ins/legacy-homeseer-plug-ins/adi-ocelot/53385-propane-level-monitoring-with-rd3-hall-effect-sensor/page2) has info on building one. 
- [DFRobot FireBeetle 32](https://amzn.to/3Xkgm8t) - I chose this because of
  the [extreme deep sleep
mode](https://diyi0t.com/reduce-the-esp32-power-consumption/) it can go into. Wifi vs. BTLE was neglible in my
testing, so I opted for Wifi. 
- [DFRobot Solar Controller](https://amzn.to/3VfDWBt) - optional. Depending on
  how often you want your esp32 to check the level, you may not need solar
panel or charger. If you bump it out to like 4x a day or once a day, you might
not need this. You can always add it later.
- [3700mah LiPO](https://amzn.to/3gmUqsW). 
- [5v Solar Panel](https://amzn.to/3Xgk8jn).
- [Arduino](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)
- [Hubitat](https://github.com/bdwilson/hubitat/tree/master/ESP32_Propane) -
  optional app and driver here.
- [Genmon](https://github.com/jgyates/genmon/) - pass your propane percentage
  directly to the Genmon API.

Installation
--------------------

1. Dissect Tank Utility device and connect Red to 3V3, Black to GND, and White
to A0. 

2. Connect battery directly to FireBeetle input OR connect it to the solar
charger and connect the 5V to 5V from the Solar Charger to the FireBeetle and
GND to GND.  

3. Load [the
code](https://github.com/bdwilson/esp-propane-sensor/tree/master/hall_sensor-firebeetle-esp32.ino)
into Arduino. You will need to add
<code>http://download.dfrobot.top/FireBeetle/package_DFRobot_index.json</code>
to <b>Additional Boards Manager</b> then go to Boards Manager and add
<b>DFRobot ESP32 Boards</b>. Configure your board as follows:
<img src="https://bdwilson.github.io/images/esp32-settings.png" width=400px>

4. Customize settings in the Arduino code - Wifi, using hubitat? using genmon?
enable BTLE? You can also search for <code>esp_sleep</code> and see additional options for
longer sleep times. You can also disable LED to save power, but it's handy to
leave it enabled when testing.  
 
5. Optionally connect a solar charger.  

 

Bugs/Contact Info
-----------------
Bug me on Twitter at [@brianwilson](http://twitter.com/brianwilson) or email me [here](http://cronological.com/comment.php?ref=bubba).


