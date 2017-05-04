// 25 fps: 6 out / 300 pixel (1800 px)
// 30 fps: 5 out / 300 pixel (1500 px)
// 40 fps: 4 out / 300 pixel (1200 px)
// 25 fps: 3 out / 600 pixel (1800 px)
// 44 fps: 2 out / 600 pixel (1200 px)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// initial user defined settings
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define NUM_OF_OUTPUTS 2
#define MAX_NUM_LED_PER_OUTPUT 600
#define NUM_CHANNEL_PER_LED 4 // do not change this

#define LED_TYPE    APA102
#define COLOR_ORDER RGB
#define BRIGHTNESS 255
#define REFRESH_RATE_KHZ 1200

//#define blackOnOpSyncTimeOut //recoment more than 20000 ms
//#define blackOnOpPollTimeOut //recoment more than 20000 ms
const static uint32_t OpSyncTimeOut = 300000;
const static uint32_t OpPollTimeOut = 30000;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// definitions calculated from user settings
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int num_channel_per_output = MAX_NUM_LED_PER_OUTPUT * NUM_CHANNEL_PER_LED;

const int num_universes_per_output = (num_channel_per_output % 512) ? num_channel_per_output / 512 + 1 : num_channel_per_output / 512;

const int num_led_per_output = num_universes_per_output * 512 / NUM_CHANNEL_PER_LED;

const int num_artnet_ports = num_universes_per_output * NUM_OF_OUTPUTS;

const int num_led_per_universe = int(512 / NUM_CHANNEL_PER_LED);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// includes and lib
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include <Ethernet.h>
#include <ArtNode.h>
#include "ArtNetFrameExtension.h"

#include "FastLED.h"

#include "TeensyMAC.h"
#include <EEPROM.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// settings error check
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if F_BUS < 60000000
#error "Teensy needs to run at 120MHz to read all packets in time"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// real code starts hear
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VERSION_HI 0
#define VERSION_LO 9

#define PIN_RESET 9

EthernetUDP udp;
uint8_t udp_buffer[600];

boolean locateMode = 0;

// variables for the node.report
float tempVal = 0;
float fps = 0;
float avgUniUpdated = 0;
uint8_t numUniUpdated = 0;
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;

uint32_t lastPoll = 0;
uint32_t lastSync = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FastLED config
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int num_leds = NUM_OF_OUTPUTS * MAX_NUM_LED_PER_OUTPUT;

CRGB leds[NUM_OF_OUTPUTS * MAX_NUM_LED_PER_OUTPUT];

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Art-Net config
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ArtConfig config = {
  {0xDE, 0xAD, 0xBE, 0x00, 0x00, 0x00}, // MAC - last 3 bytes set by Teensy
  {2, 0, 0, 1},                         // IP
  {255, 0, 0, 0},                       // Subnet mask
  0x1936,                               // UDP port
  false,                                // DHCP

  // These fields get overwritten by loadConfig:
  2, 6,                                 // Net (0-127) and subnet (0-15)
  "Pixel2LED#21",                           // Short name
  "Pixel2LED#21",                     // Long name
  num_artnet_ports, // Number of ports
  { PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput
  }, // Port types
  {0, 0, 0, 0},                         // Port input universes (0-15)
  {0, 1, 2, 3},                          // Port output universes (0-15)
  VERSION_HI,
  VERSION_LO
};

ArtNodeExtended node;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// artnetSend - takes a buffer pointer and its length
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void artnetSend(byte* buffer, int length) {
  udp.beginPacket(node.broadcastIP(), config.udpPort);
  udp.write(buffer, length);
  udp.endPacket();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Blink test all the leds full white
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void blink() {
  for (int i = 0; i < num_leds; i++) {
    leds[i] = CRGB(0, 0, 255); //set blue
  }
  FastLED.show();
  delay(300);
  for (int i = 0; i <  num_leds; i++) {
    leds[i] = CRGB(0, 0, 0); //set 0
  }
  FastLED.show();
  delay(100);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// EEPROM setup
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ID of the settings block
#define CONFIG_VERSION "ls1"

// Tell it where to store your config data in EEPROM
#define CONFIG_MEM_START 16
#define CONFIG_START 17
#define CONFIG_END 2

int oemCode = 0x0000; // OemUnkown

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// loadConfig - loads configurations from EEPROM
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_MEM_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_MEM_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_MEM_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int t = CONFIG_START; t < sizeof(config) - CONFIG_END; t++) {
      *((char*)&config + t ) = EEPROM.read(CONFIG_MEM_START + t + 3 - CONFIG_START);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// saveConfig - saves configurations to EEPROM
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void saveConfig() {
  EEPROM.write(CONFIG_MEM_START + 0, CONFIG_VERSION[0]);
  EEPROM.write(CONFIG_MEM_START + 1, CONFIG_VERSION[1]);
  EEPROM.write(CONFIG_MEM_START + 2, CONFIG_VERSION[2]);
  for (unsigned int t = CONFIG_START; t < sizeof(config) - CONFIG_END; t++) {
    EEPROM.write(CONFIG_MEM_START + t - CONFIG_START + 3, *((char*)&config + t));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// setup
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
 // Serial.begin(115200);
  saveConfig(); //<-- uncomment to force the EEPROM config to your settings on eatch reboot
  ArtConfig tempConfig = config;
  loadConfig();
  config.numPorts = tempConfig.numPorts;
  config.numPorts = tempConfig.numPorts;
  config.verHi = tempConfig.verHi;
  config.verLo = tempConfig.verLo;
  saveConfig();


#ifdef PIN_RESET
  pinMode(PIN_RESET, OUTPUT);
  digitalWrite(PIN_RESET, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_RESET, HIGH);
  delay(150);
#endif

  // Read MAC address
  mac_addr mac;
  for (int i = 3; i < 6; i++) {
    config.mac[i] = mac.m[i];
  }

  // Calculate IP address
  config.ip[0] = 2;
  config.ip[1] = config.mac[3] + (oemCode & 0xFF);// + ((oemCode >> 16) & 0xFF);
  config.ip[2] = config.mac[4];
  config.ip[3] = config.mac[5];

  // Open Ethernet connection
  IPAddress gateway(config.ip[0], 0, 0, 1);
  IPAddress subnet(255, 0, 0, 0);

  Ethernet.begin(config.mac, config.ip,  gateway, gateway, subnet);
  udp.begin(config.udpPort);

  // Open ArtNet
  node = ArtNodeExtended(config, sizeof(udp_buffer), udp_buffer);

  // Setup FastLed
  if (NUM_OF_OUTPUTS > 0)FastLED.addLeds<LED_TYPE, 14, 7, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 0, MAX_NUM_LED_PER_OUTPUT);
  if (NUM_OF_OUTPUTS > 1)FastLED.addLeds<LED_TYPE, 6, 5, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, MAX_NUM_LED_PER_OUTPUT, MAX_NUM_LED_PER_OUTPUT);
  if (NUM_OF_OUTPUTS > 2)FastLED.addLeds<LED_TYPE, 6, 5, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 2 * MAX_NUM_LED_PER_OUTPUT, MAX_NUM_LED_PER_OUTPUT);
  if (NUM_OF_OUTPUTS > 3)FastLED.addLeds<LED_TYPE, 16, 15, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 3 * MAX_NUM_LED_PER_OUTPUT, MAX_NUM_LED_PER_OUTPUT);
  if (NUM_OF_OUTPUTS > 4)FastLED.addLeds<LED_TYPE, 18, 17, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 4 * MAX_NUM_LED_PER_OUTPUT, MAX_NUM_LED_PER_OUTPUT);
  if (NUM_OF_OUTPUTS > 5)FastLED.addLeds<LED_TYPE, 20, 19, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 5 * MAX_NUM_LED_PER_OUTPUT, MAX_NUM_LED_PER_OUTPUT);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setDither(1);

  blink();

  // to read internal temperature
  analogReference(INTERNAL);
  analogReadResolution(12);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// main loop
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  while (udp.parsePacket()) {
    // First read the header to make sure it's Art-Net
    unsigned int n = udp.read(udp_buffer, sizeof(ArtHeader));
    if (n >= sizeof(ArtHeader)) {
      ArtHeader* header = (ArtHeader*)udp_buffer;
      // Check packet ID
      if (memcmp(header->ID, "Art-Net", 8) == 0) {  //is Art-Net
        // Read the rest of the packet
        udp.read(udp_buffer + sizeof(ArtHeader), udp.available());
        // Package Op-Code determines type of packet
        switch (header->OpCode) {

          // Poll packet
          case OpPoll: {
              //T_ArtPoll* poll = (T_ArtPoll*)udp_buffer;
              //if(poll->TalkToMe & 0x2){

#ifdef blackOnOpPollTimeOut
              lastPoll = millis();
#endif

              float tempCelsius = 25.0 + 0.17083 * (2454.19 - tempVal);
              sprintf(node.pollReport, "numOuts;%d;numUniPOut;%d;temp;%.1f;fps;%.1f;uUniPF;%.1f;", NUM_OF_OUTPUTS, num_universes_per_output, tempCelsius, fps, avgUniUpdated);
              node.createPollReply(); //create pollReply
              artnetSend(udp_buffer, sizeof(ArtPollReply)); //send pollReply
              //}
              break;
            }

          // DMX packet
          case OpDmx: {
              ArtDmx* dmx = (ArtDmx*)udp_buffer;
              int port = node.getAddress(dmx->SubUni, dmx->Net) - node.getStartAddress();
              if (port >= 0 && port < config.numPorts) {


                uint16_t portOffset = 0;

                for (int i = 0; i < port; i++) {
                  portOffset = portOffset + num_led_per_universe;
                  // skip to next universe, if the number of led per output is reached.
                  // like that, every output of the controller will start with dmx adress 1 on a new universe
                  int numLedOnOutput = portOffset - int(portOffset / MAX_NUM_LED_PER_OUTPUT) * MAX_NUM_LED_PER_OUTPUT; //subtstracts the outputs where the max number of led is reached. the result tells, how much left over data exists in the universe
                  if (numLedOnOutput == num_led_per_output - MAX_NUM_LED_PER_OUTPUT) {
                    portOffset = portOffset - numLedOnOutput;
                  }
                }

                //write the dmx data to the Octo frame buffer
                uint32_t* dmxData = (uint32_t*) dmx->Data;
                for (int i = 0; i < 128; i++) {
                  //int ledIndex=i+portOffset
                  leds[i + portOffset] = CRGB(dmxData[i]);
                }
                numUniUpdated++;
              }
              break;
            }

          // OpSync
          case 0x5200: {
            //  Serial.println(fps);
              FastLED.show();

#ifdef blackOnOpSyncTimeOut
              lastSync = millis();
#endif

              // calculate framerate
              currentMillis = millis();
              if (currentMillis > previousMillis) {
                fps = 1 / ((currentMillis - previousMillis) * 0.001);
              } else {
                fps = 0;
              }
              previousMillis = currentMillis;

              // calculate average universes Updated
              avgUniUpdated = numUniUpdated * 0.16 + avgUniUpdated * 0.84;
              numUniUpdated = 0;

              break;
            }

          // OpAddress
          case OpAddress: {

              T_ArtAddress * address = (T_ArtAddress*)udp_buffer;

              if (address->LongName[0] != 0) {
                memcpy(config.longName, address->LongName, 64);
              }
              if (address->ShortName[0] != 0) {
                memcpy(config.shortName, address->ShortName, 18);
              }
              if (address->NetSwitch != 0x7F) {               // Use value 0x7f for no change.
                if ((address->NetSwitch & 0x80) == 0x80) { // This value is ignored unless bit 7 is high. i.e. to program a  value 0x07, send the value as 0x87.
                  config.net = address->NetSwitch & 0x7F;
                }
              }
              if (address->SubSwitch != 0x7F) {               // Use value 0x7f for no change.
                if ((address->SubSwitch & 0x80) == 0x80) { // This value is ignored unless bit 7 is high. i.e. to program a  value 0x07, send the value as 0x87.
                  config.subnet = address->SubSwitch & 0x7F;
                }
              }
              for (int i = 0; i < 4; i++) {
                if (address->SwIn[i] != 0x7F) {
                  if ((address->SwIn[i] & 0x80) == 0x80) {
                    config.portAddrIn[i] = address->SwIn[i] & 0x7F;
                  }
                }
                if (address->SwOut[i] != 0x7F) {
                  if ((address->SwOut[i] & 0x80) == 0x80) {
                    config.portAddrOut[i] = address->SwOut[i] & 0x7F;
                  }
                }
              }

              if (address->Command == 0x04) {
                locateMode = true;
              } else {
                locateMode = false;
              }
              node = ArtNodeExtended(config, sizeof(udp_buffer), udp_buffer);
              saveConfig();
              loadConfig();
              node.createPollReply();
              artnetSend(udp_buffer, sizeof(ArtPollReply));
              //node.createExtendedPollReply();
              //artnetSend(udp_buffer, node.sizeOfExtendedPollReply());
              break;
            }

          // Unhandled packet
          default: {
              break;
            }
        }

        // answer routine for Art-Net Extended
      } else if (memcmp(header->ID, "Art-Ext", 8) == 0) {
        // Read the rest of the packet
        udp.read(udp_buffer + sizeof(ArtHeader), udp.available());
        // Package Op-Code determines type of packet
        switch (header->OpCode) {
          // ArtNet Frame Extension
          case OpPoll | 0x0001: {
              node.createExtendedPollReply();
              artnetSend(udp_buffer, node.sizeOfExtendedPollReply());
              break;
            }
        }
      } else if (memcmp(header->ID, "MadrixN", 8) == 0) {
        LEDS.show();

#ifdef blackOnOpSyncTimeOut
        lastSync = millis();
#endif

        // calculate framerate
        currentMillis = millis();
        if (currentMillis > previousMillis) {
          fps = 1 / ((currentMillis - previousMillis) * 0.001);
        } else {
          fps = 0;
        }
        previousMillis = currentMillis;

        // calculate average universes Updated
        avgUniUpdated = numUniUpdated * 0.16 + avgUniUpdated * 0.84;
        numUniUpdated = 0;
      }
    }
  }

  // read temperature value
  tempVal = analogRead(38) * 0.01 + tempVal * 0.99;

#ifdef blackOnOpSyncTimeOut
  currentMillis = millis();
  if (currentMillis - lastSync > OpSyncTimeOut) {
    for (int i = 0; i < num_led_per_output * 8; i++) {
      LEDS.setPixel(i, 0);
    }
    LEDS.show();
  }
#endif

#ifdef blackOnOpPollTimeOut
  currentMillis = millis();
  if (currentMillis - lastPoll > OpPollTimeOut) {
    for (int i = 0; i < num_led_per_output * 8; i++) {
      LEDS.setPixel(i, 0);
    }
    LEDS.show();
  }
#endif
}
