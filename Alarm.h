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

    bool* getDays() {
      return days;
    }

    uint8_t getHour() {
      return hour;
    }

    uint8_t getMinute() {
      return minute;
    }

    bool getEnabled() {
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

    // char* toString() {
    //   static char value[100]; // Increased size to accommodate the boolean array string
    //   char daysString[8] = ""; // Adjust size as needed

    //   // Convert the boolean array to a string of '1's and '0's
    //   for (int i = 0; i < 7; i++) {
    //     daysString[i] = this->days[i] ? '1' : '0';
    //   }
    //   daysString[7] = '\0'; // Null-terminate the string

    //   // Format the hour and minute as two-digit numbers
    //   sprintf(value, "%02d:%02d | Days Mon to Sun: [%s] | Enabled: %d", this->hour, this->minute, daysString, this->enabled);

    //   return value;
    // }

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