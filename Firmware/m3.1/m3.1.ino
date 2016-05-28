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

Changed LED status to analog output (8 level)
0,   1,    2,   3,     4,     5,     6,     7
0,   8,   16,  32,    64,   128,   224,   252
 */

#include <avr/io.h>
#include <avr/wdt.h> 
#include <avr/interrupt.h>

const int buttonPin = 1;  // Momentary push button
const int ledPin = 4;     // LED
const int outPin = 2;     // MicroUSB 5V Output controll signal
const int shutdownRQPin = 0;   //Output of the shutdown request pin
const int rpiKAPin = 3; //RPi keepalive watch pin

int stateLoc = 0;
int buttonState = LOW;
int rpiState = LOW;  //input status from rpiKAPin
int ledState[] = {
  255, 247, 239, 223, 191, 127, 31, 3
};
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
int wdt_counter = 0;

void setup() {
  // Enable PLL and async PCK for high-speed PWM
  PLLCSR |= (1 << PLLE) | (1 << PCKE);
  // Set prescaler to PCK/2048
  TCCR1 |= (1 << CS10) | (1 << CS11) | (0 << CS12) | (0 << CS13);
  // Enable OCRB output on PB4, configure compare mode and enable PWM B
  // DDRB-Port B Data Direction Register
  DDRB |= (1 << PB4);
  // GTCCR-General Timer/Counter Control Register
  GTCCR |= (1 << COM1B0) | (1 << COM1B1);
  GTCCR |= (1 << PWM1B);
  // I don't know why the follow line is needed, but if not it dosen't work correctly.
  TCCR1 |= (1 << COM1A0);

  pinMode(buttonPin, INPUT_PULLUP);  // Momentary push button
  pinMode(outPin, OUTPUT);
  pinMode(shutdownRQPin, OUTPUT);
  pinMode(rpiKAPin, INPUT);
  
  // set initial state
  // Set OCR1B compare value and OCR1C TOP value
  OCR1B = ledState[0];
  OCR1C = 255;
  digitalWrite(outPin, LOW);
  digitalWrite(shutdownRQPin, LOW);

  //wdt_enable(WDTO_4S); 
  //delay(500);
  watchdogStart();
}

void watchdogStart(void)
{
 cli();  // disable all interrupts 
 wdt_reset(); // reset the WDT timer
 // Enter Watchdog Configuration mode: 
 WDTCR |= (1<<WDCE) | (1<<WDE); // Set Watchdog settings:
 WDTCR = (1<<WDIE) | (0<<WDE) | (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (0<<WDP0); 
 sei();
}

void watchdogArm(void)
{
 cli();  // disable all interrupts 
 wdt_reset(); // reset the WDT timer
 // Enter Watchdog Configuration mode: 
 WDTCR |= (1<<WDCE) | (1<<WDE); // Set Watchdog settings:
 WDTCR = (1<<WDIE) | (1<<WDE) | (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (0<<WDP0); 
 sei();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
   if(wdt_counter==0)
   {
    wdt_counter++;
    watchdogArm();
   }
  stateLoc = 3; //force normal operation if freeze the program
  connectKAPin = false;
}

void led_blink (int count, int interval, int ledInt) {
  int i = 0;
  int uledInt = ledInt;
  while (i < count) {
    if (ledInt < 255) ledInt = 255;
    OCR1B = ledInt;
    delay(interval);
    if ((ledInt == 255) && (uledInt != 255)) {
      ledInt = uledInt;
    } else {
      ledInt = 0;
    }
    OCR1B = ledInt;
    delay(interval);
    i++;
  }
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void loop() {
    wdt_reset();
    switch (stateLoc) {
      case 0:
        // start up procedure  (wait the push button)
        buttonState = !digitalRead(buttonPin);
          if (buttonState == HIGH) {
            if (w1Time == 0) w1Time = millis();
            if ((millis() - w1Time) > debounceDelay) {
                // push
              OCR1B = ledState[1];
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
            OCR1B = ledState[2];
            if (debugMode) led_blink(1, 300, ledState[2]);
          }
        }          
        break;
      case 2:
        // bootstrapping of the RPi
        if (w1Time == 0) w1Time = millis();
        rpiState = digitalRead(rpiKAPin);
        if (rpiState == HIGH) {
          if (debugMode) led_blink(2, 300, ledState[7]);
          stateLoc = 3;
          OCR1B = ledState[7];  //Max. brightness
          w1Time = 0;
        } else if ((millis() - w1Time) > waitingDelay) {
          connectKAPin = false;
          OCR1B = ledState[6];
          led_blink(2, 800, ledState[6]); //indicate the KA pin disconnected or doesn't configuration
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
            OCR1B = ledState[3];
            if (debugMode) led_blink(4, 300, ledState[3]);
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
              OCR1B = ledState[4];
              digitalWrite(shutdownRQPin, HIGH);  //shutdwon request even if any stuation
              if (debugMode) led_blink(5, 300, ledState[4]);
              stateLoc = 5;  //Shutdown mode by rpiKAPin is set to LOW
              w2Time = 0;
            }
          } else if (rpiState == HIGH && (w2Time != 0)) {
                led_blink(3, 800, ledState[7]);  //recognize RPi rebooted
                OCR1B = ledState[7];
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
              led_blink(2, 300, ledState[7]);
              OCR1B = ledState[7];
           }
          } else if ((millis() - w3Time) > 3*resetTimerDelay) w3Time = 0;    
        }
        break;
      case 5:
         // rpiKAPin is set to Low
         stateLoc = 7; //Power off
         led_blink(10, 30, ledState[2]);
         OCR1B = ledState[2];
         delay(shutdownDelay);  //need to know enough of shutdown time
         break;
      case 6:
         // normal shutdown mode
         if (connectKAPin) {
           rpiState = digitalRead(rpiKAPin);
           if (rpiState == LOW) {
             if (w1Time == 0) w1Time = millis();
             if ((millis() - w1Time) > afterKAlowDelay) {
               if (debugMode) led_blink(7, 300, ledState[1]);
               stateLoc = 7; //power off
               OCR1B = ledState[1];
               w1Time = 0;
             }
           }
         } else {
           //without watch the rpiKAPin
             if (w2Time == 0) w2Time = millis();
             if ((millis() - w2Time) > shutdownDelay) {
               OCR1B = ledState[1];
               stateLoc = 7;
               if (debugMode) led_blink(7, 100, ledState[1]);
               w2Time = 0;
             }
         }
         break;         
      case 7:
        stateLoc = 0;
        buttonState = LOW;
        rpiState = LOW;
        w1Time = 0;
        w2Time = 0;
        w3Time = 0;
        w8Time = 0;
        w9Time = 0;
        connectKAPin = true;
        entForcePWRoff = false;
        digitalWrite(outPin, LOW);
        OCR1B = ledState[0];
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
                led_blink(5, 30, ledState[7]);
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

