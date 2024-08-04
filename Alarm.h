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

    void setAlarm(bool days[7], uint8_t hour, uint8_t minute){
      for (int i = 0; i < 7; i++) {
        this->days[i] = days[i];
      }
      this->hour = hour;
      this->minute = minute;
      this->enabled = true;
    }

    void setHour(uint8_t hour){
      if(hour <= 24 && hour >= 0){
        this->hour = hour;
      }
    }

    void setMinute(uint8_t minute){
      if(minute <= 59 && minute >= 0){
        this->minute = minute;
      }
    }

    void enableDisableDay(uint8_t day){
      this->days[day] = !this->days[day];
    }

    // Method to save the alarm to EEPROM
    void saveToEEPROM(int startAddress) {
      for (int i = 0; i < 7; i++) {
        EEPROM.update(startAddress + i, days[i]);
      }
      EEPROM.update(startAddress + 7, hour);
      EEPROM.update(startAddress + 8, minute);
      EEPROM.update(startAddress + 9, enabled);
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