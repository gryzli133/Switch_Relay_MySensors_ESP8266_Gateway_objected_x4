/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Henrik EKblad
 * Contribution by a-lurker and Anticimex,
 * Contribution by Norbert Truchsess <norbert.truchsess@t-online.de>
 * Contribution by Ivo Pullens (ESP8266 support)
 *
 * DESCRIPTION
 * The EthernetGateway sends data received from sensors to the WiFi link.
 * The gateway also accepts input on ethernet interface, which is then sent out to the radio network.
 *
 * VERA CONFIGURATION:
 * Enter "ip-number:port" in the ip-field of the Arduino GW device. This will temporarily override any serial configuration for the Vera plugin.
 * E.g. If you want to use the defualt values in this sketch enter: 192.168.178.66:5003
 *
 * LED purposes:
 * - To use the feature, uncomment any of the MY_DEFAULT_xx_LED_PINs in your sketch, only the LEDs that is defined is used.
 * - RX (green) - blink fast on radio message recieved. In inclusion mode will blink fast only on presentation recieved
 * - TX (yellow) - blink fast on radio message transmitted. In inclusion mode will blink slowly
 * - ERR (red) - fast blink on error during transmission error or recieve crc error
 *
 * See http://www.mysensors.org/build/esp8266_gateway for wiring instructions.
 * nRF24L01+  ESP8266
 * VCC        VCC
 * CE         GPIO4
 * CSN/CS     GPIO15
 * SCK        GPIO14
 * MISO       GPIO12
 * MOSI       GPIO13
 * GND        GND
 *
 * Not all ESP8266 modules have all pins available on their external interface.
 * This code has been tested on an ESP-12 module.
 * The ESP8266 requires a certain pin configuration to download code, and another one to run code:
 * - Connect REST (reset) via 10K pullup resistor to VCC, and via switch to GND ('reset switch')
 * - Connect GPIO15 via 10K pulldown resistor to GND
 * - Connect CH_PD via 10K resistor to VCC
 * - Connect GPIO2 via 10K resistor to VCC
 * - Connect GPIO0 via 10K resistor to VCC, and via switch to GND ('bootload switch')
 *
  * Inclusion mode button:
 * - Connect GPIO5 via switch to GND ('inclusion switch')
 *
 * Hardware SHA204 signing is currently not supported!
 *
 * Make sure to fill in your ssid and WiFi password below for ssid & pass.
 */


// Enable debug prints to serial monitor
#define MY_DEBUG

// Use a bit lower baudrate for serial prints on ESP8266 than default in MyConfig.h
#define MY_BAUD_RATE 9600

// Enables and select radio type (if attached)
//#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69

#define MY_GATEWAY_ESP8266

#define MY_ESP8266_SSID "UPC8268667"
#define MY_ESP8266_PASSWORD "Natalia8Marek8"

// Enable UDP communication
//#define MY_USE_UDP

// Set the hostname for the WiFi Client. This is the hostname
// it will pass to the DHCP server if not static.
#define MY_ESP8266_HOSTNAME "ESP8266sensor-gateway"

// Enable MY_IP_ADDRESS here if you want a static ip address (no DHCP)
#define MY_IP_ADDRESS 192,168,0,236

// If using static ip you need to define Gateway and Subnet address as well
#define MY_IP_GATEWAY_ADDRESS 192,168,0,1
#define MY_IP_SUBNET_ADDRESS 255,255,255,0

// The port to keep open on node server mode
#define MY_PORT 5003

// How many clients should be able to connect to this gateway (default 1)
#define MY_GATEWAY_MAX_CLIENTS 2

// Controller ip address. Enables client mode (default is "server" mode).
// Also enable this if MY_USE_UDP is used and you want sensor data sent somewhere.
//#define MY_CONTROLLER_IP_ADDRESS 192, 168, 178, 68

// Enable inclusion mode
#define MY_INCLUSION_MODE_FEATURE

// Enable Inclusion mode button on gateway
// #define MY_INCLUSION_BUTTON_FEATURE
// Set inclusion mode duration (in seconds)
#define MY_INCLUSION_MODE_DURATION 60
// Digital pin used for inclusion mode button
#define MY_INCLUSION_MODE_BUTTON_PIN  3

// Set blinking period
// #define MY_DEFAULT_LED_BLINK_PERIOD 300

// Flash leds on rx/tx/err
// Led pins used if blinking feature is enabled above
//#define MY_DEFAULT_ERR_LED_PIN 2  // Error led pin
//#define MY_DEFAULT_RX_LED_PIN  2  // Receive led pin
//#define MY_DEFAULT_TX_LED_PIN  2  // the PCB, on board LED

#if defined(MY_USE_UDP)
#include <WiFiUdp.h>
#endif

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MySensors.h>  
#include <Bounce2.h>
#include <Wire.h>
#include <MySensors.h>




class SwitchRelay
{ 

  uint8_t child_id;
  int buttonPin;
  int relayPin;
  bool relayON;
  bool relayOFF;
  Bounce debouncer = Bounce();

  public:
  SwitchRelay(int childId, int button, int relay, int debaunce, bool invertedRelay) : msg(childId, V_LIGHT)
  {
    child_id = childId;
    buttonPin = button;
    relayPin = relay;
    relayON = !invertedRelay;
    relayOFF = invertedRelay;
    pinMode(buttonPin, INPUT_PULLUP);
    debouncer.attach(buttonPin);
    debouncer.interval(debaunce);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, loadState(child_id)?relayON:relayOFF); 
  }
  
  MyMessage msg;
  
  void Update()
  {
    if (debouncer.update()) 
    {
      // Get the update value.
      int value = debouncer.read();
      // Send in the new value.
      if(value == LOW)
      {
         saveState(child_id, !loadState(child_id));
         digitalWrite(relayPin, loadState(child_id)?relayON:relayOFF);
         SyncController();
      }
    }    
  }

  void SyncController()
  {
    send(msg.set(loadState(child_id)));
  }
  
  void Present()
  {
    present(child_id, S_LIGHT);
  }

  void Receive(const MyMessage &message)
  {
    // We only expect one type of message from controller. But we better check anyway.
    if (message.type==V_LIGHT && message.sensor==child_id) 
    {
       // Change relay state
       digitalWrite(relayPin, message.getBool()?relayON:relayOFF);
       // Store state in eeprom
       saveState(message.sensor, message.getBool());
       // Write some debug info
       Serial.print("Incoming change for sensor:");
       Serial.print(message.sensor);
       Serial.print(", New status: ");
       Serial.println(message.getBool());
     } 
  }

  
};

// define your switch/relay objects here
// SwitchRelay(int childId, int button, int relay, int debaunce, bool invertedRelay)
SwitchRelay switch1(1, 0, 12, 50, 0);   // 0 = D3,  12 = D6 
SwitchRelay switch2(2, 10, 13, 50, 0);  // 10= SD3, 13 = D7
SwitchRelay switch3(3, 4, 14, 50, 0);   // 4 = D2,  14 = D5
SwitchRelay switch4(4, 2, 5, 50, 0);    // 2 = D4,  5  = D1


void setup() 
{ 
  // Setup locally attached sensors
  delay(10000);
  switch1.SyncController(); // send actual value to controller
  switch2.SyncController();
  switch3.SyncController();
  switch4.SyncController();
}
void presentation()  
{   
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Relay as class object", "1.0");
  switch1.Present();
  switch2.Present();
  switch3.Present();
  switch4.Present();
}

void loop() 
{ 
  switch1.Update();
  switch2.Update();
  switch3.Update();
  switch4.Update();
}

void receive(const MyMessage &message) {
  switch1.Receive(message);
  switch2.Receive(message);
  switch3.Receive(message);
  switch4.Receive(message);
}
