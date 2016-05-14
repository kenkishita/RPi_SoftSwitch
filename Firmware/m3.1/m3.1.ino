/*
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
 */

const int buttonPin = 1;  // Momentary push button
const int ledPin = 4;     // LED
const int outPin = 2;     // MicroUSB 5V Output controll signal
const int shutdownRQPin = 0;   //Output of the shutdown request pin
const int rpiKAPin = 3; //RPi keepalive watch pin

int stateLoc = 0;
int buttonState = LOW;
int rpiState = LOW;  //input status from rpiKAPin
int ledState = LOW;  //LED status
long w1Time = 0;
long w2Time = 0;
long w3Time = 0;
long w8Time = 0;
long w9Time = 0;

//int delayTime = 250;
long debounceDelay = 50;
long resetTimerDelay = 1000;
long waitingDelay = 30000;
long shutdownDelay = 15000;
long rebootDelay = 30000;
long forcePWRoffDelay = 5000;
long afterKAlowDelay = 10000; //power off delay after rpiKAPin to low 
boolean connectKAPin = true;
boolean entForcePWRoff = false;
boolean debugMode = false;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);  // Momentary push button
  pinMode(ledPin, OUTPUT);
  pinMode(outPin, OUTPUT);
  pinMode(shutdownRQPin, OUTPUT);
  pinMode(rpiKAPin, INPUT);
  
  // set initial state
  digitalWrite(ledPin, LOW);
  digitalWrite(outPin, LOW);
  digitalWrite(shutdownRQPin, LOW);
}
void(* resetFunc) (void) = 0; //declare reset function @ address 0

void loop() {
    switch (stateLoc) {
      case 0:
        // start up procedure  (wait the push button)
        buttonState = !digitalRead(buttonPin);
          if (buttonState == HIGH) {
            if (w1Time == 0) w1Time = millis();
            if ((millis() - w1Time) > debounceDelay) {
                // push
              ledState = HIGH;
              digitalWrite(ledPin, ledState);
              digitalWrite(outPin, HIGH);
              w1Time = 0;
              stateLoc = 1;   //next state
            }
          }
        break;
      case 1:
        // button release check
        buttonState = !digitalRead(buttonPin);
        if (buttonState == LOW) {
          if (w1Time == 0) w1Time = millis();
          if ((millis() - w1Time) > debounceDelay) {
          // release button
            stateLoc = 2;
            w1Time = 0;
            if (debugMode) led_blink(1, 300, ledState);
          }
        }          
        break;
      case 2:
        // bootstrapping of the RPi
        if (w1Time == 0) w1Time = millis();
        rpiState = digitalRead(rpiKAPin);
        if (rpiState == HIGH) {
          if (debugMode) led_blink(2, 300, ledState);
          stateLoc = 3;
          w1Time = 0;
        } else if ((millis() - w1Time) > waitingDelay) {
          connectKAPin = false;
          led_blink(2, 800, ledState); //indicate the KA pin disconnected or doesn't configuration
          stateLoc = 3;
          w1Time = 0;
        }
        break;
      case 3:
          // normal operation
        buttonState = !digitalRead(buttonPin);  //check the pushing button for shutdown
        if (buttonState == HIGH) {
          if (w1Time == 0) w1Time = millis();
          if ((millis() - w1Time) > debounceDelay) {
            digitalWrite(shutdownRQPin, HIGH);
            ledState = LOW;
            digitalWrite(ledPin, ledState);
            if (debugMode) led_blink(4, 300, ledState);
            stateLoc = 6;  //normal shutdown mode
            w1Time = 0;
            //  delay(delayTime);
          } 
        } else if ((millis() - w1Time) > resetTimerDelay) w1Time = 0;
        // RPi set rpiKAPin to low (force overwrite/shutdown/reboot)
        if (connectKAPin && !w1Time) {
          rpiState = digitalRead(rpiKAPin);
          if (rpiState == LOW) {
            if (w2Time == 0) w2Time = millis();
            if ((millis() - w2Time) > rebootDelay) {
              ledState = LOW;
              digitalWrite(ledPin, ledState);
              digitalWrite(shutdownRQPin, HIGH);  //shutdwon request even if any stuation
              if (debugMode) led_blink(5, 300, ledState);
              stateLoc = 5;  //Shutdown mode by rpiKAPin is set to LOW
              w2Time = 0;
            }
          } else if (rpiState == HIGH && (w2Time != 0)) {
                led_blink(3, 800, ledState);  //recognize RPi rebooted
                w2Time = 0;
          }            
        }
        // It can take relief action for connection the KA pin after boot.
        if (!connectKAPin) {
          rpiState = digitalRead(rpiKAPin);
          if (rpiState == HIGH) {
            if (w3Time == 0) w3Time = millis();
            if ((millis() - w3Time) > resetTimerDelay) {
              connectKAPin = true;
              w3Time = 0;
              led_blink(2, 300, ledState);
            }
          } else if ((millis() - w3Time) > 3*resetTimerDelay) w3Time = 0;    
        }
        break;
      case 5:
         // rpiKAPin is set to Low
         stateLoc = 7; //Power off
         led_blink(10, 30, ledState);
         delay(shutdownDelay);  //need to know enough of shutdown time
         break;
      case 6:
         // normal shutdown mode
         if (connectKAPin) {
           rpiState = digitalRead(rpiKAPin);
           if (rpiState == LOW) {
             if (w1Time == 0) w1Time = millis();
             if ((millis() - w1Time) > afterKAlowDelay) {
               if (debugMode) led_blink(7, 300, ledState);
               stateLoc = 7; //power off
               w1Time = 0;
             }
           }
         } else {
           //without watch the rpiKAPin
             if (w2Time == 0) w2Time = millis();
             if ((millis() - w2Time) > shutdownDelay) {
               stateLoc = 7;
               if (debugMode) led_blink(7, 100, ledState);
               w2Time = 0;
             }
         }
         break;         
      case 7:
        stateLoc = 0;
        buttonState = LOW;
        rpiState = LOW;
        ledState = LOW;
        w1Time = 0;
        w2Time = 0;
        w3Time = 0;
        w8Time = 0;
        w9Time = 0;
        connectKAPin = true;
        entForcePWRoff = false;
        digitalWrite(outPin, LOW);
        digitalWrite(ledPin, ledState);
        digitalWrite(shutdownRQPin, LOW);
        
        delay(3000); //allowance of long push for the force shutdown 
        resetFunc();  //call reset
        break;
      default:
        stateLoc = 7;
        break;
    }

  // Force powerdown    
    if (stateLoc > 1)  {
         buttonState = !digitalRead(buttonPin); //force shutdown check
         if (buttonState == HIGH) {
            if (w8Time == 0) w8Time = millis();
            if ((millis() - w8Time) > debounceDelay) {
              entForcePWRoff = true;  //enter force poweroff count
              if (w9Time == 0) w9Time = millis();
              if ((millis() - w9Time) > forcePWRoffDelay) {
                stateLoc = 7;
                led_blink(5, 30, ledState);
                entForcePWRoff = false;
                w8Time = 0;
                w9Time = 0;
              }
            }
         } else if ((buttonState == LOW) && (entForcePWRoff)) {
           // buttonState to LOW after entForcePWRoff (reset counts)
           w8Time = 0;
           w9Time = 0;
           entForcePWRoff = false;
         }
    }

}

void led_blink (int count, int interval, int ledState) {
  int i = 0;
  while (i < count) {
    ledState = !(ledState);
    digitalWrite(ledPin, ledState);
    delay(interval);
    ledState = !(ledState);
    digitalWrite(ledPin, ledState);
    delay(interval);
    i++;
  }
}
