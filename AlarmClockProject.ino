#include <EEPROM.h>
#include <Bonezegei_DS3231.h>
#include "NotesAndSongs.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <MD_MAX72xx.h>

/////////////////////////////
/////// PIN SETTINGS ////////
/////////////////////////////
int buzzer = 12;

//Display settings
#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9

//LED MATRIX SETTINGS
#define MAX7219_DIN 6
#define MAX7219_CS 5
#define MAX7219_CLK 4

/////////////////////////////

//LED MATRIX SETTINGS
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //Hardware configuration may need to be changed according to different led matrixes
                                          //Refer to https://majicdesigns.github.io/MD_MAX72XX/page_new_hardware.html for more info.
#define MAX_DEVICES 4

//Device initialization
TFT_ILI9163C tft = TFT_ILI9163C(TFT_CS, TFT_DC, TFT_RST);
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, MAX7219_DIN, MAX7219_CLK, MAX7219_CS, MAX_DEVICES);
Bonezegei_DS3231 rtc(0x68);

/*---------------SOUNDS SETTINGS----------------*/
// change this to make the song slower or faster
int tempo = 160;

// sizeof gives the number of bytes, each int value is composed of two bytes (16 bits)
// there are two values per note (pitch and duration), so for each note there are four bytes
int notes = sizeof(melody) / sizeof(melody[0]) / 2;

// this calculates the duration of a whole note in ms
int wholenote = (60000 * 4) / tempo;

int divider = 0, noteDuration = 0;

/*----------------------------------------------*/

class Alarm {
  private:
    bool days[7]; // The weekdays the alarm goes off
    uint8_t hour; // The hour the alarm goes off
    uint8_t minute; // The minute the alarm goes off
    bool enabled; //Alarm will only play if enabled

  public:
    //Default constructor
    Alarm() {
      for (int i = 0; i < 7; i++) {
        this->days[i] = false;
      }
      this->hour = 0;
      this->minute = 0;
      this->enabled = false;
    }

    // Parameterized Constructor
    Alarm(bool days[7], uint8_t hour, uint8_t minute) {
      for (int i = 0; i < 7; i++) {
        this->days[i] = days[i];
      }
      this->hour = hour;
      this->minute = minute;
      this->enabled = true;
    }

    void setEnabled(bool enabled){
      this->enabled = enabled;
    }

    bool isEnabled(){
      return enabled;
    }

    bool checkIfTimeMatches(uint8_t day, uint8_t hour, uint8_t minute) {
      if (this->hour != hour) {
        return false;
      }

      if (this->minute != minute) {
        return false;
      }

      if (this->days[day - 1]) {
        return true;
      }

      return false;
    }

    void setAlarm(bool days[7], uint8_t hour, uint8_t minute){
      for (int i = 0; i < 7; i++) {
        this->days[i] = days[i];
      }
      this->hour = hour;
      this->minute = minute;
      this->enabled = true;
    }

    // Method to save the alarm to EEPROM
    void saveToEEPROM(int startAddress) {
      for (int i = 0; i < 7; i++) {
        EEPROM.write(startAddress + i, days[i]);
      }
      EEPROM.write(startAddress + 7, hour);
      EEPROM.write(startAddress + 8, minute);
      EEPROM.write(startAddress + 9, enabled);
    }

    // Method to load the alarm from EEPROM
    void loadFromEEPROM(int startAddress) {
      for (int i = 0; i < 7; i++) {
        days[i] = EEPROM.read(startAddress + i);
      }
      hour = EEPROM.read(startAddress + 7);
      minute = EEPROM.read(startAddress + 8);
      enabled = EEPROM.read(startAddress + 9);
    }
};

bool alarmDays[7] = {false, false, false, false, false, false, false};
Alarm defaultAlarm(alarmDays, 0, 0);

Alarm alarms[10];

void setup() {
  //Initialize the alarms.
  initializeAlarms(alarms, sizeof(alarms), 1); //Initialize at address 1 to leave 0 for "Is already set?" marker;

  Serial.begin(115200);

  rtc.begin();
  tft.begin();
  mx.begin();
}

void loop() {
  if (rtc.getTime()) { // Ensure the RTC time is read
    uint8_t currentDay = rtc.getDay();
    uint8_t currentHour = rtc.getHour();
    uint8_t currentMinute = rtc.getMinute();

    

    for(int i = 0; i < sizeof(alarms); i++){
      if(alarms[i].isEnabled()){
        if (defaultAlarm.checkIfTimeMatches(currentDay, currentHour, currentMinute)) {
          Serial.println("IT IS TIME!");
          playAlarm();
        } else {
          Serial.println("You can keep sleeping...");
        }
      }
    }
  } else {
    Serial.println("Failed to get time");
  }

  delay(5000);
}

// Function to save an array of alarms to EEPROM
void saveAlarmsToEEPROM(Alarm alarms[], int numAlarms, int startAddress) {
  int alarmSize = 10; // Each Alarm object takes 10 bytes
  for (int i = 0; i < numAlarms; i++) {
    alarms[i].saveToEEPROM(startAddress + i * alarmSize);
  }
}

// Function to load an array of alarms from EEPROM
void loadAlarmsFromEEPROM(Alarm alarms[], int numAlarms, int startAddress) {
  int alarmSize = 10; // Each Alarm object takes 10 bytes
  for (int i = 0; i < numAlarms; i++) {
    alarms[i].loadFromEEPROM(startAddress + i * alarmSize);
  }
}

// Function to initialize alarms if not already done
void initializeAlarms(Alarm alarms[], int numAlarms, int startAddress) {
  // Check if alarms have already been initialized
  if (EEPROM.read(0) != 0xAA) { // 0xAA is a marker value
    // Save alarms to EEPROM
    saveAlarmsToEEPROM(alarms, numAlarms, startAddress);
    // Set the marker value
    EEPROM.write(0, 0xAA);
  } else {
    // Load alarms from EEPROM
    loadAlarmsFromEEPROM(alarms, numAlarms, startAddress);
  }
}

void playAlarm(){
  //By Robson Couto

  // iterate over the notes of the melody.
  // Remember, the array is twice the number of notes (notes + durations)
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider = melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(buzzer, melody[thisNote], noteDuration * 0.9);

    // Wait for the specief duration before playing the next note.
    delay(noteDuration);

    // stop the waveform generation before the next note.
    noTone(buzzer);
  }
}
