/*
    Lighting controller for Anita's Dropship
    http://dropship.github.io

    No warranties and stuff.

    (c) 2014 Alex Southgate, Carlos Mogollan, Kasima Tharnpipitchai
*/

#include <Adafruit_CC3000.h>
#include <Adafruit_NeoPixel.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include <utility/socket.h>
#include <utility/debug.h>
#include <MemoryFree.h>

#include "config.h"




/**** ADAFRUIT CC3000 CONFIG ****/

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(
  ADAFRUIT_CC3000_CS,
  ADAFRUIT_CC3000_IRQ,
  ADAFRUIT_CC3000_VBAT,
  SPI_CLOCK_DIVIDER); // you can change this clock speed




/**** NETWORKING CONFIG ****/

#define LISTEN_PORT 9000 // where Dropship is broadcsting events

const unsigned long
  dhcpTimeout     = 60L * 1000L, // Max time to wait for address from DHCP
  connectTimeout  = 15L * 1000L, // Max time to wait for server connection
  responseTimeout = 15L * 1000L; // Max time to wait for data from server

bool connected = false;

// UDP Socket variables
unsigned long listen_socket;

sockaddr from;
socklen_t fromlen = 8;

char rx_packet_buffer[256];
unsigned long recvDataLen;




/**** DROPSHIP PROTOCOL ****/

// Structures for event name lookup
#define CONTROL     0
#define KICK        1
#define SNARE       2
#define WOBBLE      3
#define SIREN       4
char* event_names[5];
uint32_t led_values[5];

#define POST_DROP -1
#define AMBIENT    0
#define BUILD      1
#define DROP_ZONE  2
#define PRE_DROP   3
#define DROP       4
int current_drop_state = AMBIENT;

unsigned long previousMillis = millis();
long interval = 10; // Refresh every 10ms.




/**** NEOPIXEL CONFIG *****/

#define NEOPIXEL_PIN 6
#define SIZE(x)  (sizeof(x) / sizeof(x[0]))

// Initialize neopixel
Adafruit_NeoPixel strip = Adafruit_NeoPixel(150, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
uint32_t white = strip.Color(255, 255, 255);
uint32_t black = strip.Color(0, 0, 0);
uint32_t red   = strip.Color(255, 0, 0);
uint32_t palette_1[5];

void define_palettes() {
  palette_1[CONTROL] = white;
  palette_1[KICK]    = strip.Color(19, 95, 255);
  palette_1[SNARE]   = strip.Color(139, 255, 32);
  palette_1[WOBBLE]  = strip.Color(255, 77, 32);
  palette_1[SIREN]   = strip.Color(255, 0, 255); // Magenta
}




/**** MAIN PROGRAM ****/

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!"));
  setupNetworking();
  Serial.print("Hello!");

  // Setup event lookup structures
  event_names[CONTROL] = "nop";
  event_names[KICK]    = "kick";
  event_names[SNARE]   = "snare";
  event_names[WOBBLE]  = "wobble";
  event_names[SIREN]   = "siren";

  setupNeoPixel();
}


int loop_count = 0;
int strobe_switch = 0;
uint32_t color;

void loop(void) {
  int rcvlen;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    loop_count++;

    if (loop_count % 100 == 0) {
      Serial.print(F("Alive for "));
      Serial.print(loop_count / 100);
      Serial.println(F(" seconds"));
    }

    // change lighting state every [interval] milliseconds
    repaintLights();
  }

  // Receive events
  rcvlen = recvfrom(listen_socket, rx_packet_buffer, 255, 0, &from, &fromlen);

  if (rcvlen > 0) {
    parse_events(rx_packet_buffer);
    memset(rx_packet_buffer, 0, 256);
  }
}




/***
 * This function is called whenever an event is received.
 *
 * CUSTOMIZE LIGHTING RESPONSE TO EVENTS BY REWRITING THIS FUNCTION.
 ***/
void handle_event(char* event_name, float event_value, int drop_state,
                  float build, float lcrank, float rcrank) {
  current_drop_state = drop_state;

  if (strcmp(event_name, "nop") == 0) {
    return;
  }

  for (int i = 0; i < SIZE(event_names); i++) {
    if (strcmp(event_names[i], event_name) == 0) {
      uint32_t color = led_values[i];

      if (current_drop_state == DROP) {
        if (strcmp("wobble", event_name) == 0) {
          color = fade_color(color, event_value);
          setAllColor(color, 5);
        }
      } else {
        setAllColor(color);
      }
    }
  }
}




/**** NEOPIXEL ****/

void setupNeoPixel() {
  define_palettes();
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  uint32_t* palette = palette_1;

  led_values[CONTROL] = palette[CONTROL];
  led_values[KICK]    = palette[KICK]; // Purple
  led_values[SNARE]   = palette[SNARE]; // Yellow
  led_values[WOBBLE]  = palette[WOBBLE];
  led_values[SIREN]   = palette[SIREN];
}

uint32_t fade_color(uint32_t color, float fade) {
  uint8_t r, g, b;

  r = (color >> 16);
  g = (color >> 8);
  b = color;

  r *= fade;
  g *= fade;
  b *= fade;

  return strip.Color(r, g, b);
}


// Fill the dots one after the other with a color
void setAllColor(uint32_t c) {
  setAllColor(c, 99999999);
}


// Fill the dots one after the other with a color, except every nth dot.
void setAllColor(uint32_t c, int except) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    if (!((i+1) % except == 0)) {
      strip.setPixelColor(i, c);
    }
  }
  strip.show();
}

// Fill the dots one after the other with a color, except every nth pixel.
void setAllColorOnly(uint32_t c, int only) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    if (((i+1) % only == 0)) {
      strip.setPixelColor(i, c);
    }
  }
  strip.show();
}


uint32_t strobe_color = black;
void repaintLights() {
  // Different drop-state animation loops
  if (current_drop_state == DROP) {
    if (loop_count % 64 < 16) {
      strobe_switch = 1;
    } else {
      strobe_switch = 0;
      strobe_color = black;
      setAllColorOnly(strobe_color, 5);
    }

    // Strobe every 5th light
    if (strobe_switch) {
      if (loop_count % 4 == 0) {
        if (strobe_color == white) {
          strobe_color = black;
        } else {
          strobe_color = white;
        }
        setAllColorOnly(strobe_color, 5);
      }
    }
  }
  else if (current_drop_state == AMBIENT ||
           current_drop_state == BUILD ||
           current_drop_state == DROP_ZONE) {
    // Fade out all valuess
    color = strip.getPixelColor(0);
    setAllColor(fade_color(color, 0.9));
  }
  strip.show();
}




/**** DROPSHIP EVENT HANDLING ****/

void parse_events(char* packet) {
  char* message;
  int count = 0;
  while ((message = strtok_r(packet, "$", &packet)) != NULL) {
    count++;
    if (count >= 2) {
      Serial.print(count);
      Serial.println(F("x-message"));
    }

    parse_message(message);
  }
}


void parse_message(char* message) {
  // <SEQ>,<EVENT>,<VALUE>,<DROP_STATE>,<BUILD>,<LCRANK>,<RCRANK>

  /*Serial.print(F("  event:"));*/
  /*Serial.println(message);*/

  char* event_name  = strtok_r(message, ",", &message);
  float event_value = atof(strtok_r(message, ",", &message));
  int   drop_state  = atoi(strtok_r(message, ",", &message));
  float build       = atof(strtok_r(message, ",", &message));
  float lcrank      = atof(strtok_r(message, ",", &message));
  float rcrank      = atof(strtok_r(message, ",", &message));

  handle_event(event_name, event_value, drop_state, build, lcrank, rcrank);
}





/**** NETWORKING ****/

bool displayConnectionDetails(void) {
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

  if (!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv)) {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  } else {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}


uint32_t getIPAddress(void) {
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv)) {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
  } else {
    return ipAddress;
  }
}


void setupNetworking(void) {
  long optvalue_block = SOCK_ON;

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);

  Serial.print(F("Initializing..."));
  if(!cc3000.begin()) {
    Serial.println(F("failed. Check your wiring?"));
    return;
  }

  Serial.print(F("OK.\r\nConnecting to network..."));
  if(!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    return;
  }
  Serial.println(F("connected!"));

  Serial.print(F("Requesting address from DHCP server..."));
  for(int t=millis(); !cc3000.checkDHCP() && ((millis() - t) < dhcpTimeout); delay(1000));
  if(cc3000.checkDHCP()) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("failed"));
    return;
  }

  while(!displayConnectionDetails()) delay(1000);

  // An IP4 address (can be cast to a sockaddr)
  sockaddr_in ip4_address;

  // Set the destination port
  ip4_address.sin_port = htons(LISTEN_PORT);
  ip4_address.sin_family = AF_INET;

  // Listen on any network interface
  ip4_address.sin_addr.s_addr = htonl(0);

  listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (setsockopt(listen_socket, SOL_SOCKET, SOCKOPT_RECV_NONBLOCK, &optvalue_block, sizeof(long)) != 0)
  {
    Serial.println(F("Error setting non-blocking mode on socket."));
  }

  if (bind(listen_socket, (sockaddr*) &ip4_address, sizeof(sockaddr)) < 0) {
    Serial.println(F("Error binding listen socket to address!"));
    return;
  }

  Serial.print(F("Socket bound "));
  cc3000.printIPdotsRev(getIPAddress());
  Serial.print(F(":"));
  Serial.print(String(LISTEN_PORT, DEC));
}
