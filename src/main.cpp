#include <Arduino.h>
#include <cmath>
#include <list>
using namespace std;
#include <string> // for string and to_string()

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>


// Define CS pin for the SD card module
#define SD_CS 5

const int probePin = 36; //this sets which ADC pin to take from, this will need to be updated for the correct pin number
int probeValue;//this variable will track the value from the probe - value from 0 to 4095
double doSat; //this is the saturated concentration of dissolved oxygen from the calculations
double calibrationConstant = 0;//this variable will serve as the current calibration constant in ((mg/liter of DO)/mV)
double do_mgl;//this will store the dissolved oxygen value in mg/liter of DO
double mV; //this is the mV value we get from the probe, this will need to be calculated from probeValue
int amplifierFactor;//This is the gain factor we put on the probe's mV before its converted into an int from 0 to 4095
int state = 2; //this controls what state the system is in, 2 means collecting data but not deployed
double temp;
#define button 10 //define pin 10 as the button pin, this can be changed to any digital pin on the esp
boolean notStable;
boolean full;
int stableValue = 10; //this is the value that determines how stable the probe reading needs to be in order for the system to 
                  //  calculate the calibration constant. The value should be somewhere 0 through 4095
                  // it is set to 10 but that can be changed
int lastProbeValue;
string dataMessage;

// put function declarations here:
void collectData();
void storeData();
void calibrate();

void setup() {
  // put your setup code here, to run once:

  //grabbed this code from https://randomnerdtutorials.com/esp32-data-logging-temperature-to-microsd-card/
  // it sets up the sd card 
  //I changed it a bit to fit our purposes
  // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the data.csv file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.csv");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.csv", "State,raw mV readings,mg/L DO,Callibration Constant \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();

}

void loop() {
  // put your main code here, to run repeatedly:

  //this is code to let the user decide which state the system is in.
  //currently it scrolls from 1 through 4 but this should be changed, 
  //you shouldnt have to go through one state to get to another.
  if (digitalRead(button) == HIGH) {
    state++;
    if (state >= 5) {
      state = 1;
    }
  }
  collectData();
  storeData();
  if (state == 4) {
    calibrate();
  }

}

//this is a function to help with calibration, it takes the array that keeps track of the differences
// and adds a new value to the front and deletes one off the back.
void pushFront(int arr[10], int newVal) {
  int temp1,temp2;
  temp1 = arr[0];
  arr[0] = newVal;
  for (int j = 1; j < 10; j++) {
    temp2 = arr[j];
    arr[j] = temp1;
    temp1 = temp2;
  }
}

void calibrate() {
  //here we will get the calibration constant.

  //this section waits until the value from the probe is relatively stable
  notStable = true;
  int averageArr[10];
  lastProbeValue = analogRead(probePin);
  delay(10000);//delay 10 seconds

  for (int x = 0; x < 10; x++) {
    probeValue = analogRead(probePin);
    int difference = abs(probeValue - lastProbeValue);
    averageArr[x] = difference;
    lastProbeValue = probeValue;
    delay(10000);//delay 10 seconds
  }

  while (notStable) {
    

    probeValue = analogRead(probePin);
    int difference = abs(probeValue - lastProbeValue);
    pushFront(averageArr, difference);
    lastProbeValue = probeValue;
    int sum;
    for (int y = 0; y < 10; y++) {
      sum = sum + averageArr[y];
    }
    float average = sum/10;
    if (average > stableValue) {
      notStable = false;
    }
    delay(10000);//delay 10 seconds
  }


  //this calculates the saturated concentration of dissolved oxygen based on temp variable(temp should probably be recorded in celsius)
  //this was based on JP's google spreadsheet and only slightly changed for C++
  doSat = ((exp(7.7117-1.31403*log(temp+45.93)) ) * (1-exp(11.8571- (3840.7/(temp+273.15)) - (216961/pow((temp+273.15),2)))) * (1-(0.000975-(0.00001426*temp)+(0.00000006436*(pow(temp,2))))))/(1-exp(11.8571-(3840.7/(temp+273.15))-(216961/pow((temp+273.15),2))))/(1-(0.000975-(0.00001426*temp)+(0.00000006436*(pow(temp,2)))));
  probeValue = analogRead(probePin);//this gets the mV value from the probe and saves it as a value from 0 to 4095
  mV = ((probeValue*3.3)/4096)/amplifierFactor;//on this line we should convert the int value from the analogRead and convert it into mV, this should be the original voltage coming from the probe  
  calibrationConstant = doSat/mV;

}

void collectData() {
  probeValue = analogRead(probePin);//this gets the mV value from the probe and saves it as a value from 0 to 4095
  mV = ((probeValue*3.3)/4096)/amplifierFactor;//on this line we should convert the int value from the analogRead and convert it into mV, this should be the original voltage coming from the probe
  do_mgl = mV * calibrationConstant; //calculate the mg/l of DO.
}

void storeData() {

  
  logSDCard();
  //here we should store the different values
    // - raw mV value calculated from probeValue
    // - current calibration constant
    // - do_mgl value
    // - %Sat
    // - current state, as in which mode of operation its in
    // - 
  //all of these should be in different collumns in the csv file. 
  //this csv should be stored on an SD card and then regularly pushed to a remote server. 
}




//grabbed this code from https://randomnerdtutorials.com/esp32-data-logging-temperature-to-microsd-card/ 
// I changed it a bit to suit our needs 
//Write the sensor readings on the SD card
void logSDCard() {
  dataMessage = to_string(state) + "," + to_string(mV) + "," + to_string(do_mgl) + "," + 
                to_string(calibrationConstant) + "\r\n";
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}