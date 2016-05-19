RPi Power controller
This board can plug directly to the Raspberry Pi power connector.

Assembling

This board has 8 ports, +V, KA, GND, GND, P2, SWP1, SDP0, and RST. 
But P2 and RST will need connect only when re-programming the U1 (Attiny85).
A momentary switch connects between SWP1 and GND.
SDP0 pin connect to the GPIO23(Top P8) of the Raspberry Pi, and also KA pin to the GPIO24(Top P9) by default.
Any 5V power supply output connect to the +V and GND.

The circuit will auto-detect whether you are configured the SDP0 and KA pin.
After wiring, pressing the button turns the Raspberry Pi on. Then, when you want to turn this off just briefly press the button and the operating system will safely shut down if the connected SDP0 and KA to the specified GPIO.
After the operating system shuts down, the switch will cut the power to the Raspberry Pi.
If holding for the button over 5 seconds will perform force power off with flasshing the LED.
You have to install small script for shutdown functionality in the Raspbian.
Configure the Raspberry Pi
Sample of setup.sh

--- begen
    echo '#!/bin/bash
    
    #this is the GPIO pin connected to the lead on SDP0
    GPIOpin1=23
    
    #this is the GPIO pin connected to the lead on KA
    GPIOpin2=24

    echo "$GPIOpin1" > /sys/class/gpio/export
    echo "in" > /sys/class/gpio/gpio$GPIOpin1/direction
    echo "$GPIOpin2" > /sys/class/gpio/export
    echo "out" > /sys/class/gpio/gpio$GPIOpin2/direction
    echo "1" > /sys/class/gpio/gpio$GPIOpin2/value
    while [ 1 = 1 ]; do
    power=$(cat /sys/class/gpio/gpio$GPIOpin1/value)
    if [ $power = 0 ]; then
    sleep 1
    else
    sudo poweroff
    fi
    done' > /etc/switch.sh
    sudo chmod 777 /etc/switch.sh
    sudo sed -i '$ i /etc/switch.sh &' /etc/rc.local
    
--- end of setup.sh

License Information

These files are free; you can redistribute them and/or modify them under the
terms of the Creative Commons Attribution-ShareAlike 4.0 International
License only, as published by the Creative Commons organization.

These files are distributed in the hope that they will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the Creative Commons Attribution-
ShareAlike 4.0 International License for more details.

Please contact Aynik technology Co., Ltd. at kkishita@aynik-tech.jp if you need additional information or have any questions.
