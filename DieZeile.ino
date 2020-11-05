////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Die Zeile
//  with 8 Matrix 8x8 LED arrays (Display module MAX7219)
//
//  Wifi web server displaying Clock, Math Clock and
//  allowing to scroll a message on the display.
//
//  Modifications by Claudio Laloni
//    - http post command 'set' added
//    - changed behaviour of new text setting in MODE_SCROLLING_TEXT
//        the new text is now displayed after the display of the last text has been completed (instead of
//        cleaning teh old and displaying the new text)
//    - message queue (MQ) mechanism added
//        a buffer (array of message texts) with MAX_NOF_MQMSG entries is used to store messages that 
//        subsequently displayed. Functions for message queue management:
//          mqInit()    initialization of MQ
//          mqClear()   deletion of all messe queue entries
//          mqAdd()     adds a text to the message queue (ring buffer mode)
//          mqAddAt()   adds/replaces a text at a specific position in the message queue
//          mqDelete()  deletes the message at a specific position in the queue
//          mqPrint()   auxiliary function, prints out MQ status for debugging purposes
//        an additional 'delimiter' text can be set to be displayed between subsequent messages
//    - text clock added
//        display the time as a (german) text string, like 'Viertel vor Acht'
//    - eeprom initialization modified to eliminate need of flashing the device twice
//    - configurable secrets added
//        the secrets are read from file secrets.txt that is stored in the SPIFFS file system at system startup
//        instead of using hard-coded texts
//    - secrets function modified
//        secrets are displayed at random time intervals. Lower and upper limit of the time interval can be
//        specified. 
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "FontData.h"
#include <FS.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <string.h>
#include <time.h>
#include <WebSocketsServer.h>

#define DEBUG_MQ
//#define DEBUG_CLOCK

#define DATA_PIN                           13 // DIN Scroll Matrix
#define LOAD_PIN                           15 // CS  Scroll Matrix
#define CLOCK_PIN                          14 // CLK Scroll Matrix
#define MAX_NOF_DISPLAYS                    8 // Number of used Led matrices 
#define HARDWARE_TYPE     MD_MAX72XX::FC16_HW // Display type FC16

#define CONNECT_AP_TIMEOUT              10000 // Timeout for connecting AP
#define MATH_CLOCK_INTERVAL             30000 // Interval for math clock to display new task

#define CHAR_SPACING                        1 // Number of columns between two characters
#define MSG_SPACING                         8 // Number of columns between two characters
#define MAX_NOF_CHARS_OF_MSG              255 // maxlength of 80 on website (i.e. 3 utf-8 chars for €) (maximum length of string = 600)
#define MAX_NOF_CHARS_OF_AP               100 // maxlength of 30 on website (i.e. 3 utf-8 chars for €)

#define MAX_NOF_MQMSG                       3 // maximum nuber of messages in message queue

#define MODE_CLOCK                          1 // Normal CLock mode
#define MODE_MATH_CLOCK                     2 // Math Clock mode
#define MODE_ADDONLY_CLOCK                  3 // Add Only Clock mode
#define MODE_PROGRESS_TIME                  4 // Scrolling Text mode
#define MODE_SCROLLING_TEXT                 5 // Scrolling Text mode
#define MODE_TEXT_CLOCK                     6 // Text Clock mode
#define MODE_SET_THEORY_CLOCK               7 // Text Clock mode
#define MODE_BOLD_CLOCK                     8 // Clock mode with bold digits (no date)

#define SCROLL_MIN_SPEED                    5 // Minimum speed for scrolling text
#define SCROLL_MAX_SPEED                   42 // Maximum speed for scrolling text

#define SECRET_SHOW_PROBABILITY           42 // Inverse of probability of showing the secret if there is on for the current minute


/*************************************************************************************************************************************/
/************************************************   G L O B A L   V A R I A B L E S   ************************************************/
/*************************************************************************************************************************************/

const char *ssidAP       = "Die Zeile";  // The name of the Wi-Fi network that will be created
const char *passwordAP   = "DieZeile";   // The password required to connect to it, leave blank for an open network

const char *OTAName      = "DieZeile";   // A name and a password for the OTA service
const char *OTAPassword  = "hrlbrmpf"; 

const char* mdnsName     = "DieZeile";   // Domain name for the mDNS responder

char initMessage[] = "Bitte mit \"Die Zeile\" verbinden (Passwort \"DieZeile\") und IP 192.168.4.1 aufrufen.";
char currentMessage[MAX_NOF_CHARS_OF_MSG];

char MQ[MAX_NOF_MQMSG][MAX_NOF_CHARS_OF_MSG];   // message queue buffer
char MQD[MAX_NOF_CHARS_OF_MSG];                 // delimiter text shown between messages
char MQS[MAX_NOF_CHARS_OF_MSG];                 // secret message
int  MQcount=0;                                 // number of messages in message queue buffer
int  MQnextAdd=0;                               // next buffer position to be filled with new message
int  MQnextDisplay=0;

bool recordDisplayBuffer=false;
uint8_t displayBuffer[64];

int  lastMode=0;                         // 
bool display_mode_changed  = false;      // Remember if display mode changed
bool connectedAP           = false;      // Remember if connected to an external AP
bool validTime             = false;
unsigned long prevMillis   = millis();   // Initialize current millisecond
struct tm *currentTime;                  // Store current time in this variable

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLOCK_PIN, LOAD_PIN, MAX_NOF_DISPLAYS); // Initialize display HW

ESP8266WebServer server(80);             // Create a web server on port 80
WebSocketsServer webSocket(81);          // Create a websocket server on port 81

File fsUploadFile;                       // A File variable to temporarily store the received file

struct {                                 // config struct storing all relevant data in EEPROM
    char MAC[18];                        // 'xx.xx.xx.xx.xx.xx'
    uint8_t brightness;
    uint8_t mode;
    uint8_t speed;
    char message[MAX_NOF_CHARS_OF_MSG];
    char ssid[MAX_NOF_CHARS_OF_AP];
    char password[MAX_NOF_CHARS_OF_AP];
} config;

typedef struct t_Secret {
  int day;
  int month;
  char *secret;
} Secret;
Secret *Secrets=NULL;


/*************************************************************************************************************************************/
/*************************************************   H E L P E R   F U N C T I O N S   ***********************************************/
/*************************************************************************************************************************************/

// Determine the filetype of a given filename, based on the extension
String getContentType(String filename)
{
    if(filename.endsWith(".html"))      return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js"))  return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".gz"))  return "application/x-gzip";
    return "text/plain";
}

// Get current time and store it in the global variable currentTime
void updateTime(void)
{
    time_t t = time(NULL);
    currentTime = localtime(&t);

    if(currentTime->tm_year>119)  // 119 years since 1900
      validTime=true;
}



/*************************************************************************************************************************************/
/**********************************************************   E E P R O M   **********************************************************/
/*************************************************************************************************************************************/

void eepromReadConfig(void)
{
    String eepromMAC;
    String WiFiMAC=String(WiFi.macAddress());
    
    EEPROM.begin(512);
    EEPROM.get(0, config);
    EEPROM.end();
    
    eepromMAC=config.MAC;
    if(eepromMAC!=WiFiMAC)    // eeprom contains invalid data -> not yet initialized
    {
        Serial.printf("Initilizing default configuration\n");
        sprintf(config.MAC, "%s", WiFiMAC.c_str());
        config.brightness = 1;
        config.mode = 2;
        config.speed = 32;
        strcpy(config.message, "Die Zeile !");
        strcpy(config.ssid, "YOUR AP HERE");
        strcpy(config.password, "YOUR PWD HERE");
        eepromWriteConfig();
    }
    else
    {
        Serial.printf("Reading configuration from eeprom\n");
        printConfiguration();
    }

    //mqAddAt(0, String(config.message));
}

void eepromWriteConfig(void)
{
    Serial.printf("Writing configuration to eeprom\n");
    printConfiguration();
   
    EEPROM.begin(512);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
}

void printConfiguration()
{
    Serial.printf("Configuration:\n");
    Serial.printf("MAC:        %s\n", config.MAC);
    Serial.printf("brightness: %d\n", config.brightness);
    Serial.printf("mode:       %d\n", config.mode);
    Serial.printf("speed:      %d\n", config.speed);
    Serial.printf("message:    %s\n", config.message);
    Serial.printf("ssid:       %s\n", config.ssid);
    Serial.printf("password:   %s\n\n", config.password);        
}

/*************************************************************************************************************************************/
/*************************************************   S E R V E R   H A N D L E R S   *************************************************/
/*************************************************************************************************************************************/

// If the requested file or page doesn't exist, return a 404 not found error
void handleNotFound()
{
    if(!handleFileRead(server.uri())) // Check if the file exists in the flash memory (SPIFFS), if so, send it
    {
        server.send(404, "text/plain", "404: File Not Found");
    }
}

// Send the right file to the client (if it exists)
bool handleFileRead(String path)
{
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
    String contentType = getContentType(path);              // Get the MIME type
    String pathWithGz = path + ".gz";
    if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))    // If the file exists, either as a compressed archive, or normal
    {
        if(SPIFFS.exists(pathWithGz))                       // If there's a compressed version available
        {
            path += ".gz";                                  // Use the compressed verion
        }
        File file = SPIFFS.open(path, "r");                 // Open the file
        size_t sent = server.streamFile(file, contentType); // Send it to the client
        file.close();                                       // Close the file again
        Serial.println(String("\tSent file: ") + path);
        return true;
    }
    Serial.println(String("\tFile Not Found: ") + path);    // If the file doesn't exist, return false
    return false;
}

// Change settings via set HTTP command 'set'
// Example:  http://DieZeile.local/set?B=12
//
void handleSet()
{
    Serial.printf("received /set\n"); 
    int argcount=server.args();

    for(int i=0; i<argcount; i++) 
    {
        String argName=server.argName(i);
        if(argName == "B" || argName == "brightness" )
            setBrightness((int)(server.arg(argName.c_str())).toInt());

        else if(argName == "S" || argName == "speed" )
            setSpeed((int)(server.arg(argName.c_str())).toInt());

        else if(argName == "M" || argName == "mode" )
            setMode((int)(server.arg(argName.c_str())).toInt());

        else if(argName == "T" || argName == "text" )
            setText(server.arg(argName.c_str()));

        else if(argName == "X" || argName == "save" )
            setSaveConfig();

        else if(argName == "A" || argName == "mqAdd" )
            mqAdd(server.arg(argName.c_str()));

        else if(argName == "a" || argName == "mqSetDelimiter" )
            mqDelimiter(server.arg(argName.c_str()));

        else if(argName == "D" || argName == "mqDelete" )
            mqDelete((int)(server.arg(argName.c_str())).toInt());

        else if(argName.charAt(0) == 'A')
        {
            String argIndex=argName;
            argIndex.remove(0, 1);
            int index=argIndex.toInt();
            Serial.printf("argName=%s argIndex=%s index=%d\n", argName.c_str(), argIndex.c_str(), index);
            mqAddAt(index, server.arg(argName.c_str()));
        }
    }

    // return current settings 
    String settings =  "B=" + String(config.brightness) +
                      " M=" + String(config.mode) + 
                      " S=" + String(config.speed) +
                      " Message=" + String(config.message) + "\n";
    for(int i=0; i<MAX_NOF_MQMSG; i++)
        settings += "MQ[" + String(i) + "]='" + String(MQ[i]) + "'\n";
    settings += "MQD  ='" + String(MQD) + "'\n";
                     
    server.send(200, "text/plain", settings.c_str());
}

// Upload a new file to the SPIFFS
void handleFileUpload()
{
    HTTPUpload& upload = server.upload();
    String path;
    if(upload.status == UPLOAD_FILE_START)
    {
        path = upload.filename;
        if(!path.startsWith("/"))
        {
            path = "/"+path;
        }
        if(!path.endsWith(".gz"))                               // The file server always prefers a compressed version of a file 
        {
            String pathWithGz = path+".gz";                     // So if an uploaded file is not compressed, the existing compressed
            if(SPIFFS.exists(pathWithGz))                       // version of that file must be deleted (if it exists)
            {
                SPIFFS.remove(pathWithGz);
            }
        }
        Serial.print("handleFileUpload Name: "); Serial.println(path);
        fsUploadFile = SPIFFS.open(path, "w");                  // Open the file for writing in SPIFFS (create if it doesn't exist)
        path = String();
    }
    else if(upload.status == UPLOAD_FILE_WRITE)
    {
        if(fsUploadFile)
        {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    }
    else if(upload.status == UPLOAD_FILE_END)
    {
        if(fsUploadFile)                                        // If the file was successfully created
        {                                    
            fsUploadFile.close();                               // Close the file again
            Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
            server.sendHeader("Location","/success.html");      // Redirect the client to the success page
            server.send(303);
        }
        else
        {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

// When a WebSocket message is received
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght)
{ 
    IPAddress ip;
  
    switch(type)
    {
        case WStype_DISCONNECTED:                                    // If the websocket is disconnected
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:                                       // If a new websocket connection is established
            ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            if(WiFi.softAPgetStationNum() == 0)                      // If the ESP is connected to an AP
            {
                webSocket.sendTXT(num, "c" + String("SSID: ") + String(WiFi.SSID()) + "<br>IP: " + String(WiFi.localIP().toString()));
                connectedAP = true;
            }
            else                                                     // If a station is connected to the ESP SoftAP
            {
                webSocket.sendTXT(num, "c" + String("SSID: ") + String(ssidAP) + "<br>IP: " + String("192.168.4.1"));
                connectedAP = false;
            }
            eepromReadConfig();                                      // Read configuration from EEPROM
            webSocket.sendTXT(num, "b" + String(config.brightness));
            webSocket.sendTXT(num, "m" + String(config.mode));
            webSocket.sendTXT(num, "s" + String(SCROLL_MAX_SPEED - config.speed + SCROLL_MIN_SPEED));
            webSocket.sendTXT(num, "t" + String(config.message));
            webSocket.sendTXT(num, "i" + String(config.ssid));
            webSocket.sendTXT(num, "p" + String(config.password));
            clearDisplay();
            strcpy(currentMessage, config.message);                  // Copy config message in case startAP() has overwritten it
            display_mode_changed = true;                             // Set display_mode_changed for clocks to be set at once
            break;
        case WStype_TEXT:                                            // If new text data is received
            Serial.printf("[%u] get Text: %s\n", num, payload);
            if(payload[0] == 'B')                                    // We get brightness
            {
                setBrightness((int)strtol((const char *)&payload[1], NULL, 10));
            }
            else if(payload[0] == 'S')                               // We get speed for scrolling text
            {
                setSpeed((int)strtol((const char *)&payload[1], NULL, 10));
            }
            else if(payload[0] == 'T')                               // We get text to scroll
            {
                String Text=(const char *)&payload[1];
                setText(Text);
                webSocket.sendTXT(num, "m" + String(config.mode));
            }
            else if(payload[0] == 'M')                               // We get mode
            {
                setMode((int)strtol((const char *)&payload[1], NULL, 10));
            }
            else if(payload[0] == 'I')                               // We get SSID
            {
                strcpy(config.ssid, (const char *)&payload[1]);
            }
            else if(payload[0] == 'P')                               // We get Password
            {
                strcpy(config.password, (const char *)&payload[1]);
            }
            else if(payload[0] == 'X')                               // We get save config command
            {
                setSaveConfig();
            }
            break;
    }
}

/*************************************************************************************************************************************/
/***************************************************   S E T   F U N C T I O N S  ****************************************************/
/*************************************************************************************************************************************/
void setBrightness(int newBrightness)
{
    config.brightness = newBrightness;
    mx.control(MD_MAX72XX::INTENSITY, config.brightness);  

    Serial.printf("set brightness=%d\n", config.brightness);
}

void setSpeed(int newSpeed)
{
    config.speed = SCROLL_MAX_SPEED - (newSpeed - SCROLL_MIN_SPEED);

    Serial.printf("set speed=%d\n", config.speed);
}

void setMode(int newMode)
{
    if(config.mode!=newMode) {
      config.mode = newMode;
      if(config.mode == MODE_SCROLLING_TEXT)
      {
        strcpy(currentMessage, "");
        if(MQcount==0) 
          mqAddAt(0, String(config.message));
      }
      display_mode_changed = true;
   
      Serial.printf("set mode=%d\n", config.mode);
    }
    else
      Serial.printf("set mode=%d (no change)\n", config.mode);
}

void setText(String newText)
{
    mqClear();
    mqAddAt(0, newText);
    strcpy(config.message, MQ[0]); // Copy it into config struct
    config.mode = MODE_SCROLLING_TEXT;  
}

void setSaveConfig()
{
    eepromWriteConfig();
    delay(200);
    ESP.restart();
}

/*************************************************************************************************************************************/
/***************************************************   M E S S A G E  Q U E U E   ****************************************************/
/*************************************************************************************************************************************/
// A queue (MQ) to store up to to MAX_NOF_MSMSG messages to be displayed. The messages stored in MQ are subsequently displayed. 
// An optional 'delimiter text' (MQD) that is shown between subsequent messages can be defined. 
// All MQ related function do not return any error codes, they simply do nothing if parameters are invalid.

// ------------------------------------------------------------------------------------------------------------------------------------
// mqInit() initializes the message queue
void mqInit()
{
  #ifdef DEBUG_MQ  
  Serial.printf("mqInit()\n");
  #endif  
  strcpy(MQD, "");                      // clear delimiter buffer
  strcpy(MQS, "");                      // clear secret buffer
  mqClear();                            // clear message buffers  

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}


// ------------------------------------------------------------------------------------------------------------------------------------
// mqClear() clears all messages in queue
void mqClear()
{
  MQcount=0;
  MQnextAdd=0;
  MQnextDisplay=0;

  for(int i=0; i<MAX_NOF_MQMSG; i++)
    strcpy(MQ[i], "");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// mqAdd() uses the message queue as a ring buffer and adds/replaces the new message at position 'MQnextAdd'
void mqAdd(String text)
{
  #ifdef DEBUG_MQ  
  Serial.printf("mqAdd(%s)\n", text.c_str());
  #endif
  
  if(text.c_str()[0] != '\0')                                 // valid (non empty) string
  {
      if(MQ[MQnextAdd][0]=='\0')                              // new entry and no replacement
            MQcount++;
      strcpy(MQ[MQnextAdd], text.c_str());                    // add new (or replace existing) message
      MQnextAdd=++MQnextAdd%MAX_NOF_MQMSG;                    // update ring buffer pointer
  }

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// mqAddAt() sets a message at queue position 'index'. 
void mqAddAt(int index, String text)
{
  #ifdef DEBUG_MQ  
  Serial.printf("mqAddAt(%d, %s)\n", index, text.c_str());
  #endif

  //if(0<=index && index<=MQcount && (text.c_str())[0]!='\0')   // valid index and no empty string
  if(0<=index && index<=MAX_NOF_MQMSG && (text.c_str())[0]!='\0')   // valid index and no empty string
  {
      if(MQ[index][0]=='\0')                                  // new entry and no replacement?
            MQcount++;
      strcpy(MQ[index], text.c_str());                        // add new (or replace existing) message
  }

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// mqDelete() deletes the queue element at position 'index'
void mqDelete(int index)
{
  Serial.printf("mqDelete(%d)\n", index);

  if(0<=index && index<MAX_NOF_MQMSG &&                       // valid index and
      MQ[index][0]!='\0')                                     //   MQ entry not empty
  {
      strcpy(MQ[index], "");
      MQcount--;
  }

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// sets the queue delimiter text
void mqDelimiter(String delimiter)
{
  strcpy(MQD, delimiter.c_str());

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// printd queue elements for debugging purposes 
void mqPrint()
{
  Serial.printf("MQcount=%d MQnextAdd=%d\n", MQcount, MQnextAdd);
  for(int i=0; i<MAX_NOF_MQMSG; i++)
      Serial.printf("  MQ[%d]='%s'\n", i, (MQ[i][0]?MQ[i]:"<empty>"));
  Serial.printf("  MQD  ='%s'\n", (MQD[0]?MQD:"<empty>"));
  Serial.printf("  MQS  ='%s'\n", (MQS[0]?MQS:"<empty>"));
}

/*************************************************************************************************************************************/
/******************************************************   M A T R I X   R O W   ******************************************************/
/*************************************************************************************************************************************/

void cbScrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
{
  /*
  unsigned char b=(unsigned char) col;
  unsigned char bits[9];
  int j;
  static int recordIndex=0;

  if(recordDisplayBuffer) {
    displayBuffer[recordIndex++]=col;
    for(int j=7; j>=0; j--) 
      if((col >> j) & 1)
        bits[7-j]='X';
      else
        bits[7-j]=' ';
    bits[8]=0;

    Serial.printf("%s\n", bits);

    if(recordIndex==64) {
      recordDisplayBuffer=false;
      recordIndex=0;
    }
  }
  */
}

char *nextMessage()
{
    static enum { S_QUEUEorSECRET, S_DELIMITER, S_SECRET_END } state = S_QUEUEorSECRET;
    char *message="";

    switch(state) 
    {
      case S_QUEUEorSECRET:
          {
              if(MQS[0]!='\0') {                                    // secret defined?
                message=MQS;                                        //   display as message
                state=S_SECRET_END;                                 //   
              }
              else {
                if(MQcount>0) {
                    bool found=false;
                    for(int i=0; i<MAX_NOF_MQMSG && !found; i++) {  // cycle through message queue to find next
                          if(MQ[MQnextDisplay][0]!='\0') {          // message found
                              message=MQ[MQnextDisplay];            // show message
                              found=true;
                          }
                          MQnextDisplay=++MQnextDisplay%MAX_NOF_MQMSG;
                    }
                    if(MQD[0]!='\0')                                // if delimiter is defined
                        state=S_DELIMITER;                          //   schedule delimiter as next message
                    else 
                        state=S_QUEUEorSECRET;                      //   schedule next message
                }
             }
          }
          break;
          
       case S_DELIMITER:
          {
              message=MQD;                                          // display delimiter
              state=S_QUEUEorSECRET;                                // schedule next message
          }
          break;

      case S_SECRET_END:
          {
              strcpy(MQS, "");
              Serial.printf("config.mode=%d <- lastMode=%d\n", config.mode, lastMode);
              if(lastMode!=config.mode) {                           // last mode before secret != SCROLLING
                config.mode=lastMode;
                display_mode_changed=true;
              }
              if(MQD[0]!='\0')                                      // if delimiter is defined
                  state=S_DELIMITER;                                //   schedule delimiter as next message
              else 
                  state=S_QUEUEorSECRET;                            //   schedule next message
          }
          break;
          
      default:
          state=S_QUEUEorSECRET;
    }          

    //Serial.printf(">>> next message: %s\n", message);
    return message;
}

uint8_t cbScrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
{
    static enum { S_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE } state = S_IDLE;
    static char *p;
    static uint16_t curLen, showLen;
    static uint8_t cBuf[8];
    uint8_t colData = 0;
    char cc;

    // Finite state machine to control what we do on the callback
    switch(state)
    {
        case S_IDLE:                        // Reset the message pointer and check for new message to load
            strcpy(currentMessage, nextMessage());              // copy next message to be shown to buffer
  
            if(currentMessage[0]!=0) {
                p=currentMessage;
                state = S_NEXT_CHAR;
            }
            break;
        case S_NEXT_CHAR:                   // Load the next character from the font table
            if(*p == '\0')
            {
                state = S_IDLE;
            }
            else
            {
                if(*p == 195)               // Convert UTF-8 characters with 2 bytes to 1 byte
                {
                    p++;
                    switch(*p++)
                    {
                        case 164:           // ä
                            cc = 128;
                            break;
                        case 182:           // ö
                            cc = 129;
                            break;
                        case 188:           // ü
                            cc = 130;
                            break;
                        case 132:           // Ä
                            cc = 131;
                            break;
                        case 150:           // Ö
                            cc = 132;
                            break;
                        case 156:           // Ü
                            cc = 133;
                            break;
                        case 159:           // ß
                            cc = 134;
                            break;
                        deafault:
                            break;
                    }
                }
                else if((*p == 226) && (*(p + 1) == 130) && (*(p + 2) == 172)) // Euro character
                {
                    p += 3;
                    cc = 135;
                }
                else if((*p == 194) && (*(p + 1) == 167))                      // Paragraph character
                {
                    p += 2;
                    cc = 20;
                }
                else {
                    cc = *p++;
                }
                showLen = mx.getChar(cc, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
                curLen = 0;
                state = S_SHOW_CHAR;
            }
            break;
        case S_SHOW_CHAR:                                                      // Display the character
            colData = cBuf[curLen++];
            if(curLen < showLen)
            {
                break;
            }
            //showLen = (*p != '\0' ? CHAR_SPACING : (MAX_NOF_DISPLAYS * COL_SIZE) / 2); // Set up the inter character spacing
            showLen = (*p != '\0' ? CHAR_SPACING : MSG_SPACING); // Set up the inter character/inter message spacing
            curLen = 0;
            state = S_SHOW_SPACE;
        case S_SHOW_SPACE:                                                     // Display inter-character spacing (blank column)
            curLen++;
            if(curLen == showLen)
            {
                state = S_NEXT_CHAR;
            }
            break;
        default:
            state = S_IDLE;
    }

    return(colData);
}

void clearDisplay(void)
{
    strcpy(currentMessage, "");
    mx.transform(MD_MAX72XX::TSL);     // Cancel scrolling by calling callback one time with empty string
    mx.clear();                        // Clear display
}


/*************************************************************************************************************************************/
/******************************************************   M A T H   C L O C K   ******************************************************/
/*************************************************************************************************************************************/

int (*operators[4])(int, int);
char operatorTexts[] = "+-*:";

int sum(int a, int b)
{
    return a + b;
}

int sub(int a, int b)
{
    return a - b;
}

int mul(int a, int b)
{
    return a * b;
}

int divi(int a, int b)
{
    if(b == 0 || a % b != 0) // Dummy value for non-divisible numbers
    {
        return 9999;
    }
  return a / b;
}

/*
 *  Initialize CLock
 */
void startMathClock()
{
    operators[0] = sum;
    operators[1] = sub;
    operators[2] = mul;
    operators[3] = divi;
}

/*
 *  Evaluates the mathematical expression x op1 y op2 z assuming that the result will be integer. Unsuccessful
 *  divisions will be replaced by 9999 in the process
 */
int eval(int x, int y, int z, int op1, int op2)
{
    if(op1 == 3 || (op1 == 2 && op2 != 3) || (op1 == 1 && op2 < 2)) //If first operator should get executed first
    {
        return operators[op2](operators[op1](x, y), z);
    }
    else
    {
        return operators[op1](x, operators[op2](y, z));
    }
}

/*
 * Generates a random formula of the form a op1 b op2 c, whose result will be the specified number.
 * a, b, c are one-digibt numbers and op1, op2 are the 4 basic arithmetic operators.
 * The formula will be drawn uniformly from all such possible formulas.
 */
void getFormulaRepresentation(char* result, int number)
{
    int foundSolutions = 0;
    // Iterate all possible combinations and sample those possible for current number
    for(int x = 0; x < 10; x++)
    {
        for(int y = 0; y < 10; y++)
        {
            for(int z = 0; z < 10; z++)
            {
                for(int op1 = 0; op1 < 4; op1++)
                {
                    for(int op2 = 0; op2 < 4; op2++)
                    {
                        if(eval(x, y, z, op1, op2) == number)
                        {
                            if(random(0, foundSolutions + 1) == 0) //Reservoir sampling
                            {
                                sprintf(result, "%d%c%d%c%d", x, operatorTexts[op1], y, operatorTexts[op2], z);
                            }
                            foundSolutions++;
                        }
                    }
                }
            }
        }
    }
}



void getFormulaRepresentationAddOnly(char* result, int number)
{
    int foundSolutions = 0;
    // Iterate all possible combinations and sample those possible for current number
    for(int x = 1; x < 10; x++)
    {
        for(int y = 0; y < 10; y++)
        {
            for(int z = 1; z < 10; z++)
            {
                for(int a = 0; a < 10; a++)
                {
                    for(int op1 = 0; op1 < 4; op1++)
                    {
                        if(10 * x + y + 10 * z + a == number)
                        {
                            if(random(0, foundSolutions + 1) == 0) //Reservoir sampling
                            {
                                sprintf(result, "%d%d+%d%d", x, y, z, a);
                            }
                            foundSolutions++;
                        }        
                    }
                }
            }
        }
    }
    for(int x = 1; x < 10; x++)
    {
        for(int y = 0; y < 10; y++)
        {
            for(int z = 1; z < 10; z++)
            {
                if(x + y + z == number)
                {
                    if(random(0, foundSolutions + 1) == 0) //Reservoir sampling
                    {
                        sprintf(result, "%d+%d+%d", x, y, z);
                    }
                    foundSolutions++;
                }        
            }
        }
    }  
}



/*************************************************************************************************************************************/
/**************************************************   M O D E   F U N C T I O N S   **************************************************/
/*************************************************************************************************************************************/

void showClock(bool monthAsString)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    static char *Month[] = { "Jan", "Feb", "Mär", "Apr", "Mai", "Jun", 
                             "Jul", "Aug", "Sep", "Okt", "Nov", "Dez" }; 

    if((currentTime->tm_min != lastMinute) || (display_mode_changed == true))
    {          
        if(display_mode_changed)              display_mode_changed = false;
        if(currentTime->tm_min != lastMinute) lastMinute = currentTime->tm_min;

        char timeString[13] = "00:00 00.00."; // or "00:00 DD.MMM" (00:00 MMM DD)
        if(validTime)
        {
            clearDisplay();
            timeString[0] = 0x30 + currentTime->tm_hour / 10;
            timeString[1] = 0x30 + currentTime->tm_hour - (currentTime->tm_hour / 10) * 10;
            timeString[3] = 0x30 + currentTime->tm_min / 10;
            timeString[4] = 0x30 + currentTime->tm_min - (currentTime->tm_min / 10) * 10;
            if(monthAsString)
            {
                timeString[6]  = 0x30 + currentTime->tm_mday / 10;
                timeString[7]  = 0x30 + currentTime->tm_mday - (currentTime->tm_mday / 10) * 10;
                timeString[9]  = Month[currentTime->tm_mon][0];
                timeString[10] = Month[currentTime->tm_mon][1];
                timeString[11] = Month[currentTime->tm_mon][2];
            }
            else
            {
                timeString[6] = 0x30 + currentTime->tm_mday / 10;
                timeString[7] = 0x30 + currentTime->tm_mday - (currentTime->tm_mday / 10) * 10;
                timeString[9]  = 0x30 + (currentTime->tm_mon + 1) / 10;
                timeString[10] = 0x30 + (currentTime->tm_mon + 1) - ((currentTime->tm_mon + 1) / 10) * 10;
            }
        }
        mx.setChar(63, timeString[0]);      // Hour 1st digit
        mx.setChar(57, timeString[1]);      // Hour 2nd digit
        mx.setChar(51, 147);                // use small : 
        mx.setChar(48, timeString[3]);      // Minutes 1st digit
        mx.setChar(42, timeString[4]);      // minutes 2nd digit
        if(monthAsString)
        {
            mx.setChar(31, timeString[6]);      // Day 1st digit
            mx.setChar(25, timeString[7]);      // Day 2nd digit
            mx.setChar(19, 138);                // small point 
            mx.setChar(16, timeString[9]);      // Month 1st char
            mx.setChar(10, timeString[10]);     // Month 2nd char
            mx.setChar(4,  timeString[11]);     // Month 3rd char
        }
        else
        {
            mx.setChar(28, timeString[6]);      // Day 1st digit
            mx.setChar(22, timeString[7]);      // Day 2nd digit
            mx.setChar(16, 138);                // use small point and display in comressed form to support two dots
            mx.setChar(13, timeString[9]);      // Month 1st digit
            mx.setChar(7,  timeString[10]);     // Month 2nd digit
            mx.setChar(1,  138);                // use small point and display in comressed form to support two dots
        }
        //mqAddAt(0, String(timeString)); 
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
void showBoldClock(bool showSeconds)
{
    static uint8_t lastSecond = 60;     // initialize for first time with not defined minute value
    static uint8_t lastDigits[8] = { 11, 11, 11, 11, 11, 11, 11, 11};
    uint8_t digits[8];            
    int hour   =currentTime->tm_hour;
    int minutes=currentTime->tm_min;
    int seconds=currentTime->tm_sec;
    static uint8_t boldChars[][5] = { { B11111111, B11111111, B11000011, B11111111, B11111111 }, // 148 - 0 (Bold)
                                      { B00000000, B00000000, B00000000, B11111111, B11111111 }, // 149 - 1 (Bold)
                                      { B11111011, B11111011, B11011011, B11011111, B11011111 }, // 150 - 2 (Bold)
                                      { B11000011, B11011011, B11011011, B11111111, B11111111 }, // 151 - 3 (Bold)
                                      { B00011111, B00011111, B00011000, B11111111, B11111111 }, // 152 - 4 (Bold)
                                      { B11011111, B11011111, B11011011, B11111011, B11111011 }, // 153 - 5 (Bold)
                                      { B11111111, B11111111, B11011011, B11111011, B11111011 }, // 154 - 6 (Bold)
                                      { B00000011, B00000011, B00000011, B11111111, B11111111 }, // 155 - 7 (Bold)
                                      { B11111111, B11111111, B11011011, B11111111, B11111111 }, // 156 - 8 (Bold)
                                      { B11011111, B11011111, B11011011, B11111111, B11111111 }, // 157 - 9 (Bold)
                                      { B00000000, B11101110, B11101110, B11101110, B00000000 }  // 158 - : (Bold)
                                    };
    
    if((seconds!=lastSecond) || (display_mode_changed == true))
    {          
      if(display_mode_changed) {
        display_mode_changed = false;
        clearDisplay();
      }
      lastSecond=seconds;

      if(validTime)
      {
        digits[0] = hour / 10;
        digits[1] = hour - (hour / 10) * 10;
        digits[2] = 10;
        digits[3] = minutes / 10;
        digits[4] = minutes - (minutes / 10) * 10;
        digits[5] = 10;
        digits[6] = seconds / 10;
        digits[7] = seconds - (seconds / 10) * 10;
      }
      
      for(int i=0; i<(showSeconds?8:5); i++) 
        for(int j=0; j<5; j++) {
          mx.setColumn(60-i*7-j, boldChars[digits[i]][j]);  
        }

      //mqAddAt(0, String(timeString)); 
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
void showProgressClock(void)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    
    if((currentTime->tm_min != lastMinute) || (display_mode_changed == true))
    {          
        clearDisplay();

        int dots = (currentTime->tm_hour * 60 + currentTime->tm_min) * (8 * 8 * 8) / (24 * 60);
        if(display_mode_changed)              display_mode_changed = false;
        if(currentTime->tm_min != lastMinute) lastMinute = currentTime->tm_min;
        
        int current_cursor = 63; // Set location at start of LED matrix
        // While not all dots printed, print them in the corresponding representation with as many full columns
        // as possible and then a partial column
        while (dots > 0) 
        {
            if (dots >= 8) {
                mx.setChar(current_cursor, 146); //full column
                dots -= 8;
                current_cursor--;
            } else {
                mx.setChar(current_cursor, 138 + dots);
                dots = 0;
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// show a set theory clock
//    The diplay is organized in three sections Hour Minutes Seconds
//    The time is represented by as h12 + h4 + h : m15 + m5 + m : s30 + s5 + s
//    where  '[h|m|s]Number' represents the number of 

void showSetTheoryClock(void)
{
    static int last_second=60;
    int hour=currentTime->tm_hour;
    int minutes=currentTime->tm_min;
    int seconds=currentTime->tm_sec;
    int h12=0, h4=0, h=0;
    int m15=0, m5=0, m=0;
    int s=0, s5=0, s30=0;
    
    if((seconds != last_second) || (display_mode_changed == true)) {
        char timeString[6];

        if(display_mode_changed) {
          display_mode_changed = false;
          clearDisplay();
        }
        last_second = seconds;

        h12= hour/12;
        h4 =(hour-h12*12)/4;
        h  = hour-h12*12-h4*4;
        
        m15= minutes/15;
        m5 =(minutes-m15*15)/5;
        m  = minutes-m15*15-m5*5;

        s30= seconds/30;
        s5 =(seconds-s30*30)/5;
        s  = seconds-s30*30-s5*5;

        //h12=h4=h=m15=m5=m=s30=s5=s=5;                           // alle squares on! for testing

        displaySquare(63, 7, 8, 8, (h12?true:false));             // 12 hours

        for(int i=0; i<2; i++)
          displaySquare(54,  7-i*5, 6, 3, (i<h4?true:false));     // 4 hours
          
        for(int i=0; i<3; i++)                                    // hours
          displaySquare(47, 7-i*3, 3, 2, (i<h?true:false));

        for(int i=0; i<3; i++)                                    // 15 minutes
          displaySquare(35, 7-i*3, 8, 2, (i<m15?true:false));
          
        for(int i=0; i<2; i++)                                    // 5 minutes
          displaySquare(26, 7-i*5, 3, 3, (i<m5?true:false));

        for(int i=0; i<4; i++)                                    // minutes
          displaySquare(22, 7-i*2, 3, 1, (i<m?true:false));

        displaySquare(10, 7, 3, 8, (s30?true:false));             // 30 seconds

        for(int i=0; i<2; i++)                                    // 5 seconds (2nd col)
          displaySquare(6, 7-i*3, 2, 2, (i+3<s5?true:false));
        for(int i=0; i<3; i++)                                    // 5 seconds
          displaySquare(3, 7-i*3, 2, 2, (i<s5?true:false));
        
        for(int i=0; i<4; i++)                                    // seconds
          displaySquare(0, 7-i*2, 1, 1, (i<s?true:false));

        //Serial.printf("%dx12 + %dx4 + %d : %dx15 + %dx5 + %d\n", h12, h4, h, m15, m5, m);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// display a square of sx *  sy led's at lower left position px,py on the matrtix display
void displaySquare(uint8_t px, uint8_t py, uint8_t sx, uint8_t sy, bool on)
{
  //Serial.printf("Square %d %d %d %d %s\n", px, py, sx, sy, (on?"on":"off"));    // debug output

  for(uint8_t x=0; x<sx; x++)
    for(uint8_t y=0; y<sy; y++)
      mx.setPoint(py-y, px-x, on); 
}

// ------------------------------------------------------------------------------------------------------------------------------------
void showMathClock(bool addonly)
{
    if((millis() > prevMillis + MATH_CLOCK_INTERVAL) || (display_mode_changed == true))
    {          
        prevMillis = millis();
        if(display_mode_changed) display_mode_changed = false;
  
        char timeString[12] = "0+0+0 0+0+0";
        if(validTime)
        {
            clearDisplay();

            if(addonly)
            {
                getFormulaRepresentationAddOnly(&timeString[0], currentTime->tm_hour);
                getFormulaRepresentationAddOnly(&timeString[6], currentTime->tm_min);
            }
            else
            {
                getFormulaRepresentation(&timeString[0], currentTime->tm_hour);
                getFormulaRepresentation(&timeString[6], currentTime->tm_min);
            }
        }
        for(int i = 0; i < 5; i++)
        {
            mx.setChar(63 - (i * 6), timeString[i]);
            mx.setChar(28 - (i * 6), timeString[i + 6]);
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
void showTextClock(bool longFormat)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    String timeText;
    static String HourName[] = { "Zwölf", "Eins", "Zwei", "Drei", "Vier", "Fünf", "Sechs", 
                                 "Sieben", "Acht", "Neun", "Zehn", "Elf" }; 

    if((currentTime->tm_min != lastMinute) || (display_mode_changed == true) && validTime)
    {          
      int Minute=currentTime->tm_min;
      int Hour=currentTime->tm_hour;

      lastMinute=Minute;

      if(display_mode_changed) {             
        display_mode_changed = false;
        clearDisplay();
      }

      if(longFormat) timeText="Es ist ";
      if(Minute<3 || Minute>=58)
        if(Hour==0)       timeText+=                    "Mitternacht";              // special case 'Mitternacht'
        else if(Hour==1)  timeText+=                    "Ein Uhr";                  // special case 'Eins' Uhr
        else              timeText+=                    HourName[Hour%12] + " Uhr"; // other full hour
      else if(Minute<8)   timeText+="Fünf nach "      + HourName[Hour%12];
      else if(Minute<13)  timeText+="Zehn nach "      + HourName[Hour%12];
      else if(Minute<18)  timeText+="Viertel nach "   + HourName[Hour%12];
      else if(Minute<23)  timeText+="Zwanzig nach "   + HourName[Hour%12];
      else if(Minute<28)  timeText+="Fünf vor halb "  + HourName[Hour%12];
      else if(Minute<33)  timeText+="Halb "           + HourName[++Hour%12];
      else if(Minute<38)  timeText+="Fünf nach halb " + HourName[++Hour%12];
      else if(Minute<43)  timeText+="Zwanzig vor "    + HourName[++Hour%12];
      else if(Minute<48)  timeText+="Dreiviertel "    + HourName[++Hour%12];
      else if(Minute<53)  timeText+="Zehn vor "       + HourName[++Hour%12];
      else if(Minute<58)  timeText+="Fünf vor "       + HourName[++Hour%12];
      else                timeText+="error";

      mqAddAt(0, timeText); 
      #ifdef DEBUG_CLOCK
      Serial.printf("Time: %02d:%02d\n", Hour, Minute);
      #endif
    }
    else if(!validTime) {
      timeText="no time information available";

      mqAddAt(0, timeText); 
      #ifdef DEBUG_CLOCK
      Serial.printf("Time: %sd\n", timeText.c_str());
      #endif
    }
}


void scrollText(uint8_t speed)
{
    static uint32_t prevTime = 0;

    if(millis() - prevTime >= speed)
    {
        mx.transform(MD_MAX72XX::TSL); // Scroll along - the callback will load all the data
        prevTime = millis();           // Starting point for next time
    }
}

/*************************************************************************************************************************************/
/*************************************************   S E C R E T S    F U N C T I O N S  *********************************************/
/*************************************************************************************************************************************/

// ------------------------------------------------------------------------------------------------------------------------------------
// readSecretsFromFile()
//    read a list of secrets from a the file secrets.txt in the SPIFFS file system
//    Each line in the file secrets.txt must have the following fixed format:
//    <int>, <int>, <any text>\n11
//
//    The first <int> represents the day, the second <int> the month and <any text> the secret message to be displayed
//    The last line must have the format:
//    -1, -1, <any text>\n
//
//    Warning: this is a very simple implementation without sophisticated parsing of the file structure!
// ------------------------------------------------------------------------------------------------------------------------------------
bool readSecretsFromFile()
{
  File secretFile;
  char *fileName="/secrets.txt";
  int nofLines=0;
  int iLine=0;

  Serial.printf("\nlooking for secrets...\n");
  if(!SPIFFS.exists(fileName)) {
    Serial.printf("could not find secrets file %s\n", fileName);
    return false;
  }

  secretFile=SPIFFS.open(fileName, "r");
  nofLines=0;                                         // determine the numbrer of lines in the file
  while(secretFile.available()) {                     // still chars to read
    char inChar=secretFile.read();    
    if(inChar=='\n')                                  // new line found
      nofLines++;
  }
  secretFile.close();  
  Serial.printf("found secrets.txt with %d entries\n", nofLines);

  Secrets=(Secret *)malloc(nofLines*sizeof(Secret));  
  if(Secrets==NULL) {                                 // malloc failed
    return false;                                     //   return
  }

  secretFile=SPIFFS.open(fileName, "r");
  #define MAX_FILE_LINESIZE 500
  char buffer[MAX_FILE_LINESIZE];
  char secret[MAX_FILE_LINESIZE];

  iLine=0;                                            // scan and parse all lines
  while(secretFile.available() && iLine<nofLines) {   // if still chars to read from file 
    int day;
    int month;
    int len=0;
    bool eolFound=false;
    do {                                              // read an entire line until '\n' found or EOF
      char inChar=secretFile.read();    
      if(inChar=='\n' || inChar==-1) {
        eolFound=true;
        buffer[len]=0;
      }
      else if(inChar!='\r')                           // copy char to buffer (except \CR char)
        buffer[len++]=inChar;
    }
    while(!eolFound);

    sscanf(buffer, "%d, %d, %[^\n]s\n",               // 'parse' line 
           &(Secrets[iLine].day), 
           &(Secrets[iLine].month), secret);
   
    int msgLen=strlen(secret);
    Secrets[iLine].secret=(char *)malloc(msgLen*sizeof(char)+1);
    strcpy(Secrets[iLine].secret, secret);
    
    Serial.printf("%02d: %02d %02d %s\n", iLine, Secrets[iLine].day, Secrets[iLine].month, Secrets[iLine].secret);
    iLine++;
  }
  
  secretFile.close();
  return true;
}

bool handleSecret()
{
    static unsigned long nextMillis = 0;
    int min=60;
    int max=360;

    if (MQS[0]=='\0' && millis() > nextMillis)                                    // no secret defined and next lookup time reached
    {       
        int day=currentTime->tm_mday;
        int month=currentTime->tm_mon+1;
        Secret *sp=Secrets;

        while(MQS[0]=='\0' && (*sp).day!=-1)                                      // no secret found AND secret list end not reached
        {
          if( (((*sp).day==0)   || ((*sp).day==day))     &&                       // specific day or wildcard found
              (((*sp).month==0) || ((*sp).month==month)) )                        // specific month or wildcard found
          {
            snprintf(MQS, MAX_NOF_CHARS_OF_MSG,                                   // add secret to display with leading/trailing spaces
                     "        %s         ", (*sp).secret);                    
            Serial.printf("new secret '%s'\n", (*sp).secret);            

            lastMode=config.mode;                                                 // save current mode and
            config.mode=MODE_SCROLLING_TEXT;                                      //  switch to text scrolling mode
          }
          sp++;
        }
        int waitSeconds=random(min, max);
        nextMillis=millis()+waitSeconds*1000;

        if(MQS[0]=='\0') 
          Serial.printf("currently there is no new secret\n");
        Serial.printf("next secret lookup in %d seconds\n", waitSeconds);
    }          
}


/*************************************************************************************************************************************/
/*************************************************   S E T U P   F U N C T I O N S   *************************************************/
/*************************************************************************************************************************************/

// Start the LED Matrix Display
void startDisplay()
{
    mx.begin();
    mx.setShiftDataInCallback(cbScrollDataSource);
    mx.setShiftDataOutCallback(cbScrollDataSink);
    mx.setFont(charset);
    mx.control(MD_MAX72XX::INTENSITY, config.brightness);
}

// Start Station mode and tray to connect to given Access Point
bool startSTA(void)
{
    String msg="Connecting to " + String(config.ssid) + ".......";
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    mqAddAt(0, msg);
    Serial.println(msg);
    
    prevMillis = millis();
    while((WiFi.status() != WL_CONNECTED) && (millis() < prevMillis + CONNECT_AP_TIMEOUT))
    {
        delay(20);
        scrollText(1);
    }
    while(millis() < prevMillis + 5000) {
        delay(20);
        scrollText(1);
    }
    mqDelete(0);

    if(millis() < prevMillis + CONNECT_AP_TIMEOUT)
    {
        Serial.print("Connected to ");                 // Tell us what network we're connected to
        Serial.println(WiFi.SSID());  
        Serial.print("IP address:\t");
        Serial.print(WiFi.localIP());
        Serial.println("\n");
        connectedAP = true;
    }
    else
    {
        connectedAP = false;
    }

    clearDisplay();
    return connectedAP;
}

// Start Access Point mode
bool startAP(void)
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidAP, passwordAP);                    // Start the access point
    strcpy(currentMessage, initMessage);
    Serial.println();
    Serial.print("Access Point \"");
    Serial.print(ssidAP);
    Serial.println("\" started\r\n");
    Serial.println("Waiting for connection ...");
    while(WiFi.softAPgetStationNum() < 1)               // Wait for the Wi-Fi to connect
    {
        delay(20);
        scrollText(1);
    }
    Serial.print("Station connected to AP \"Die Zeile\"\n\n");
}

// Start MDNS to enable .local instead of IP address
void startMDNS(void)
{
    if(!MDNS.begin(mdnsName))                           // Start the mDNS responder
    {
        Serial.println("Error setting up MDNS responder!");
    }
    else
    {   
        MDNS.addService("http", "tcp", 80);
        Serial.print("mDNS responder started: http://");
        Serial.print(mdnsName);
        Serial.println(".local");
    }
}

// Start the SPIFFS and list all contents
void startSPIFFS()
{
    SPIFFS.begin();                                   // Start the SPI Flash File System (SPIFFS)
    Serial.println("SPIFFS started. Contents:");
    {
        Dir dir = SPIFFS.openDir("/");
        while(dir.next())                             // List the file system contents
        {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            Serial.printf("\tFS File: %s, size: %d Bytes\r\n", fileName.c_str(), fileSize);
        }
        Serial.printf("\n");
    }
}

// Start the OTA service
void startOTA()
{
    ArduinoOTA.setHostname(OTAName);
    ArduinoOTA.setPassword(OTAPassword);

    ArduinoOTA.onStart([]()
    {
        Serial.println("Start");
    });
    ArduinoOTA.onEnd([]()
    {
        Serial.println("\r\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.printf("Error[%u]: ", error);
        if(error      == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
        else if(error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
        else if(error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if(error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if(error == OTA_END_ERROR)     Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready\r\n");
}

// Start a WebSocket server
void startWebSocket()
{
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);          // If there's an incomming websocket message, go to function 'webSocketEvent'
    Serial.println("WebSocket server started.");
}

// Start a HTTP server with a file read handler and an upload handler
void startServer()
{
    server.on("/set", handleSet);              // set request
    server.on("/edit.html",  HTTP_POST, [](){  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", ""); 
    }, handleFileUpload);                      // Go to 'handleFileUpload'

    server.onNotFound(handleNotFound);         // If someone requests any other file or page, go to function 'handleNotFound'
                                               // and check if the file exists

    server.begin();                            // Start the HTTP server
    Serial.println("HTTP server started.");
}

// Start a NTP request and set time
void startNTP()
{
    if(connectedAP)
    {
        configTime(0, 0, "pool.ntp.org");
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 3);
        tzset();
    }
}



/*************************************************************************************************************************************/
/***********************************************************   S E T U P   ***********************************************************/
/*************************************************************************************************************************************/

void setup() 
{
    mqInit();                    // init message queue
    
    Serial.begin(115200);        // Start the Serial communication to send messages to the computer
    delay(10);
    Serial.println("\r\n");

    eepromReadConfig();          // Read configuration from EEPROM
    
    startDisplay();              // Start the LED Matrix Display

    startSPIFFS();               // Start the SPIFFS and list all contents

    if(startSTA())               // Start WiFi Station mode and connect to AP with credentials from config struct
    {
        startMDNS();             // Start MDNS when connection succeeded
        //mqAddAt(0, String(config.message));
        strcpy(currentMessage, config.message);
    }
    else
    {
        startAP();               // If connection failed, start your own access point
        config.mode  = MODE_SCROLLING_TEXT;
        config.speed = 20;
    }

    startOTA();                  // Start the OTA service
  
    startWebSocket();            // Start a WebSocket server
  
    startServer();               // Start a HTTP server with a file read handler and an upload handler

    startNTP();                  // Start a NTP request and set time

    startMathClock();            // Initialize Math Clock

    randomSeed(analogRead(0));

    readSecretsFromFile();
}



/*************************************************************************************************************************************/
/************************************************************   L O O P   ************************************************************/
/*************************************************************************************************************************************/

void loop() 
{
    MDNS.update();
    webSocket.loop();             // Constantly check for websocket events
    server.handleClient();        // Run the server
    ArduinoOTA.handle();          // Listen for OTA events
    updateTime();                 // Update currentTime variable

    if(Secrets!=NULL)             // if secrets defined
      handleSecret();             //   then look up for a secret 
    
    switch(config.mode)
    {
        case MODE_CLOCK:
            showClock(true);
            break;
        case MODE_MATH_CLOCK:
            showMathClock(false);
            break;
        case MODE_ADDONLY_CLOCK:
            showMathClock(true);
            break;
        case MODE_PROGRESS_TIME:
            showProgressClock();
            break;
        case MODE_SET_THEORY_CLOCK:
            showSetTheoryClock();
            break;
        case MODE_BOLD_CLOCK:
            showBoldClock(true);
            break;
        case MODE_TEXT_CLOCK:
            showTextClock(false);
            break;
        case MODE_SCROLLING_TEXT:
            break;
        default:
            break;
    }

    switch(config.mode)
    {
      case MODE_TEXT_CLOCK:
      case MODE_SCROLLING_TEXT:
            scrollText(config.speed);
            break;
    }
}
