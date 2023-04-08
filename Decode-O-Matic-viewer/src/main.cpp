/* 
  This program received midi data and prints it in a 
  human readable for to serial out.
  Sysex data is skipped.
  You can cat the usb device it uses or use minicom or other software 
  to visualise the output. The baudrate must be set to 112500 
  and you need to switch on 'add carriage return . 
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// Version 1.0

//#define DEBUG 1

#define MAX_NR_BYTES 60
#define CNT 10

#define BUILDIN_LED 2
#define EXTERNAL_LED D8

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    byte size;
    char bytes[MAX_NR_BYTES];
} struct_message;

// Create a struct_message called myData
struct_message midiData;
byte byteIndex = 0;
byte lastCommand;
int counter = 1;

byte getNext() {
  if (byteIndex < midiData.size) 
  {
    return(midiData.bytes[byteIndex++]);
  }
  return(0xFF);
}

const char *notes[] = { "C", "C*", "D", "D*", "E", "F", "F*", "G", "G*", "A", "A*", "B" };

void decodeAndPrint() {
  byte note;
  byte velocity;
  byte pressure;
  byte param;
  byte message;
  byte programNumber;
  byte value;
  byte song;
  byte pointer;
  char tmp[6];

  while(byteIndex < midiData.size) {
    // Read from a circular buffer.
    byte c = getNext();
    if (((c & 0x80) == 0) && (lastCommand & 0x80))
    {
      c = lastCommand;
    }

    // channel is in low order bits
    int channel = (c & 0x0F) + 1;
    // messages start with high-order bit set
    if (c & 0x80)
    {
      lastCommand = c;
      switch ((c >> 4) & 0x07)
      {
        case 0:
          note = getNext();
          velocity = getNext(); // Note off
          sprintf(tmp, "%s%02d", notes[note % 12], (int) (note / 12) - 1);
          if (strlen(tmp) < 4) 
            printf("%02d NF %s%d  %3d\n", channel, notes[note % 12], (int) (note / 12) - 1, velocity);
          else 
            printf("%02d NF %s%d %3d\n", channel, notes[note % 12], (int) (note / 12) - 1, velocity);
          break;

        case 1:
          note = getNext();
          velocity = getNext(); // Note on
          sprintf(tmp, "%s%02d", notes[note % 12], (int) (note / 12) - 1);
          if (strlen(tmp) < 4)           
            printf("%02d NO %s%d  %3d\n", channel, notes[note % 12], (int) (note / 12) - 1, velocity);
          else 
            printf("%02d NO %s%d %3d\n", channel, notes[note % 12], (int) (note / 12) - 1, velocity);          
          break;

        case 2:
          note = getNext();
          pressure = getNext(); // Polyphonic Pressure
          printf("%02d PP %d PR:%0d\n", channel, note, pressure);
          break;

        case 3:
          message =  getNext() & 0x7F; // Control Change
          param = getNext();
          printf("%02d CC %d (0x%02x) Value:%3d\n", channel, message, message, param);
          break;

        case 4:
          programNumber = getNext(); // Program Change
          printf("%02d PC PG:%3d (0x%02x)\n", channel, programNumber, programNumber);
          break;

        case 5:
          pressure = getNext();  // After touch pressure
          printf("%02d %s %s%03d\n", channel, "AT", "Val:", pressure);
          break;

        case 6:
          value = getNext() | getNext() << 7; // Pitch wheel
          printf("%02d %s%03d\n", channel, "PW Val:", value);
          break;

        case 7:  // system message
          {
            lastCommand = 0;           // these won't repeat I don't think
            switch (c & 0x0F)
            {
              case 0:
                value = getNext(); // System Exclusive, Vendor ID
                if (value == 0xF1) {
                  printf("Connection test\n");
                } else {
                  printf("%s%03d\n", "sx=", value);
                }
                break;

              case 1:
                value = getNext();  // Time code.
                printf("%s%03d\n", "tc,type=", value);
                break;

              case 2:
                pointer = getNext() | getNext() << 7; // Song position pointer.
                printf("%s%03d\n", "sp=", pointer);
                break;

              case 3:
                song = getNext(); // Song Select
                printf("%s%03d\n", "ss=", song);
                break;

              case 4:
                printf("Reserved-4\n");
                break;

              case 5:
                printf("Reserved-5\n");
                break;

              case 6:
                printf("Tune-request\n");
                break;

              case 7:
                printf("End_of_Exclusive\n");
                break;

              case 8:
                printf("Timing_clock\n");
                break;

              case 9:
                printf("Reserved-9\n");
                break;

              case 10:
                printf("Start\n");
                break;

              case 11:
                printf("Continue\n");
                break;

              case 12:
                printf("Stop\n");
                break;

              case 13:
                printf("Reserved-13\n");
                break;

              case 14:
                printf("Active-Sensing\n");
                break;

              case 15:
                printf("Reset\n");
                break;
            }  // end of switch on system message
          }  // end system message
          break;
      }  // end of switch
    }  // end of if
    // #ifdef DEBUG
      else {
        printf("0x%02x == d%d?\n", c, c);
      }
    // #endif
    if ((byteIndex % CNT) == 0) {
          wdt_reset();
      }
  }
}

// Callback function that will be executed when data is received.
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&midiData, incomingData, sizeof(midiData));
  printf("Packet(%d), number of bytes received: %d\n", counter++, midiData.size);    
  #ifdef DEBUG
    for (unsigned int i = midiData.size - 6; i < midiData.size; i++) {
      printf("%02x ", incomingData[i+1]);
    }
    printf("\n");
  #endif
  byteIndex = 0;
  // Toggle the led to signal data reception.  
  digitalWrite(EXTERNAL_LED, !digitalRead(EXTERNAL_LED)); 
  digitalWrite(BUILDIN_LED, !digitalRead(BUILDIN_LED)); 
  decodeAndPrint();
}

void setup() {
  // Initialize Serial Monitor.
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station.
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    printf("Error initializing ESP-NOW\n");
    return;
  }
  printf("Ready to receive data.\n");
  
  // Once ESPNow is successfully initialized, we will register for recv CB to
  // get recv packer info.
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  pinMode(BUILDIN_LED, OUTPUT);
  pinMode(EXTERNAL_LED, OUTPUT);
  digitalWrite(BUILDIN_LED, LOW);
  digitalWrite(EXTERNAL_LED, LOW);
}

void loop() {

}
