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
#define CC3000_BUFFER_SIZE    512

// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(
  ADAFRUIT_CC3000_CS,
  ADAFRUIT_CC3000_IRQ,
  ADAFRUIT_CC3000_VBAT,
  SPI_CLOCK_DIVIDER); // you can change this clock speed





/**** NETWORKING CONFIG ****/

#define LISTEN_PORT 9000 // where Dropship is broadcsting events

const unsigned long dhcpTimeout = 60L * 1000L; // Max time to wait for address from DHCP

bool connected = false;

// UDP Socket variables
unsigned long listen_socket;

sockaddr from;
socklen_t fromlen = 8;

char rx_packet_buffer[CC3000_BUFFER_SIZE];
unsigned long recvDataLen;

/**** DROPSHIP PROTOCOL ****/

// Structures for event name lookup
#define CONTROL     0
#define KICK        1
#define SNARE       2
#define WOBBLE      3
#define PRE_DROP    4
#define CHORD       5
char* event_names[6];
uint32_t led_values[6];

#define POST_DROP -1
#define AMBIENT    0
#define BUILD      1
#define DROP_ZONE  2
#define PRE_DROP   3
#define DROP       4
int current_drop_state = AMBIENT;

unsigned long last_painted = millis();

#define THROB_INTENSITY_MIN 0.02
#define THROB_INTENSITY_MAX 0.95
#define THROB_SPEED 0.02

/**** NEOPIXEL CONFIG *****/
#define SIZE(x)  (sizeof(x) / sizeof(x[0]))
#define NEOPIXEL_PIN 6

#define LED_REFRESH 10
#define STROBE_NTH 10
#define STRIP_1_LENGTH 150

// Initialize neopixel
Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIP_1_LENGTH, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
uint32_t white = strip.Color(255, 255, 255);
uint32_t black = strip.Color(0, 0, 0);
uint32_t red   = strip.Color(255, 0, 0);
uint32_t blue  = strip.Color(0, 0, 255);

uint32_t palette[5];

void define_palettes() {
  palette[CONTROL]  = strip.Color(164, 123, 230); // Pink
  palette[KICK]     = strip.Color(19, 95, 255);
  palette[SNARE]    = strip.Color(139, 255, 32);
  palette[CHORD]    = strip.Color(255, 0, 255); // Magenta
  palette[WOBBLE]   = strip.Color(255, 33, 33);
  palette[PRE_DROP] = strip.Color(164, 123, 230);
}




/**** MAIN PROGRAM ****/

void setup(void) {
  Serial.begin(115200);
  setupNetworking();

  // Setup event lookup structures
  event_names[CONTROL] = "control";
  event_names[KICK]    = "kick";
  event_names[SNARE]   = "snare";
  event_names[CHORD]   = "chord";
  event_names[WOBBLE]  = "wobble";

  setupNeoPixel();
  reset_throb();
}


uint16_t loop_count = 0;
int strobe_switch = 0;
uint32_t color;
uint16_t handled_events = 0;
uint16_t received_events = 0;
unsigned long now;
unsigned long last_received_event = millis();
float throb_direction;
float throb_intensity;

void loop(void) {
  now = millis();

  // Repain lights every LED_REFRESH milliseconds
  if (should_repaint()) {
    last_painted = now;
    loop_count += 1;

    if (loop_count % (100 / LED_REFRESH) == 0) {
      Serial.print(loop_count / (100 / LED_REFRESH));
      Serial.print(" : ");
      Serial.print(received_events);
      Serial.print(" : ");
      Serial.print(handled_events);
      Serial.println(" events");
      handled_events = 0;
      received_events = 0;
    }

    repaintLights();
  }

  receive_events();
}

int should_repaint(void) {
  return (now - last_painted > LED_REFRESH);
}

void receive_events(void) {
  // Receive events
  int rcvlen = recv(listen_socket, rx_packet_buffer, CC3000_BUFFER_SIZE - 1, 0);

  if (rcvlen > 0) {
    last_received_event = millis();
    received_events += 1;
    parse_events(rx_packet_buffer);
    memset(rx_packet_buffer, 0, CC3000_BUFFER_SIZE);
  }
}


/**** NEOPIXEL ****/

void setupNeoPixel() {
  define_palettes();

  led_values[CONTROL]  = palette[CONTROL];
  led_values[KICK]     = palette[KICK]; // Purple
  led_values[SNARE]    = palette[SNARE]; // Yellow
  led_values[WOBBLE]   = palette[WOBBLE];
  led_values[CHORD]    = palette[CHORD];
  led_values[PRE_DROP] = palette[PRE_DROP];

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  light_check();
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

// Cycle through all the lights in the strip
void light_check() {
  int pixels = strip.numPixels();

  for(uint16_t i=0; i<strip.numPixels(); i+=1) {
    strip.setPixelColor(i, led_values[CONTROL]);
    strip.setPixelColor(pixels - i, led_values[CONTROL]);
    strip.show();
    strip.setPixelColor(i, black);
    strip.setPixelColor(pixels - i, black);
  }
}

// Fill the dots one after the other with a color
void setAllColor(uint32_t c) {
  setAllColor(c, 99999999);
}


// Fill the dots one after the other with a color, except every nth pixel.
void setAllColor(uint32_t c, int except) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    if (!((i+1) % except == 0)) {
      strip.setPixelColor(i, c);
    }
  }
}

// Fill the dots one after the other with a color, but only every nth pixel.
void setNthColor(uint32_t c, int only) {
  for(uint16_t i=(only - 1); i<strip.numPixels(); i += only) {
    strip.setPixelColor(i, c);
  }
}


void repaintLights() {
  // Different drop-state animation loops

  if (last_received_event < (now - 5000)) {
    strobe_random_pixel();
    throb_all_pixels(blue);
  } else {
    reset_throb();
    if (current_drop_state == DROP) {
      strobe_random_pixel();
    }
    else if (current_drop_state == PRE_DROP) {
      for (int i = 0; i < strip.numPixels(); i += 50) {
        strip.setPixelColor((i + loop_count) % strip.numPixels(), palette[PRE_DROP]);
      }
      fade_all_pixels();
    }
    else if (current_drop_state == AMBIENT ||
             current_drop_state == BUILD ||
             current_drop_state == DROP_ZONE) {
      fade_all_pixels();
    }
  }

  strip.show();
}

void fade_all_pixels() {
  // Fade out all values
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    color = strip.getPixelColor(i);
    strip.setPixelColor(i, fade_color(color, 0.9));
  }
}

void throb_all_pixels(uint32_t color) {
  if (throb_intensity <= THROB_INTENSITY_MIN) {
    throb_direction = 1.0;
  } else if (throb_intensity >= THROB_INTENSITY_MAX) {
    throb_direction = -1.0;
  }
  throb_intensity = throb_intensity * (1.0 + THROB_SPEED * throb_direction);
  setAllColor(fade_color(color, throb_intensity), STROBE_NTH);
}

void reset_throb() {
  throb_intensity = THROB_INTENSITY_MIN;
  throb_direction = 1.0;
}

/**
  Strobes a random Nth pixel for the DROP effect.
*/
uint32_t strobe_color = white;
unsigned long strobe_pixel;
void strobe_random_pixel() {
  if (loop_count % 64 == 0) {
    // Choose pixel to strobe
    strobe_pixel = (random(strip.numPixels() / STROBE_NTH) * STROBE_NTH) - 1;
  }
  if (loop_count % 48 < 16) {
    // Enable strobing for 16 loops
    strobe_switch = 1;
  }
  else {
    // Disable strobing for 32 loops
    strobe_switch = 0;
    // Wipe any strobes
    setNthColor(black, STROBE_NTH);
  }

  if (strobe_switch && loop_count % 4 == 0) {
    if (strobe_color == white) {
      strobe_color = black;
    } else {
      strobe_color = white;
    }
    strip.setPixelColor(strobe_pixel, strobe_color);
  }
}


/**** DROPSHIP EVENT HANDLING ****/

void parse_events(char* packet) {
  char* message;

  int count = 0;
  while ((message = strtok_r(packet, "$", &packet)) != NULL) {
    /*count++;
    if (count >= 2) {
      Serial.print(count);
      Serial.println(F("x-message"));
    }*/
    parse_message(message);
  }
}


void parse_message(char* message) {
  // <SEQ>,<EVENT>,<VALUE>,<DROP_STATE>,<BUILD>,<LCRANK>,<RCRANK>

  char* event_name  = strtok_r(message, ",", &message);
  float event_value = atof(strtok_r(message, ",", &message));
  int   drop_state  = atoi(strtok_r(message, ",", &message));
  float build       = atof(strtok_r(message, ",", &message));
  float lcrank      = atof(strtok_r(message, ",", &message));
  float rcrank      = atof(strtok_r(message, ",", &message));

  handle_event(event_name, event_value, drop_state, build, lcrank, rcrank);
}


/***
 * This function is called whenever an event is received.
 *
 * CUSTOMIZE LIGHTING RESPONSE TO EVENTS BY REWRITING THIS FUNCTION.
 ***/
void handle_event(char* event_name, float event_value, int drop_state,
                  float build, float lcrank, float rcrank) {
  unsigned long now = millis();
  int previous_drop_state = current_drop_state;
  current_drop_state = drop_state;

  handled_events += 1;

  // Ignore nops
  if (strcmp(event_name, "nop") == 0) { return; }

  // Test control event
  if (strcmp(event_name, "control") == 0) {
    light_check();
    return;
  }

  // Wipe the slate when switching to PRE_DROP
  if ((drop_state == PRE_DROP && previous_drop_state != PRE_DROP) ||
      (drop_state == DROP && previous_drop_state != DROP)) {
    setAllColor(black);
  }

  for (int i = 0; i < SIZE(event_names); i++) {
    if (strcmp(event_names[i], event_name) == 0) {
      uint32_t color = led_values[i];

      if (current_drop_state == DROP) {
        // DROP state: wobble and strobe
        if (strcmp("wobble", event_name) == 0) {
          color = fade_color(color, event_value);
          // on DROP this can get called every 1ms which causes a hang
          setAllColor(color, STROBE_NTH);
        }
      } else {
        // non-DROP state: paint chords, kicks and snares
        if (strcmp("chord", event_name) == 0) {
          int n = ((int) (event_value * 5));
          setNthColor(color, n);
        } else {
          setAllColor(color);
        }
      }
    }
  }
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
  Serial.println("Setting up networking...");

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);

  Serial.print(F("Initializing..."));
  if(!cc3000.begin()) {
    Serial.println(F("failed. Check your wiring?"));
    return;
  }

  displayFirmwareVersion();
  displayDriverMode();

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

  if (setsockopt(listen_socket, SOL_SOCKET, SOCKOPT_RECV_NONBLOCK, &optvalue_block, sizeof(long)) != 0) {
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

void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER
    Serial.println(F("CC3000 is configure in 'Tiny' mode"));
  #else
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

void displayFirmwareVersion(void)
{
  #ifndef CC3000_TINY_DRIVER
  uint8_t major, minor;

  if(!cc3000.getFirmwareVersion(&major, &minor))
  {
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
  }
  else
  {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
  }
  #endif
}
