// Decode-O-Matic
//
// Decode-O-Matic v0.1 20 July 2019
// To compile this code choose board "WeMos D1 R1"

// Original code by:  Nick Gammon
// Date:    11th April 2012.

// Version 1.4: 2023-03-15 Jos Bouten
// This program uses 9 LEDs to show on which channel a midi message it received.
// Led 0 .. 7 show channel 1 ... 8. When data on higher channels is received the 9th 
// led will be lit. The leds are activated immediately but will be deactivated 
// after a delay so that even short messages can be traced.
// The program collects the messages in a buffer and as soon as that is (nearly) filled
// the data is transfered via ESP_NOW to another ESP device.
// This communication is non blocking, so the decoder works without a receiving ESP device.
// The MAC address of the receiving ESP device is used to address it.
// You will need to set that to the proper value would you want to use this code.
// No WiFi router etc. is necessary, the devices communicate with each other
// in a direct way using ESP_NOW.

// Note the debug flag must not be defined during operation otherwise 
// Q9 will not be properly defined. As a consequence the HIGHCHANNEL 
// led will always be lit.

//#define DEBUG

#include <MIDI.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <LibPrintf.h>

const char version[] = "Decode-O-Matic v1.4";

// REPLACE WITH RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//
// define LEDs
//

/*
FLTR <number used in IDE> // <integer value> i.e. <name of pin(s) on silkscreen of wemos pcb>
D9 //  1 i.e.  D3 on wemos D1 R1
D8 //  0 i.e. D10 on wemos D1 R1
D7 // 13 i.e.  D7 == D11 on wemos D1 R1
D6 // 12 i.e.  D6 == D12 on wemos D1 R1
D5 // 14 i.e.  D5 == D13 on wemos D1 R1 = blue SCK LED
D4 //  4 i.e.  D9 (dus niet D4!).
D3 //  5 i.e.  D8 (dus niet D3!).
D2 // 16 i.e. D14 (dus niet D2!).
D1 //  1 i.e. D15 (dus niet D1!).
D0 //  3 i.e.  D2 on wemos D1 R1
A0 // no responding pin found
*/

int Q0 = D10; // 15 i.e. 1st LED
int Q1 = D2;  // 16 i.e. 2nd LED
int Q2 = D14; //  4 i.e. 3rd LED
int Q3 = D8;  //  0 i.e. 4th LED
int Q4 = D9;  //  2 i.e. 5th LED
int Q5 = D5;  // 14 i.e. 6th LED
int Q6 = D6;  // 12 i.e. 7th LED
int Q7 = D7;  // 13 i.e. 8th LED
// The 9th LED is hard wired to the midi input.
#ifdef DEBUG
  int Q9 = D7; // replacement for debug purposes
#else
  int Q9 = D1;  //  4 i.e. 10th LED
#endif

/* ----------------------------------

Warning: Using D0 and D1 on the Wemos D1 R1 implies
that you cannot upload code or use a serial terminal
to look at any printed output as long as they are connected
to the IO-devices.
Only if you disconnect D0 you can upload code.
If you disconnect D1 and do not redefine its pin setting 
only then will the serial output be visible in the serial out.
Using the "#define DEBUG" So you could temporarily redefine Q9 to get rid of the problem.

   ---------------------------------- */

int BUTTON = Q9;
int HIGHCHANNEL = Q9;

int LED[9] = { Q0, Q1, Q7, Q6, Q5, Q4, Q3, Q2, Q9};

#define NR_COUNTERS 16
#define MAX_COUNT 40
volatile int delayCounter[NR_COUNTERS]; // Used to switch leds off after a delay.

// These defines must be placed at the beginning before #include "ESP8266TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     1

// Select a Timer Clock
#define USING_TIM_DIV1                false           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              true            // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"

volatile uint32_t lastMillis = 0;
volatile uint8_t brightnessValue = 21;
volatile uint8_t delta = 1;

#define TIMER_INTERVAL_MS  1000

// Init ESP8266 timer 1
ESP8266Timer ITimer;

void IRAM_ATTR TimerHandler() {
  bool atLeastOne = false;
  for (byte i = 0; i < NR_COUNTERS; i++) {  
    if (i > 7) {
      if (delayCounter[i] > 0) {
        digitalWrite(LED[i % 8] , HIGH);
        digitalWrite(LED[8], HIGH); // high channel led
        atLeastOne = true;
      }
    } 
    else {
      digitalWrite(LED[i] , delayCounter[i] > 0 ? HIGH: LOW);
    }
    if (delayCounter[i] > 0) {
      delayCounter[i]--;
    }
  }
  if (!atLeastOne)
    digitalWrite(LED[8], LOW);      
}

//=======================================================================
// End timer stuff.

#define MAX_NR_BYTES 60 // MUST be <- 250!

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  byte size;
  char bytes[MAX_NR_BYTES];
} struct_message;

struct_message midiData;
int counter = 1;

const int noRunningStatus = -1;
int runningStatus;
byte lastCommand;


void ledSetup() {
  for (int i = 0; i < 9; i++) {
    pinMode(LED[i], OUTPUT);
    digitalWrite(LED[i], LOW);
  }
}

#define INCREMENT 5 // 'Time' added to delay timer each time a message for a given channel is received.

void channelLedOn(int channel) {
  if (delayCounter[channel] < MAX_COUNT) {
    delayCounter[channel] += INCREMENT; 
  }
}

void channelLedOff(int channel) {
  delayCounter[channel] = 0;
}

void channelTest() {
  for (int i = 0; i < NR_COUNTERS; i++) {
    channelLedOn(i);
  }
}

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  printf("Sent packet(%d). ", counter++);
  if (sendStatus == 0){
    printf("Delivery success\n");
  }
  else{
    printf("Delivery fail\n");
  }
}

void RealTimeMessage(const byte msg) {
  printf("A real time message was detected: %d\n", msg);
} // end of RealTimeMessage

#define CNT 10000 // ESP watch dog retrigger counter

int getNext () {
  int cnt = 0;
  
  if (runningStatus != noRunningStatus)
  {
    int c = runningStatus;
    // finished with look ahead.
    runningStatus = noRunningStatus;
    return c;
  }

  // The MIDI serial in input is connected to RX<-D0.
  while (true)
  {
    while (Serial.available () == 0) {
      if ((cnt % CNT) == 0) {
          wdt_disable();  // Reset watch dog of ESP
      }
      cnt += 1;
    }
    byte c = Serial.read();
    if (c >= 0xF8)  // RealTime messages.
      RealTimeMessage(c);
    else {
      return c;
    }
  }

} // end of getNext

// Dump a system exclusive message in hex.
void readPastSysex() {
  int count = 0;
  while (true)
  {
    while (Serial.available () == 0) {}
    byte c = Serial.read ();
    if (c >= 0x80) {
      runningStatus = c;
      return;
    }
    if (++count > 32) {
      count = 0;
    }
  } // End of reading until all system exclusive done.
}

void setup() {
  Serial.begin(31250);  
 
  // The MIDI serial in input is connected to RX<-D0.

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    printf("Error initializing ESP-NOW\n");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  #ifdef DEBUG
    printf("\n");
    printf("\n");
    printf("%s\n", version);
  #endif
  delay(2000);

  // Prepare LEDs.
  ledSetup();
  for (int i = 0; i < NR_COUNTERS; i++) {
    delayCounter[i] = 20 - i;
  }
  channelTest();
  runningStatus = noRunningStatus;
  // Interval in microsecs.
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 100, TimerHandler))
  {
    printf("Starting ITimer OK.\n");
  }
  else {
    printf("Can't set ITimer correctly. Select another freq. or interval.\n");
  }
}

const char *notes[] = { "C", "C*", "D", "D*", "E", "F", "F*", "G", "G*", "A", "A*", "B" };

void handleMidi()
{
  static String timestamp = "";
  byte note;
  byte velocity;
  byte pressure;
  byte param;
  byte message;
  byte programNumber;
  byte value;
  static byte index = 0; // index to buffer containing midi bytes.
  byte song;
  byte sindex;

  wdt_disable(); // Reset watch dog of ESP

  byte c = getNext();

  if (((c & 0x80) == 0) && (lastCommand & 0x80))
  {
    runningStatus = c;
    c = lastCommand;
  }

  // Channel is in low order bits.
  int channel = (c & 0x0F); // Channels are counted from 0 ... 15 by machines.
  //printf("channel %02x\n", channel);
  if (delayCounter[channel] < MAX_COUNT) {
    delayCounter[channel] += INCREMENT;
  }

  channel++; // Channels are counted from 1 ... 16 by humans.
  // Messages start with high-order bit set.
  if (c & 0x80)
  {
    midiData.bytes[index++] = c; // Add the first byte.
    lastCommand = c;
    switch ((c >> 4) & 0x07)
    {
      case 0:
        note = getNext();
        midiData.bytes[index++] = note;        
        velocity = getNext(); // Note off
        midiData.bytes[index++] = velocity;
        break;

      case 1:
        note = getNext();
        midiData.bytes[index++] = note;        
        velocity = getNext(); // Note on
        midiData.bytes[index++] = velocity;
        break;

      case 2:
        note = getNext();
        midiData.bytes[index++] = note;        
        pressure = getNext(); // Polyphonic Pressure
        midiData.bytes[index++] = pressure;
        break;

      case 3:
        message =  getNext() & 0x7F; // Control Change
        midiData.bytes[index++] = message;
        param = getNext();
        midiData.bytes[index++] = param;
        break;

      case 4:
        programNumber = getNext(); // Program Change
        midiData.bytes[index++] = programNumber;
        break;

      case 5:
        pressure = getNext();  // After touch pressure
        midiData.bytes[index++] = pressure;
        break;

      case 6:
        value = getNext() | getNext() << 7; // Pitch wheel
        midiData.bytes[index++] = value;
        break;

      case 7:  // system message
        {
          lastCommand = 0;           // these won't repeat I don't think
          switch (c & 0x0F)
          {
            case 0:
              value = getNext(); // System Exclusive, Vendor ID
              midiData.bytes[index++] = value;
              readPastSysex(); // Read past sysex
              break;

            case 1:
              value = getNext();  // Time code.
              midiData.bytes[index++] = value;              
              break;

            case 2:
              sindex = getNext() | getNext() << 7; // Song position index.
              midiData.bytes[index++] = sindex;
              break;

            case 3:
              song = getNext(); // Song Select
              midiData.bytes[index++] = song;
              break;

            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
              break;
          }
        }  
        break;
    }
  }
  if (index > MAX_NR_BYTES - 2) {
    midiData.size = index;
    for (int i = index - 6; i < index; i++) {
      printf("%02x ", midiData.bytes[i]);
    }
    printf("\n");
    esp_now_send(broadcastAddress, (uint8_t *) &midiData, sizeof(struct_message));
    index = 0;
  }
}

void loop() {
  handleMidi();
}
