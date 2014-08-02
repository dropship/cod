// Listens for JSON messages via UDP

#include <Adafruit_CC3000.h>
#include <Adafruit_NeoPixel.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include <utility/socket.h>
#include <utility/debug.h>
#include <MemoryFree.h>
#include "config.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

#define LISTEN_PORT 9000
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11

#define NEOPIXEL_PIN 6
#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))

Adafruit_CC3000 cc3000 = Adafruit_CC3000(
  ADAFRUIT_CC3000_CS,
  ADAFRUIT_CC3000_IRQ,
  ADAFRUIT_CC3000_VBAT,
  SPI_CLOCK_DIVIDER); // you can change this clock speed

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


// Structures for event name lookup
#define CONTROL     0
#define KICK        1
#define SNARE       2
#define WOBBLE      3
#define SIREN       4

char* event_names[5];
uint32_t led_values[5];


unsigned long previousMillis = millis();
long interval = 10; // Refresh every 10ms.

// Initialize neopixel
Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!"));
  setupNetworking();
  Serial.print("Hello!");

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'


  // Setup event lookup structures
  event_names[CONTROL] = "nop";
  event_names[KICK]    = "kick";
  event_names[SNARE]   = "snare";
  event_names[WOBBLE]  = "wobble";
  event_names[SIREN]   = "siren";

  led_values[CONTROL] = strip.Color(255, 255, 255); // White
  led_values[KICK]    = strip.Color(255, 0, 0); // Red
  led_values[SNARE]   = strip.Color(0, 255, 0); // Green
  led_values[WOBBLE]  = strip.Color(0, 0, 255); // Blue
  led_values[SIREN]   = strip.Color(255, 0, 255); // Purple
}

void loop(void) {
  int rcvlen, i = 0;
  uint8_t r, g, b;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;

    // Fade out all valuess
    for (i=0;i<strip.numPixels();i++) {
      r = (strip.getPixelColor(i) >> 16);
      g = (strip.getPixelColor(i) >>  8);
      b = (strip.getPixelColor(i)      );

      r *= 0.90;
      g *= 0.90;
      b *= 0.90;

      strip.setPixelColor(i, r, g, b);
    }
    strip.show();

    rcvlen = recvfrom(listen_socket, rx_packet_buffer, 255, 0, &from, &fromlen);

    if (rcvlen > 0) {
      parse_events(rx_packet_buffer);
      memset(rx_packet_buffer, 0, 256);
    }
  }
}

void parse_events(char* packet) {
  char* message;
  int count = 0;
  while ((message = strtok_r(packet, "$", &packet)) != NULL) {
    count++;
    if (count == 2) {
      Serial.println(F("multi-message"));
    }

    parse_message(message);
  }
}

void parse_message(char* message) {
  // <SEQ>,<EVENT>,<VALUE>,<DROP_STATE>,<BUILD>,<LCRANK>,<RCRANK>
  /*char* sequence    = strtok_r(message, ",", &message);*/
  char* event_name  = strtok_r(message, ",", &message);
  char* event_value = strtok_r(message, ",", &message);
  char* drop_state  = strtok_r(message, ",", &message);
  char* build       = strtok_r(message, ",", &message);
  char* lcrank      = strtok_r(message, ",", &message);
  char* rcrank      = strtok_r(message, ",", &message);

  Serial.print(F("  event:"));
  Serial.println(event_name);

  handle_event(event_name, event_value, drop_state, build, lcrank, rcrank);
}

void handle_event(char* event_name, char* event_value, char* drop_state,
                  char* build, char* lcrank, char* rcrank) {
  if (strcmp(event_name, "nop") == 0) {
  }
  else {
    for (int i = 0; i < NELEMS(event_names); i++) {
      if (strcmp(event_names[i], event_name) == 0) {
        uint32_t color = led_values[i];
        setAllColor(color);
      }
    }
  }
}

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

// Fill the dots one after the other with a color
void setAllColor(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}
