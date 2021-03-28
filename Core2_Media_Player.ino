

/*
   March 28, 2021
   Made changes to the Example Core2 Web Radio Player from:
   https://www.hackster.io/tommyho/arduino-web-radio-player-c4cb23

    I see that the actual library for the functions came from:
    Earle Philhower III
    Who credits his library to :  StellaPlayer and libMAD
    https://github.com/earlephilhower/ESP8266Audio

*/

//
//  m5StreamTest Version 2020.12b (Source/Buffer Tester)
//  Board: M5StackCore2 (esp32)
//  Author: tommyho510@gmail.com
//  Required: Arduino library ESP8266Audio 1.60
//

#include <M5Core2.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "Free_Fonts.h"
//#include <spiram-fast.h>

const int bufferSize = 128 * 1024; // buffer size in byte


// Enter your WiFi, Station, button settings here:

const char *SSID = "ENTER_SSID_HERE";
const char *PASSWORD = "ENTER_WIFI_PASSWORD_HERE";

// Added Charlie FM in Portland Oregon
//http://24083.live.streamtheworld.com:80/KYCHFM_SC
//
//Removed these from the list:
//  {"Mega Shuffle", "http://jenny.torontocast.com:8134/stream"},
//  {"Way Up Radio", "http://188.165.212.154:8478/stream"},
//  {"Asia Dream", "https://igor.torontocast.com:1025/;.-mp3"},
//  {"KPop Way Radio", "http://streamer.radio.co/s06b196587/listen"},
//  {"SomaFM", "http://ice2.somafm.com/christmas-128-mp3"}

const int stations = 7;// Change Number here if you add feeds!
char * stationList[stations][2] = {
  {"Charlie FM", "http://24083.live.streamtheworld.com:80/KYCHFM_SC"},
  {"MAXXED Out", "http://149.56.195.94:8015/steam"},
  {"Orig. Top 40", "http://ais-edge09-live365-dal02.cdnstream.com/a25710"},
  {"Smooth Jazz", "http://sj32.hnux.com/stream?type=http&nocache=3104"},
  {"Smooth Lounge", "http://sl32.hnux.com/stream?type=http&nocache=1257"},
  {"Classic FM", "http://media-ice.musicradio.com:80/ClassicFMMP3"},
  {"Lite Favorites", "http://naxos.cdnstream.com:80/1255_128"}
};

float audioGain = 0.0;
float gainfactor = 0.08;
int currentStationNumber = 0;
unsigned long disUpdate = millis();

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *filemp3;
AudioFileSourceBuffer *buffmp3;
AudioOutputI2S *out, *outmp3;


// Draw a + mark centred on x,y
void drawDatumMarker(int x, int y)
{
  M5.Lcd.drawLine(x - 5, y, x + 5, y, TFT_GREEN);
  M5.Lcd.drawLine(x, y - 5, x, y + 5, TFT_GREEN);
}

/// WIFI Routines *********************

void initwifi() {

  M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextDatum(BC_DATUM);
  M5.Lcd.setFreeFont(FSB12);   
  M5.Lcd.drawString("Connecting..", M5.Lcd.width()/2, 200, GFXFF);

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  // Try forever
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("STATUS(Connecting to WiFi) ");
    delay(1500);
    i = i + 1;
    if (i > 15) {
      ESP.restart();
    }
  }
  Serial.println("\nWiFi Connected!\n");
}


// Display network information on the LCD
void displayWiFiInformation() {
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFreeFont(FSS9);    
  M5.Lcd.setTextDatum(BL_DATUM); 
  M5.Lcd.drawString("Network: ", 10, 165, GFXFF);
  M5.Lcd.drawString("IP: " , 10, 190, GFXFF);
  M5.Lcd.drawString(SSID, 90, 165, GFXFF);
  M5.Lcd.drawString(WiFi.localIP().toString(),40,190,GFXFF);
}


// Update WiFi Signal Strength
void updateWiFiSignal() {
  // Display the WiFi Signal Strength
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFreeFont(FSS9);    
  M5.Lcd.setTextDatum(BL_DATUM); 
  M5.Lcd.drawString("WiFi Signal: ", 10, 215, GFXFF);
  M5.Lcd.fillRect(112,195,30,20,BLACK);
  uint16_t clr = GREEN;
  clr = (WiFi.RSSI() < -70) ? TFT_RED : TFT_GREEN;
  M5.Lcd.setTextColor(clr, TFT_BLACK);  
  M5.Lcd.drawString(String(WiFi.RSSI()),115, 215, GFXFF);
}


/// Battery ***************************

// Calculate Battery Useable range  (3.2 to 4.1 Volts)
void displayBattery() {
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFreeFont(FSS9);  
  int maxVolts = 410;  // Battery Max volts * 100
  int minVolts = 320;  // Battery Min Volts * 100
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  
  M5.Lcd.setTextSize(1);
  char battInfo[5];
  dtostrf(M5.Axp.GetBatVoltage(),1,2,battInfo); 
  String btInfo = "Batt: " + String(battInfo);
  M5.Lcd.setTextDatum(BL_DATUM);  
  M5.Lcd.drawString(btInfo, 230, 215, GFXFF);
//  drawDatumMarker(230,215);
    
  int batt = map(M5.Axp.GetBatVoltage() * 100, minVolts, maxVolts, 0 , 10000) / 100.0;

  // Draw Battery bar(s) on the right side of the screen
  uint16_t clr = GREEN;
  for (int x = 9; x >= 0; x--) {
    if (x < 3) clr = RED;
    else if (x < 6) clr = YELLOW;   
    M5.Lcd.fillRoundRect(314, (216 - (x * 24)), 6, 21, 2, (batt > (x * 10)) ? clr : BLACK);
    M5.Lcd.drawRoundRect(314, (216 - (x * 24)), 6, 21, 2, TFT_LIGHTGREY);
  }
}


// MISC  ****************************


// Remove the Track information (While changing stations)
void clearTrack() {
  M5.Lcd.fillRect(10, 55, 300, 70, TFT_DARKGREY); // Clear the area of old data
  M5.Lcd.drawRect(10, 55, 300, 70, BLUE); // Draw a box around the Track Information
}


// Identify buttons at the bottom of screen
void drawButtons() {
  M5.Lcd.fillRect(10,220,300,25,YELLOW);
  M5.Lcd.setTextColor(TFT_BLACK);  
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setFreeFont(FSB12);   
  M5.Lcd.drawString("Volume", 55,220, GFXFF);
  M5.Lcd.drawString("Station", M5.Lcd.width()/2,220, GFXFF);
  M5.Lcd.drawString("Mute", 270 ,220, GFXFF);  
}


// Get the Split String Value Used for Band or Track
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


// MP3, Audio etc.  ****************************

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;

  String band  = getValue(s2, '-', 0);
  band.trim();
  String track = getValue(s2, '-', 1);
  track.trim();

  if(band.length() > 30) band = band.substring(0, 30);
  if(track.length() > 30) track = track.substring(0, 30);

//  Serial.printf("Band: %s   Track:  %s  \n", band.c_str(), track.c_str());
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  M5.Lcd.setTextSize(1);

  M5.Lcd.setTextColor(TFT_BLACK, TFT_DARKGREY );
  
  clearTrack();
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS9); 
  if(band.length() < 20) M5.Lcd.setFreeFont(FSS12);                              // Select the font
  M5.Lcd.drawString(band, M5.Lcd.width()/2, 72, GFXFF);
  M5.Lcd.setFreeFont(FSS9);
  if(track.length() < 20) M5.Lcd.setFreeFont(FSS12);
//  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.drawString(track, M5.Lcd.width()/2, 107, GFXFF);
  Serial.flush();
  // Make sure the new song information does not overwrite the battery
  displayBattery();
}


// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}


void stopPlaying() {
  Serial.printf("Stopping MP3...\n");
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  Serial.printf("MP3 Stopped, Stopping Buffer...\n");
  if (buffmp3) {
    buffmp3->close();
    delete buffmp3;
    buffmp3 = NULL;
  }
  Serial.printf("Buffer stopped... Stopping File ...\n");
  if (filemp3) {
    filemp3->close();
    delete filemp3;
    filemp3 = NULL;
  }
  if (outmp3) {
    //    filemp3->close();
    delete outmp3;
    outmp3 = NULL;
  }

  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
}


// Update the Station Label
void updateStation(String message) {
  M5.Lcd.fillRect(10, 10, 300, 35, BLACK); // Clear out other information on the line
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(TC_DATUM);

  M5.Lcd.setFreeFont(FSB18);
  M5.Lcd.drawString(message, M5.Lcd.width()/2, 10, GFXFF);
}


// Change to the next station
void changeStation() {
  updateStation("Changing..Wait..");
  currentStationNumber++;
  if (currentStationNumber >= stations) currentStationNumber = 0;
  Serial.printf("\n******** Changing to channel number: %i\n", currentStationNumber);
}


// Change the volume level
// Update the volume graphic
void changeVolume() {
  audioGain += 1.0;
  if (audioGain > 10.0) {
    audioGain = 1.0;
  }
  if (audioGain < 0.0) {
    audioGain = 0.0;
  }
  
  int xtPos = 260; // X Position for the Volume indication
  outmp3->SetGain(audioGain * gainfactor); // Change Volume to new level

//---------New Volume Bar on left side of LCD  *******************
  // Draw Volume bar(s) on the left side of the screen
  uint16_t clr = RED;
  for (int x = 9; x >= 0; x--) {
    if (x < 5) clr = GREEN;
    else if (x < 8) clr = TFT_ORANGE;   
    M5.Lcd.fillRoundRect(0, (216 - (x * 24)), 6, 21, 2, (audioGain > x ) ? clr : BLACK);
    M5.Lcd.drawRoundRect(0, (216 - (x * 24)), 6, 21, 2, TFT_LIGHTGREY);
  }

  // Alternate Draw the Volume Indicator (Triangle)
//  M5.Lcd.fillTriangle(xtPos, 20, xtPos + 50, 20, xtPos + 50, 0, BLACK); // Clear out old Meter
//  if (audioGain > 9) { // If we are full, draw red, blue and green
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * audioGain), 20, xtPos + (5 * audioGain), 20 - (2 * audioGain), RED);
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * 9), 20, xtPos + (5 * 9), 20 - (2 * 9), BLUE);
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * 6), 20, xtPos + (5 * 6), 20 - (2 * 6), GREEN);
//  }
//  else if (audioGain >= 6) { // if above 5, draw blue and green
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * audioGain), 20, xtPos + (5 * audioGain), 20 - (2 * audioGain), BLUE);
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * 6), 20, xtPos + (5 * 6), 20 - (2 * 6), GREEN);
//  }
//  else if (audioGain >= 1)
//    M5.Lcd.fillTriangle(xtPos, 20, xtPos + (5 * audioGain), 20, xtPos + (5 * audioGain), 20 - (2 * audioGain), GREEN);
}


/*
   Setup output to I2S Device
   Set Pins and Gain
   Set FileSource as web radio station
   Join FileSource to get MetaData
   Create Buffer for data
   Register Callback for...?
   Begin the MP3 playback
*/
void playMP3() {
  outmp3 = new AudioOutputI2S(0, 0); // Output to builtInDAC
  outmp3->SetPinout(12, 0, 2);
  outmp3->SetOutputModeMono(true);
  outmp3->SetGain(audioGain * gainfactor);
  filemp3 = new AudioFileSourceICYStream(stationList[currentStationNumber][1]);
  filemp3->RegisterMetadataCB(MDCallback, (void*)"ICY"); // ID3TAG  // ICY
  // StreamTitle
  buffmp3 = new AudioFileSourceBuffer(filemp3, bufferSize);
  buffmp3->RegisterStatusCB(StatusCallback, (void*)"buffer");
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buffmp3, outmp3);
  Serial.printf("STATUS(URL) %s %s\n", stationList[currentStationNumber][0], stationList[currentStationNumber][1]);
  Serial.flush();
  updateStation(String(stationList[currentStationNumber][0]));
}


void loopMP3() {
  if (mp3 != NULL) {  // To avoid crash while changing stationsI
    if (mp3->isRunning()) {
      if (!mp3->loop()) mp3->stop();
    } else {
      Serial.printf("Status(Stream) Stopped \n");
      clearTrack();
      changeStation();
      //      stopPlaying();
      delay(1000);
      playMP3();
    }
  }
}


// General Arduino Routines

void setup() {
  Serial.begin(115200);

  M5.begin();
  M5.Axp.SetSpkEnable(true);
  //  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextWrap(false);

  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setFreeFont(FSB18);   
  M5.Lcd.drawString("Core2 Web Radio", M5.Lcd.width()/2, 20, GFXFF);

  initwifi();
  delay(500);
  M5.Lcd.clear();
  drawButtons();
  playMP3();
  changeVolume();  // To update Volume setting and graphic
  displayWiFiInformation();
}


void loop() {
  loopMP3();
  M5.update();

  if (m5.BtnA.wasPressed()) { //Change Volume(Button A)
    changeVolume();
  }

  if (m5.BtnB.wasPressed()) { //Change Station(Button B)
    clearTrack();
    changeStation();
    stopPlaying();
    playMP3();
  }

  if (m5.BtnC.wasPressed()) { //Mute  (Button C)
    audioGain = -1.0;
    changeVolume();
  }

  // Update the battery voltage, and WiFi Signal every second
  if ((disUpdate + 1000) < millis()) {
    disUpdate = millis();
    displayBattery();
    updateWiFiSignal();
  }
}

