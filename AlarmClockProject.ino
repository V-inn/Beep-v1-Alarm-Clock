#include <EEPROM.h>
#include <Bonezegei_DS3231.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <MD_MAX72xx.h>
#include <ButtonDebounce.h>

#include "NotesAndSongs.h"
#include "Alarm.h" //Alarm class

#define TFT_GREEN 0x3cc0
#define TFT_RED 0xf800

/////////////////////////////
/////// PIN SETTINGS ////////
/////////////////////////////
#define buzzer 7

#define RELAY 12

#define BTN_LED_1 A0
#define BTN_LED_2 A1
#define BTN_LED_3 A2

#define BTN_1 A3
#define BTN_2 2 //Cant continue onto A4 and A5 because of SDA and SCL for Realtime clock module
#define BTN_3 3

//Display settings
#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9

//LED MATRIX SETTINGS
#define MAX7219_DIN 6
#define MAX7219_CS 5
#define MAX7219_CLK 4

//END OF PIN SETTINGS
/////////////////////////////

//BUTTON DEBOUNCE SETTINGS
const unsigned long debounceDelay = 110; // Debounce time in milliseconds

// Create ButtonDebounce objects
ButtonDebounce btn1(BTN_1, debounceDelay);
ButtonDebounce btn2(BTN_2, debounceDelay);
ButtonDebounce btn3(BTN_3, debounceDelay);

//LED MATRIX SETTINGS
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //Hardware configuration may need to be changed according to different led matrixes
                                          //Refer to https://majicdesigns.github.io/MD_MAX72XX/page_new_hardware.html for more info.
#define MAX_DEVICES 4
#define CHAR_SPACING  1 // pixels between characters

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

//Device initialization
TFT_ILI9163C tft = TFT_ILI9163C(TFT_CS, TFT_DC, TFT_RST);
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, MAX7219_DIN, MAX7219_CLK, MAX7219_CS, MAX_DEVICES);
Bonezegei_DS3231 rtc(0x68);

//bool alarmDays[7] = {false, false, false, false, false, false, false};
//Alarm defaultAlarm(alarmDays, 0, 0);

//Defining quantity of alarms.
//MAXIMUM IN MEMORY IS 102 (1024 bytes - 1 from marker = 1023 / 10 bytes per Alarm = 102.3), BUT MAY NEED ADJUSTMENTS TO FIT DISPLAY.
Alarm alarms[10];

uint8_t MAX_ALARM = sizeof(alarms)/sizeof(Alarm);

void setup() {
  pinMode(BTN_LED_1, OUTPUT);
  pinMode(BTN_LED_2, OUTPUT);
  pinMode(BTN_LED_3, OUTPUT);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);

  //Initialize the alarms.
  initializeAlarms(alarms, MAX_ALARM, 1); //Initialize at address 1 to leave 0 for "Is already set?" marker;

  Serial.begin(115200);
  Serial.println("Started.");

  rtc.begin();
  tft.begin();
  mx.begin();

  tft.clearScreen();
  displayAlarm(0);
}

void loop() {
  if(rtc.getTime()){ // Update RTC time
    uint8_t currentDay = rtc.getDay();
    uint8_t currentHour = rtc.getHour();
    uint8_t currentMinute = rtc.getMinute();

    static unsigned long lastUpdate = 0;
    unsigned long currentMillis = millis();

    char buffer[7];
    sprintf(buffer, " %02d:%02d", rtc.getHour(), rtc.getMinute());

    Serial.println(buffer);

    if (currentMillis - lastUpdate >= 2000) {
      mxPrint(buffer);
      lastUpdate = currentMillis;
    }

    for (int i = 0; i < MAX_ALARM; i++) {
      if (alarms[i].getEnabled()) {
        if (alarms[i].checkIfTimeMatches(currentDay, currentHour, currentMinute)) {
          while (!checkIfAlarmHasBeenTurnedOff()) {
            Serial.println("IT IS TIME!");
            //playAlarm();
          }
        }
      }
    }
  }

  if(digitalRead(BTN_1) == LOW) {
    Serial.println("ALARM SET!");

    bool days[7] = {true, true, true, true, true, false, false};
    alarms[0].setAlarm(days, rtc.getHour(), rtc.getMinute());

    alarms[0].setEnabled(false);

    //displayAlarm(0);
    beginAlarmDefinition();
  }

  delay(2000);
}

bool checkIfAlarmHasBeenTurnedOff(){
  return digitalRead(BTN_1) == LOW;
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
    Serial.println("Saving alarms to eeprom.");
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

    if(checkIfAlarmHasBeenTurnedOff()){
      break;
    }

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

void beginAlarmDefinition() {
  uint8_t MAX_OPTION = 3;

  uint8_t currentAlarm = 0;
  uint8_t currentOption = 0;
  bool selected = false;
  bool hadChange = false;

  displayAlarm(currentAlarm);

  while (true) {
    btn1.update();
    btn2.update();
    btn3.update();

    if (digitalRead(BTN_1) == LOW && digitalRead(BTN_2) == LOW) {
      break;
    }

    if (btn1.state() == LOW) {
      if (currentOption == 0) {
        currentOption = MAX_OPTION;
      } else {
        currentOption -= 1;
      }
      hadChange = true;
    }
    else if (btn2.state() == LOW) {
      selected = !selected;
      hadChange = true;
    }
    else if (btn3.state() == LOW) {
      if (currentOption == MAX_OPTION) {
        currentOption = 0;
      } else {
        currentOption += 1;
      }
      hadChange = true;
    }

    if (hadChange) {
      if (selected) {
        switch (currentOption) {
          case 0:
            delay(200);
            while(true){
              btn1.update();
              btn2.update();
              btn3.update();

              if (btn1.state() == LOW) {
                if (currentOption == 0) {
                  currentAlarm = MAX_ALARM - 1;
                } else {
                  currentAlarm -= 1;
                }

                displayAlarm(currentAlarm);
              }
              else if (btn2.state() == LOW) {
                break;
              }
              else if (btn3.state() == LOW) {
                if (currentAlarm == MAX_OPTION - 1) {
                  currentAlarm = 0;
                } else {
                  currentAlarm += 1;
                }

                displayAlarm(currentAlarm);
              }

              delay(200);
            }
          case 1:
            break;
          case 2:
            break;
          case 3:
            break;
        }
        Serial.println("Was selected");
      } else {
        switch (currentOption) {
          case 0:
            break;
          case 1:
            break;
          case 2:
            break;
          case 3:
            break;
        }
      }
      Serial.println("Had change");
      Serial.println(currentOption);
      Serial.println(selected);
      hadChange = false;
    }

    delay(100); // Reduce if faster response time is needed.
  }

  Serial.println("EXITED LOOP");
}

void displayAlarm(uint8_t index) {
  Serial.println("Displaying alarm");

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  tft.clearScreen();
  tft.setTextColor(0xFFFF);

  Alarm alarm = alarms[index];
  char firstLine[13];
  sprintf(firstLine, "Alarm: %02d/%02d", index+1, MAX_ALARM);

  tft.setCursor(10, 1);
  tft.print(firstLine);
  tft.setCursor(10, 10);
  tft.print("Alarm Time: ");
  tft.print(alarm.getHour());
  tft.print(":");
  if (alarm.getMinute() < 10) tft.print("0");
  tft.print(alarm.getMinute());

  tft.setCursor(10, 30);
  tft.print("Enabled: ");
  tft.print(alarm.getEnabled() ? "Yes" : "No");

  tft.setCursor(10, 50);
  tft.print("Days: ");
  const char* dayNames[7] = {"M", "T", "W", "T", "F", "S", "S"};
  bool* days = alarm.getDays();
  for (int i = 0; i < 7; i++) {
    if (days[i]) {
      tft.setTextColor(TFT_GREEN);
    }else{
      tft.setTextColor(TFT_RED);
    }
    tft.print(dayNames[i]);
    tft.print(" ");
  }

  SPI.endTransaction();
}

void mxPrint(const char *pMsg) {
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = (MAX_DEVICES * COL_SIZE) - 1;

  mx.control(0, MAX_DEVICES - 1, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx.clear();

  do {
    switch(state) {
      case 0: // Load the next character from the font table
        if (*pMsg == '\0') {
          showLen = col - (MAX_DEVICES * COL_SIZE);  // padding characters
          state = 2;
          break;
        }

        showLen = mx.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
        // Fall through to next state to start displaying

      case 1: // Display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);

        if (curLen == showLen) {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;

      case 2: // Initialize state for displaying empty columns
        curLen = 0;
        state++;
        // Fall through

      case 3: // Display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1;   // This definitely ends the do loop
    }
  } while (col >= 0);

  mx.control(0, MAX_DEVICES - 1, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
