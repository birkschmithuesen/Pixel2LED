/**********************************************************************
 * EEPROM pos 20 is the ID of the controller. Never change this!!!!!!
 * ********************************************************************
 *
 * **********Bugs:
 *
 * => get problem with last universe to work
 * => check out max framerates and nr of px
 * => Namenvergabe nach ControllerID
 */


// 30 fps: 5 out / 300 pixel (1500 px)
// 40 fps: 4 out / 300 pixel (1200 px)
// 25 fps: 3 out / 600 pixel (1800 px)
// 40 fps: 2 out / 600 pixel (1200 px)
// 25 fps: 2 out / 900 pixel (1800 px)

/*  EEPROM adressing
 0 checkID
1,2,3,4,5,6 MAC
7,8,9,10 IP
11,12,13,14 subnet
15, 16 universe (lowbyte, highbyte)
17 Number of Outputs
18,19 number of leds/output (lowbyte, highbyte)
 */


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// includes and lib
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include <Ethernet.h>
#include <ArtNode.h>
#include "ArtNetFrameExtension.h"

#include <TextFinder.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include "FastLED.h"
#include "TeensyMAC.h"



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// initial user defined settings
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
byte NUM_OF_OUTPUTS;
int hardware_num_led_per_output;
byte NUM_CHANNEL_PER_LED = 4; // do not change this

#define LED_TYPE    APA102
#define COLOR_ORDER RGB
byte BRIGHTNESS = 255;
const int REFRESH_RATE_KHZ = 1200;

//#define blackOnOpSyncTimeOut //recoment more than 20000 ms
//#define blackOnOpPollTimeOut //recoment more than 20000 ms
const static uint32_t OpSyncTimeOut = 300000;
const static uint32_t OpPollTimeOut = 30000;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// definitions calculated from user settings
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int num_channel_per_output;
int num_universes_per_output;
int num_led_per_output;
int num_artnet_ports;
int num_led_per_universe;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FastLED config
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int num_leds;
CRGB* leds = 0;
//CRGB leds[301];




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// settings error check
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if F_BUS < 60000000
//#error "Teensy needs to run at 120MHz to read all packets in time"
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

byte controllerID = 0;

int redLed = 23;
boolean controlLedPause=false;
boolean receiveArtNet=false;
int waitForNextArtNet=0;
boolean restart =false;

#define RESTART_ADDR       0xE000ED0C
#define READ_RESTART()     (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WebServer
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//seting up the EthernetShield
byte mac[] = {0xDE, 0xAD, 0xBE, 0x00, 0x00, 0x00};
byte ip[] = {2, 0, 0, 100};
byte subnet[] = {255, 0, 0, 0};
byte gateway[] = {2, 0, 0, 0};
uint16_t universe = 0;
EthernetServer server(80);


// This is our buffer through which we will will "flow" our HTML code.
// It has to be as big as the longest character chain +1 including the "
char buffer[100];

// This is the HTML code all chopped up. The best way to do this is, is by typing
// your HTML code in an editor, counting your characters and divide them by 8.
// you can chop your HTML on every place, but not on the \" parts. So remember,
// you have to use \" instead of simple " within the HTML, or it will not work.

prog_char htmlx0[] PROGMEM = "<html><title>PIXEL2LED Node Setup</title><body marginwidth=\"0\" marginheight=\"0\" ";
prog_char htmlx1[] PROGMEM = "leftmargin=\"0\" style=\"margin: 0; padding: 0;\"><table bgcolor=\"#999999\" border";
prog_char htmlx2[] PROGMEM = "=\"0\" width=\"100%\" cellpadding=\"1\" style=\"font-family:Verdana;color:#fff";
prog_char htmlx3[] PROGMEM = "fff;font-size:12px;\"><tr><td>&nbsp PIXEL2LED Node Setup</td></tr></table><br>";
PROGMEM const char *string_table0[] = {htmlx0, htmlx1, htmlx2, htmlx3};

prog_char htmla0[] PROGMEM = "After pressing the REBOOT button, the controller will restart with the IP you set, so be carefull!";
prog_char htmla1[] PROGMEM = "<table><form><input type=\"hidden\" name=\"SBM\" value=\"1\"><tr><td>#################################";
prog_char htmla2[] PROGMEM = "</td></tr><tr><td>This are possible configuration, before you'll have package drops:</td></tr><tr><td>";
prog_char htmla3[] PROGMEM = "----descirption not finished. see print out in case.</td></tr><tr><td>";
prog_char htmla4[] PROGMEM = "30 fps: 5 out / 300 pixel (1500 px)</td></tr><tr><td>";
prog_char htmla5[] PROGMEM = "40 fps: 4 out / 300 pixel (1200 px)</td></tr><tr><td>";
prog_char htmla6[] PROGMEM = "25 fps: 3 out / 600 pixel (1800 px)</td></tr><tr><td>";
prog_char htmla7[] PROGMEM = "40 fps: 2 out / 600 pixel (1200 px)</td></tr><tr><td>";
PROGMEM const char *string_table1[] = {htmla0, htmla1, htmla2, htmla3, htmla4, htmla5, htmla6, htmla7};

prog_char htmlb0[] PROGMEM = "#######";
prog_char htmlb1[] PROGMEM = "########";
prog_char htmlb2[] PROGMEM = "##########";
prog_char htmlb3[] PROGMEM = "########</td></tr><tr><td>IP: <input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT7\" value=\"";
prog_char htmlb4[] PROGMEM = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT8\" value=\"";
prog_char htmlb5[] PROGMEM = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT9\" value=\"";
prog_char htmlb6[] PROGMEM = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT10\" value=\"";
PROGMEM const char *string_table2[] = {htmlb0, htmlb1, htmlb2, htmlb3, htmlb4, htmlb5, htmlb6};

prog_char htmlc0[] PROGMEM = "\">";
prog_char htmlc1[] PROGMEM = "</t";
prog_char htmlc2[] PROGMEM = "d>";
prog_char htmlc3[] PROGMEM = "</tr>";
PROGMEM const char *string_table3[] = {htmlc0, htmlc1, htmlc2, htmlc3};

prog_char htmld0[] PROGMEM = "<tr><td>Universe (0 - 32768): <input type=\"text\" size=\"5\" maxlength=\"5\" name=\"DT15\" value=\"";
prog_char htmld1[] PROGMEM = "\"></td></tr><tr><td>Outputs (1-5): <input type=\"text\" size=\"1\" maxlength=\"1\" name=\"DT16\" value=\"";
prog_char htmld2[] PROGMEM = "\"></td></tr><tr><td>Number of LED/Output: <input type=\"text\" size=\"4\" maxlength=\"4\" name=\"DT17\" value=\"";
prog_char htmld3[] PROGMEM = "\"></td></tr><tr><td>Reload setup page before changing values again!";
prog_char htmld4[] PROGMEM = "</td></tr><tr><td><br></td></tr><tr><td><input id=\"button1\"type=\"submit\" value=\"REBOOT\" ";
prog_char htmld5[] PROGMEM = "></td></tr></form></table></body></html>";
PROGMEM const char *string_table4[] = {htmld0, htmld1, htmld2, htmld3, htmld4, htmld5};

prog_char htmle0[] PROGMEM = "Onclick=\"document.getElementById('T2').value ";
prog_char htmle1[] PROGMEM = "= hex2num(document.getElementById('T1').value);";
prog_char htmle2[] PROGMEM = "document.getElementById('T4').value = hex2num(document.getElementById('T3').value);";
prog_char htmle3[] PROGMEM = "document.getElementById('T6').value = hex2num(document.getElementById('T5').value);";
prog_char htmle4[] PROGMEM = "document.getElementById('T8').value = hex2num(document.getElementById('T7').value);";
prog_char htmle5[] PROGMEM = "document.getElementById('T10').value = hex2num(document.getElementById('T9').value);";
prog_char htmle6[] PROGMEM = "document.getElementById('T12').value = hex2num(document.getElementById('T11').value);";
prog_char htmle7[] PROGMEM = "\"";
PROGMEM const char *string_table5[] = {htmle0, htmle1, htmle2, htmle3, htmle4, htmle5, htmle6, htmle7};

const byte ID = 0x92; //used to identify if valid data in EEPROM the "know" bit,
// if this is written in EEPROM the sketch has ran before
// We use this, so that the very first time you'll run this sketch it will use
// the values written above.
// defining which EEPROM address we are using for what data


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
  0, 0,                                 // Net (0-127) and subnet (0-15)
  "Pixel2LED#21",                           // Short name
  "Pixel2LED#21",                     // Long name
  num_artnet_ports, // Number of ports
  { PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput,
    PortTypeDmx | PortTypeOutput
  }, // Port types
  {0, 1, 2, 3},                         // Port input universes (0-15)
  {3, 3, 3, 3},                          // Port output universes (0-15)
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
// ArtNet Node
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void artNetNode() {
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
              int workaround=0;
              if (port >= 0 && port < config.numPorts) {
                 Serial.println(port);

                uint16_t portOffset = 0;

                for (int i = 0; i < port; i++) {
                  portOffset = portOffset + num_led_per_universe;
                  // skip to next universe, if the number of led per output is reached.
                  // like that, every output of the controller will start with dmx adress 1 on a new universe
                  int numLedOnOutput = portOffset - int(portOffset / hardware_num_led_per_output) * hardware_num_led_per_output; //subtstracts the outputs where the max number of led is reached. the result tells, how much left over data exists in the universe
                  if (numLedOnOutput == num_led_per_output - hardware_num_led_per_output) {
                    portOffset = portOffset - numLedOnOutput;
                  }
                }

                //write the dmx data to the Octo frame buffer
                uint32_t* dmxData = (uint32_t*) dmx->Data;
                for (int i = 0; i < 128; i++) {
                  int ledIndex=i+portOffset;
                  // the calculation for ledIndex goes higher then the num of leds. So we need to prevent the array to grow to big
                  if (ledIndex<num_leds){
                    leds[i + portOffset] = CRGB(dmxData[i]);
                  }
                }
                numUniUpdated++;
              }



              
              break;
            }

          // OpSync
          case 0x5200: {
            waitForNextArtNet=0;
            controlLedPause=!controlLedPause;
            receiveArtNet=true;
            Serial.println(fps);
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
              T_ArtAddress *address = (T_ArtAddress*)udp_buffer;

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
      } 
      
      else if (memcmp(header->ID, "Art-Ext", 8) == 0) {
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


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The Webinterface
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void webInterface() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    TextFinder  finder(client );
    Serial.println("Client connect");
    while (client.connected()) {
      if (client.available()) {
        //This part does all the text searching
        if ( finder.find("GET /") ) {
          // if you find the word "setup" continue looking for more
          // if you don't find that word, stop looking and go further
          // This way you can put your own webpage later on in the sketch
          if (finder.findUntil("setup", "\n\r")) {
            // if you find the word "SBM" continue looking for more
            // if you don't find that word, stop looking and go further
            // it means the SUBMIT button hasn't been pressed an nothing has
            // been submitted. Just go to the place where the setup page is
            // been build and show it in the client's browser.
            if (finder.findUntil("SBM", "\n\r")) {
              byte SET = finder.getValue();
              // Now while you are looking for the letters "DT", you'll have to remember
              // every number behind "DT" and put them in "val" and while doing so, check
              // for the according values and put those in mac, ip, subnet and gateway.
              while (finder.findUntil("DT", "\n\r")) {
                int val = finder.getValue();
                // if val from "DT" is between 1 and 6 the according value must be a MAC value.
                if (val >= 1 && val <= 6) {
                  mac[val - 1] = finder.getValue();
                }
                // if val from "DT" is between 7 and 10 the according value must be a IP value.
                if (val >= 7 && val <= 10) {
                  ip[val - 7] = finder.getValue();
                }
                // if val from "DT" is between 11 and 14 the according value must be a MASK value.
                if (val >= 11 && val <= 14) {
                  subnet[val - 11] = finder.getValue();
                }
                // if val from "DT" is 15 the according value must be the universe subnet
                if (val == 15)universe = finder.getValue();

                // if val from "DT" is 15 the according value must be the universe subnet
                if (val == 16)NUM_OF_OUTPUTS = finder.getValue();

                // if val from "DT" is 17 the according value must be the universe subnet
                if (val == 17)hardware_num_led_per_output = finder.getValue();

              }
              // Now that we got all the data, we can save it to EEPROM
              for (int i = 0 ; i < 6; i++) {
                EEPROM.write(i + 1, mac[i]);
              }
              for (int i = 0 ; i < 4; i++) {
                EEPROM.write(i + 7, ip[i]);
              }
              for (int i = 0 ; i < 4; i++) {
                EEPROM.write(i + 11, subnet[i]);
              }
              EEPROM.write(15, lowByte(universe));
              EEPROM.write(16, highByte(universe));
              EEPROM.write(17, NUM_OF_OUTPUTS);
              EEPROM.write(18, lowByte(hardware_num_led_per_output));
              EEPROM.write(19, highByte(hardware_num_led_per_output));

              /*
              for (int i = 0 ; i < 3; i++) {
                EEPROM.write(i + 16, gateway[i]);
              }
              */
              // EEPROM.write(15,55);
              Serial.println();
              Serial.print("num outputs: ");
              Serial.println(word(EEPROM.read(16), EEPROM.read(15)));

              // set ID to the known bit, so when you reset the Arduino is will use the EEPROM values
              EEPROM.write(0, 0x92);
              // if al the data has been written to EEPROM we should reset the arduino
              // for now you'll have to use the hardware reset button
              restart=true;
            }
            /*
             *
             * SETUP PAGE
             */
            // and from this point on, we can start building our setup page
            // and show it in the client's browser.
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println();
            for (int i = 0; i < 4; i++)
            {
              strcpy_P(buffer, (char*)pgm_read_dword(&(string_table0[i])));
              client.print( buffer );
            }
            for (int i = 0; i < 3; i++)
            {
              strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[i])));
              client.print( buffer );
            }

            //   client.print(mac[0], HEX);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[3])));
            client.print( buffer );
            //  client.print(mac[1], HEX);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[4])));
            client.print( buffer );
            //   client.print(mac[2], HEX);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[5])));
            client.print( buffer );
            //    client.print(mac[3], HEX);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[6])));
            client.print( buffer );
            //   client.print(mac[4], HEX);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table1[7])));
            client.print( buffer );
            //   client.print(mac[5], HEX);
            for (int i = 0; i < 4; i++)
            {
              strcpy_P(buffer, (char*)pgm_read_dword(&(string_table2[i])));
              client.print( buffer );
            }

            client.print(ip[0], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table2[4])));
            client.print( buffer );
            client.print(ip[1], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table2[5])));
            client.print( buffer );
            client.print(ip[2], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table2[6])));
            client.print( buffer );
            client.print(ip[3], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table3[0])));
            client.print( buffer );
            //      client.print(subnet[0], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table3[1])));
            client.print( buffer );
            //      client.print(subnet[1], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table3[2])));
            client.print( buffer );
            //       client.print(subnet[2], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table3[3])));
            client.print( buffer );
            //    client.print(subnet[3], DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[0])));
            client.print( buffer );
            client.print(universe, DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[1])));
            client.print( buffer );
            client.print(NUM_OF_OUTPUTS, DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[2])));
            client.print( buffer );
            client.print(hardware_num_led_per_output, DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[3])));
            client.print( buffer );
            //    client.print(REFRESH_RATE_KHZ, DEC);
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[4])));
            client.print( buffer );
            for (int i = 0; i < 8; i++)
            {
              strcpy_P(buffer, (char*)pgm_read_dword(&(string_table5[i])));
              client.print( buffer );
            }
            strcpy_P(buffer, (char*)pgm_read_dword(&(string_table4[5])));
            client.print( buffer );
            break;
          }
        }
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        // put your own html from here on
        client.print("PIXEL2LED<br />");
        client.print("controler nr ");
        client.print(controllerID);
        client.print("<br /> for configuration go to: ");
        client.print(ip[0], DEC);
        for (int i = 1; i < 4; i++) {
          client.print(".");
          client.print(ip[i], DEC);
        }
        client.print("/setup");
        // put your own html until here
        break;
      }


    }
    delay(1);
    client.stop();
    Serial.println("Client disconnect");
    if(restart)WRITE_RESTART(0x5FA0004); //call reset;
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// setup
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
delay(1000);
pinMode(9, OUTPUT);
digitalWrite(9, LOW); // reset the WIZ820io
pinMode(10, OUTPUT);
digitalWrite(10, HIGH); //de-select WIZ820io
pinMode(4, OUTPUT);
digitalWrite(4, HIGH); //de-selöect SD-card
delay(1000);
digitalWrite(9, HIGH); // release the WIZ820io

pinMode(redLed, OUTPUT); // the red LED
digitalWrite(redLed, HIGH); 

  // get the controller ID (as printed on the back). It's used to generate the normal IP Adress with 2.2.2.ID
  controllerID = EEPROM.read(20);
  String nameConstructor = String("Pixel2LED#" + String(controllerID));
  char controllerName[13];
  nameConstructor.toCharArray(controllerName, 13);
  strcpy(config.longName, controllerName);
  strcpy(config.shortName, controllerName);



  // check if there is already a saved configuration on EEPROM
  int idcheck =  EEPROM.read(0);

  // No config found:
  if (idcheck != ID) {

    Serial.println("NO config found");

    // generate Mac adress
    mac_addr mac_generated;
    for (int i = 3; i < 6; i++) {
      config.mac[i] = mac_generated.m[i];
    }

    // generate IP address
    for (int i = 0; i < 3; i++) {
      ip[i] = 2;
    }
    ip[3] = controllerID;

    //save the IP in the eeprom
    for (int i = 0 ; i < 4; i++) {
      EEPROM.write(i + 7, ip[i]);
    }

    // Open Ethernet connection
    IPAddress gateway(config.ip[0], 0, 0, 1);
    IPAddress subnet(255, 0, 0, 0);
    Ethernet.begin(config.mac, ip,  gateway, gateway, subnet);

    NUM_OF_OUTPUTS = 5;
    hardware_num_led_per_output = 300;
    //ifcheck id is not the value as const byte ID,
    //it means this sketch has NOT been used to setup the shield before
    //just use the values written in the beginning of the sketch
  }

  // Config found:
  if (idcheck == ID) {
    Serial.println("config found");

    // Generate MAC address
    mac_addr mac_generated;
    for (int i = 3; i < 6; i++) {
      config.mac[i] = mac_generated.m[i];
    }

    for (int i = 0; i < 4; i++) {
      ip[i] = EEPROM.read(i + 7);
    }
    universe = int(word(EEPROM.read(16), EEPROM.read(15)));
    hardware_num_led_per_output = int(word(EEPROM.read(19), EEPROM.read(18)));
    NUM_OF_OUTPUTS = EEPROM.read(17);

    Ethernet.begin(config.mac, ip, gateway, gateway, subnet);
  }




  num_channel_per_output = hardware_num_led_per_output * NUM_CHANNEL_PER_LED;
  num_universes_per_output = (num_channel_per_output % 512) ? num_channel_per_output / 512 + 1 : num_channel_per_output / 512;
  num_led_per_output = num_universes_per_output * 512 / NUM_CHANNEL_PER_LED;
  num_artnet_ports = num_universes_per_output * NUM_OF_OUTPUTS;
  num_led_per_universe = int(512 / NUM_CHANNEL_PER_LED);

  num_leds = NUM_OF_OUTPUTS * hardware_num_led_per_output;
  leds = new CRGB[NUM_OF_OUTPUTS * hardware_num_led_per_output]; 

  config.numPorts = num_artnet_ports;


  byte net = universe / 256;
  byte subnet = (universe - net * 256) / 16;
  byte port = universe - subnet * 16 - net * 256;

  //config artnet adrees (universe)
  config.net = net;
  config.subnet = subnet;

  for (int i = 0; i < 4; i++) {
    config.portAddrOut[i] = port;
  }

  // Setup FastLed
   if (NUM_OF_OUTPUTS > 0)FastLED.addLeds<LED_TYPE, 14, 7, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 0, hardware_num_led_per_output);
  if (NUM_OF_OUTPUTS > 1)FastLED.addLeds<LED_TYPE, 22, 21, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, hardware_num_led_per_output, hardware_num_led_per_output);
  if (NUM_OF_OUTPUTS > 2)FastLED.addLeds<LED_TYPE, 6, 5, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 2 * hardware_num_led_per_output, hardware_num_led_per_output);
  if (NUM_OF_OUTPUTS > 3)FastLED.addLeds<LED_TYPE, 16, 15, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 3 * hardware_num_led_per_output, hardware_num_led_per_output);
  if (NUM_OF_OUTPUTS > 4)FastLED.addLeds<LED_TYPE, 18, 17, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 4 * hardware_num_led_per_output, hardware_num_led_per_output);
  if (NUM_OF_OUTPUTS > 5)FastLED.addLeds<LED_TYPE, 20, 19, COLOR_ORDER, DATA_RATE_KHZ(REFRESH_RATE_KHZ)>(leds, 5 * hardware_num_led_per_output, hardware_num_led_per_output);
  FastLED.setDither(1);

  blink();

  server.begin();
    Serial.println(nameConstructor);
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  
  udp.begin(config.udpPort);
  // Open ArtNet
  node = ArtNodeExtended(config, sizeof(udp_buffer), udp_buffer);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// main loop
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  artNetNode();
  webInterface();

  if(receiveArtNet && controlLedPause)digitalWrite(redLed, LOW);
  else digitalWrite(redLed, HIGH);
}
