#include <EEPROM.h>
#include <Bonezegei_DS3231.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <MD_MAX72xx.h>

#include "NotesAndSongs.h"
#include "Alarm.h" //Alarm class

#define TFT_GREEN 0x3cc0
#define TFT_RED 0xf800

//How many button presses to turn off the alarm:
const uint8_t challengeTurns = 8;
const uint16_t timeLedIsOn = 600;

uint8_t randomSequence[challengeTurns];

//Increments and decrements for the alarm definition.
const uint8_t incrementByHour = 1;
const uint8_t incrementByMinute = 1;

/////////////////////////////
/////// PIN SETTINGS ////////
/////////////////////////////

//I absolutely hate pin 12, it has caused me so many issues.
//Had to give up on the relay for turning on the room's light because of interference with SPI and lack of power.
//#define RELAY 12

#define buzzer 7

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
//MAXIMUM IN MEMORY IS 102 (1024 bytes - 1 from marker = 1023 / 10 bytes per Alarm = 102.3).
Alarm alarms[10];

const uint8_t MAX_ALARM = sizeof(alarms)/sizeof(Alarm);

unsigned long lastExecute = 0;
unsigned long currentExecute = millis();

unsigned long alarmCooldown = 0;

bool alwaysShowTime = false;

void setup() {
  pinMode(BTN_LED_1, OUTPUT);
  pinMode(BTN_LED_2, OUTPUT);
  pinMode(BTN_LED_3, OUTPUT);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);

  //Initialize the alarms.
  initializeAlarms(alarms, MAX_ALARM, 1); //Initialize at address 1 to leave 0 for "Is already set?" marker;

  rtc.begin();
  tft.begin();
  mx.begin();

  tft.clearScreen();
}

void loop() {
  currentExecute = millis();

  if(currentExecute - lastExecute > 3000){
    lastExecute = millis();

    if(rtc.getTime()){ // Update RTC time
      uint8_t currentDay = rtc.getDay();
      uint8_t currentHour = rtc.getHour();
      uint8_t currentMinute = rtc.getMinute();

      if(alwaysShowTime){
        writeHourToLEDMatrix(currentHour, currentMinute);
      }else{
          mxPrint("");
      }

      for (int i = 0; i < MAX_ALARM; i++) {
        if (alarms[i].getEnabled()) {
          if (alarms[i].checkIfTimeMatches(currentDay, currentHour, currentMinute)) {
            if(currentExecute - alarmCooldown > 60000){
              generateRandomSequence(challengeTurns, randomSequence);

              while (true) {
                writeHourToLEDMatrix(rtc.getHour(), rtc.getMinute());
                //digitalWrite(RELAY, HIGH);

                if(playAlarm()){ //If alarm gets turned off it returns true.
                  break;
                }
              }

              mxPrint("");

              //Sets a cooldown so alarm doesn't play right after turning off.
              alarmCooldown = millis();
            }
          }
        }
      }
    }
  }

  if(digitalRead(BTN_1) == LOW) {
    alarmDefinition();
  }
  else if(digitalRead(BTN_2) == LOW){
    alwaysShowTime = !alwaysShowTime;

    if(alwaysShowTime && rtc.getTime()){
        writeHourToLEDMatrix(rtc.getHour(), rtc.getMinute());
    }else{
        mxPrint("");
    }
    delay(50);
    buttonOnHold(BTN_2);
  }
}

//Generates a random button sequence for buttons 1, 2 and 3.
void generateRandomSequence(uint8_t length, uint8_t *array) {
  randomSeed(analogRead(A5));

  for (uint8_t i = 0; i < length; i++) {
    array[i] = random(1, 4); // Generate a random number between 1 and 3
  }
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

//Plays the alarm.
bool playAlarm(){
  //Music function By Robson Couto

  // iterate over the notes of the melody.
  // Remember, the array is twice the number of notes (notes + durations)
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    //Checks if the alarm is disabled midway through the music
    if(checkIfAlarmHasBeenTurnedOff()){
      return true;
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

  return false;
}

///////////////////////////////////////////////////////
////////////////Button Handling Functions//////////////
///////////////////////////////////////////////////////

bool debounce(unsigned long& lastTime, unsigned long time){
  uint8_t threshold = 70;

  if(time - lastTime >= threshold){
    lastTime = time;
    return true;
  }else{
    return false;
  }
}

void buttonOnHold(byte Button){
  while(digitalRead(Button) == LOW){
    delay(1);
  }
}

const uint8_t MAX_OPTION = 3;
uint8_t currentAlarm = 0;
uint8_t currentOption = 0;

bool selected = false;
uint8_t lastButton = 0;

//Shows the button pattern on the leds.
void showRandomPattern(){
  for(int i = 0; i < challengeTurns; i++){
    switch(randomSequence[i]){
      case 1:
        digitalWrite(BTN_LED_1, HIGH);
        delay(timeLedIsOn);
        digitalWrite(BTN_LED_1, LOW);
        break;
      case 2:
        digitalWrite(BTN_LED_2, HIGH);
        delay(timeLedIsOn);
        digitalWrite(BTN_LED_2, LOW);
        break;
      case 3:
        digitalWrite(BTN_LED_3, HIGH);
        delay(timeLedIsOn);
        digitalWrite(BTN_LED_3, LOW);
        break;
    }

    delay(200);
  }
}

//Runs a check for disabling the alarm
bool checkIfAlarmHasBeenTurnedOff(){
  if(digitalRead(BTN_1) == LOW){
    delay(50);
    buttonOnHold(BTN_1);

    //digitalWrite(RELAY, LOW);

    showRandomPattern();

    uint8_t userInputPattern[challengeTurns];

    for(int i = 0; i < challengeTurns; i++){
      userInputPattern[i] = 0;
    }

    uint8_t currentIndex = 0;
    bool hadChange = false;

    while(true){
      currentExecute = millis();

      if(debounce(lastExecute, currentExecute)){
        if(digitalRead(BTN_1) == LOW){
          userInputPattern[currentIndex] = 1;
          hadChange = true;
          lastButton = BTN_1;
        }else if(digitalRead(BTN_2) == LOW){
          userInputPattern[currentIndex] = 2;
          hadChange = true;
          lastButton = BTN_2;
        }else if(digitalRead(BTN_3) == LOW){
          userInputPattern[currentIndex] = 3;
          hadChange = true;
          lastButton = BTN_3;
        }

        if(hadChange){
          hadChange = false;
          buttonOnHold(lastButton);

          if(randomSequence[currentIndex] != userInputPattern[currentIndex]){
            currentIndex = 0;
            for(int i = 0; i < challengeTurns; i++){
              userInputPattern[i] = 0;
            }

            tone(buzzer, 300, 700);

            delay(1500);

            showRandomPattern();
          }else{
            currentIndex++;
          }
        }

        if(currentIndex == challengeTurns){
          digitalWrite(BTN_LED_1, HIGH);
          digitalWrite(BTN_LED_2, HIGH);
          digitalWrite(BTN_LED_3, HIGH);

          tone(buzzer, 800);
          
          delay(800);

          digitalWrite(BTN_LED_1, LOW);
          digitalWrite(BTN_LED_2, LOW);
          digitalWrite(BTN_LED_3, LOW);

          noTone(buzzer);

          delay(800);

          break;
        }
      }
    }

    return true;
  }
  else if(digitalRead(BTN_3) == LOW){
    delay(50);

    //digitalWrite(RELAY, LOW);

    uint8_t minutesPassed = 0;
    lastExecute = millis();

    while(minutesPassed < 5){
      currentExecute = millis();
      if(currentExecute - lastExecute > 60000){
        lastExecute = millis();

        if(rtc.getTime()){
          writeHourToLEDMatrix(rtc.getHour(), rtc.getMinute());
        }
        minutesPassed++;
      }

      if(digitalRead(BTN_1) == LOW){
        delay(50);
        break;
      }
    }
  }
  return false;
}

void alarmDefinition() {
  displayAlarm(currentAlarm);
  drawPointer(1,1);

  while (true) {
    currentExecute = millis();

    if(debounce(lastExecute, currentExecute)){
      if (digitalRead(BTN_1) == LOW && digitalRead(BTN_2) == LOW) {
        buttonOnHold(BTN_1);
        break;
      }

      if (digitalRead(BTN_1) == LOW) {
        if (currentOption == 0) {
          currentOption = MAX_OPTION;
        } else {
          currentOption -= 1;
        }

        lastButton = BTN_1;
        displayChange();
      }
      else if (digitalRead(BTN_2) == LOW) {
        selected = !selected;
        
        lastButton = BTN_2;
        displayChange();
      }
      else if (digitalRead(BTN_3) == LOW) {
        if (currentOption == MAX_OPTION) {
          currentOption = 0;
        } else {
          currentOption += 1;
        }

        lastButton = BTN_3;
        displayChange();
      }
    }
  }

  tft.clearScreen();

  saveAlarmsToEEPROM(alarms, MAX_ALARM, 1);
}

//Calls and shows visually the selected element.
void displayChange(){
  if (selected) {
    switch (currentOption) {
      case 0:
        selected_zero();
        break;
      case 1:
        selected_one();
        break;
      case 2:
        selected_two();
        break;
      case 3:
        selected_three();
        break;
      default:
        currentOption = 0;
        break;
    }
    selected = false;
  } else {
    switch (currentOption) {
      case 0:
        drawPointer(1, 1);
        break;
      case 1:
        drawPointer(1,10);
        break;
      case 2:
        drawPointer(1,30);
        break;
      case 3:
        drawPointer(1,50);
        break;
      default:
        currentOption = 0;
        break;
    }
  }

  buttonOnHold(lastButton);
}

//Option for switching between selected alarm.
void selected_zero(){
  buttonOnHold(lastButton);
  drawPointer(1,1);

  while(true){
    currentExecute = millis();

    if(debounce(lastExecute, currentExecute)){


      if(digitalRead(BTN_1) == LOW && digitalRead(BTN_2) == LOW){
        buttonOnHold(BTN_1);
        break;
      }

      if (digitalRead(BTN_1) == LOW) {
        if (currentAlarm == 0) {
          currentAlarm = MAX_ALARM - 1;
        } else {
          currentAlarm -= 1;
        }

        displayAlarm(currentAlarm);
      }
      else if (digitalRead(BTN_2) == LOW) {
        break;
      }
      else if (digitalRead(BTN_3) == LOW) {
        if (currentAlarm == MAX_ALARM - 1) {
          currentAlarm = 0;
        } else {
          currentAlarm += 1;
        }

        displayAlarm(currentAlarm);
      }
    }
  }
}

void selected_one(){
  //Pointer spacing constants
  uint8_t selected = 0;

  const uint8_t pointerY = 20;
  const uint8_t startingXForElementPointer = 82;
  const uint8_t pixelsBetweenNumbers = 19;

  bool hadChange = false;
  drawVerticalPointer(startingXForElementPointer, pointerY);

  buttonOnHold(BTN_2);

  while (true) {
    currentExecute = millis();

    if(debounce(lastExecute, currentExecute)){
      if (digitalRead(BTN_1) == LOW && digitalRead(BTN_2) == LOW) {
        tft.fillRect(startingXForElementPointer, pointerY, 128-startingXForElementPointer, 5, 0x0000);
        buttonOnHold(BTN_1);
        break;
      }

      if (digitalRead(BTN_1) == LOW) {
        if(selected == 0){
          uint8_t alarmHour = alarms[currentAlarm].getHour();

          if(alarmHour - incrementByHour >= 0){
            alarms[currentAlarm].setHour(alarmHour - incrementByHour);
          }else{
            alarms[currentAlarm].setHour(24-incrementByHour);
          }
        }else{
          uint8_t alarmMinute = alarms[currentAlarm].getMinute();

          if(alarmMinute - incrementByMinute >= 0){
            alarms[currentAlarm].setMinute(alarmMinute - incrementByMinute);
          }else{
            alarms[currentAlarm].setMinute(60-incrementByMinute);
          }
        }

        tft.fillRect(10, 10, 118, 8, 0x0000);
        drawTime(alarms[currentAlarm]);

        buttonOnHold(BTN_1);
      }

      if (digitalRead(BTN_2) == LOW) {
        tft.fillRect(startingXForElementPointer, pointerY, 30, 5, 0x0000);

        if (selected == 0) {
          selected = 1;
          drawVerticalPointer(startingXForElementPointer + pixelsBetweenNumbers, pointerY);
        } else {
          selected = 0;
          drawVerticalPointer(startingXForElementPointer, pointerY);
        }
        
        buttonOnHold(BTN_2);
      }

      else if (digitalRead(BTN_3) == LOW) {
        if(selected == 0){
          uint8_t alarmHour = alarms[currentAlarm].getHour();

          if(alarmHour + incrementByHour < 24){
            alarms[currentAlarm].setHour(alarmHour + incrementByHour);
          }else{
            alarms[currentAlarm].setHour(0);
          }
        }else{
          uint8_t alarmMinute = alarms[currentAlarm].getMinute();

          if(alarmMinute + incrementByMinute < 60){
            alarms[currentAlarm].setMinute(alarmMinute + incrementByMinute);
          }else{
            alarms[currentAlarm].setMinute(0);
          }
        }

        tft.fillRect(10, 10, 118, 8, 0x0000);
        drawTime(alarms[currentAlarm]);

        buttonOnHold(BTN_3);
      }
    }
  }  
}

//Option for enabling or disabling the selected alarm
void selected_two(){
  alarms[currentAlarm].setEnabled(!alarms[currentAlarm].getEnabled());

  tft.fillRect(63, 30, 18, 8, 0x0000); //Positioned to cover only the YES/NO

  tft.setTextColor(0xffff);
  tft.setCursor(63, 30);
  tft.print(alarms[currentAlarm].getEnabled() ? "Yes" : "No");
}

//Option for changing weekdays
void selected_three(){
  buttonOnHold(lastButton);

  //Days go monday to sunday.
  const uint8_t MAX_WEEK_DAY = 6; //Array equivalent of the 7th element.
  uint8_t selectedWeekday = 0;

  //Pointer spacing constants
  const uint8_t pointerY = 60;
  const uint8_t startingXForElementPointer = 44;
  const uint8_t pixelsBetweenLetters = 12;

  bool hadChange = false;
  drawVerticalPointer(startingXForElementPointer, pointerY);

  while (true) {
    currentExecute = millis();

    if(debounce(lastExecute, currentExecute)){
      if (digitalRead(BTN_1) == LOW && digitalRead(BTN_2) == LOW) {
        tft.fillRect(startingXForElementPointer, pointerY, 82, 5, 0x0000);
        buttonOnHold(BTN_1);
        break;
      }

      if (digitalRead(BTN_1) == LOW) {
        if (selectedWeekday == 0) {
          selectedWeekday = MAX_WEEK_DAY;
        } else {
          selectedWeekday -= 1;
        }
        hadChange = true;
        lastButton = BTN_1;
      }
      else if (digitalRead(BTN_2) == LOW) {
        alarms[currentAlarm].enableDisableDay(selectedWeekday);

        tft.fillRect(10, 50, 118, 8, 0x0000); //Fill the entire row of weekdays.
        drawWeekdays(alarms[currentAlarm]);

        buttonOnHold(BTN_2);
      }
      else if (digitalRead(BTN_3) == LOW) {
        if (selectedWeekday == MAX_WEEK_DAY) {
          selectedWeekday = 0;
        } else {
          selectedWeekday += 1;
        }
        hadChange = true;
        lastButton = BTN_3;
      }


      if(hadChange){
        hadChange = false;
        tft.fillRect(startingXForElementPointer, pointerY, 82, 5, 0x0000);

        uint8_t currentX = startingXForElementPointer + (selectedWeekday * pixelsBetweenLetters);
        drawVerticalPointer(currentX, pointerY);

        buttonOnHold(lastButton);
      }
    }
  }
}
////////////////////////////////////////////////////

//Draws the option selector pointer.
void drawPointer(uint8_t x, uint8_t y){
  tft.fillRect(0, 0, 6, 65, 0x0000);
  tft.fillTriangle(x, y, x+4, y+4, x, y+8, 0xffff);
}

//Draws the element selection pointer for options that require it.
void drawVerticalPointer(uint8_t x, uint8_t y){
  tft.fillTriangle(x, y+4, x+4, y, x+8, y+4, 0xffff);
}

//Draws the alarm times.
void drawTime(Alarm alarm){
  tft.setCursor(10, 10);
  tft.setTextColor(0xffff);

  tft.print("Alarm Time: ");

  if(alarm.getHour() < 10) tft.print("0");
  tft.print(alarm.getHour());

  tft.print(":");

  if (alarm.getMinute() < 10) tft.print("0");
  tft.print(alarm.getMinute());
}

//Draws the weekdays row.
void drawWeekdays(Alarm alarm){
  tft.setCursor(10, 50);

  tft.setTextColor(0xffff);
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
}

//Displays the alarm object to screen
void displayAlarm(uint8_t index) {
  tft.clearScreen();

  currentOption = 0;
  drawPointer(1,1);

  tft.setTextColor(0xFFFF);

  Alarm alarm = alarms[index];
  char firstLine[13];
  sprintf(firstLine, "Alarm: %02d/%02d", index+1, MAX_ALARM);

  tft.setCursor(10, 1);
  tft.print(firstLine);

  drawTime(alarm);

  tft.setCursor(10, 30);
  tft.print("Enabled: ");
  tft.print(alarm.getEnabled() ? "Yes" : "No");

  drawWeekdays(alarm);
}

void writeHourToLEDMatrix(uint8_t hour, uint8_t minute){
  char buffer[7];
  sprintf(buffer, " %02d:%02d", rtc.getHour(), rtc.getMinute());

  mxPrint(buffer);
}

//Writes a message to the led Matrix
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
