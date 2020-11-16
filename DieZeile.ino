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
//        subsequently displayed. Functions for message queue management
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
//    - secrets wildcards and multiple secrets
//        a 0 as day and/or month is interpreted as a wildcard. As a consequence multiple secrets (limitetd to 10)
//        can match a specific day/month. handleSecrets() collects now all matching secrets for a day/month and
//        randomly selects one for display each time the function is called
//    - font clock added
//        display the time with different fonts: small (time & date), bold (time), classic (time)
//    - index.html WebSockets.js
//        new features added to the web interface and websocket handler
//    - percent clock added
//        display the time as percentages of hours of the day and miniute of the hour
//    - texts of secrets are loaded dynamically without storing 
//        this enables large secret files (lots of messages) since the text strings are not loaded into the 
//        memory
//    - timer mode added
//        count down of a user configuragbe time span. After the time is elapsed the display swiches back to 
//        the mode before the time was started. No secrets or other messages are shown during active time 
//        mode
//    - additional 'blink' display modes added
//
//  ToDo:
//    - replacement of deprecated SPIFFS functions (to be replaced by FS functions)
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
#include "WlanCredentials.h"

                                              // debugging defines
//#define DEBUG_MQ                              // message queue
#define DEBUG_SETCOMMANDS
//#define DEBUG_CLOCK                           // clock
#define DEBUG_SECRETS                         // secrets
//#define DEBUG_SECRETS_MESSAGES                // secret messages
//#define DEBUG_TIMER                           // timer
//#define DEBUG_WEBSOCKET                       // websocket

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

#define SCROLL_MIN_SPEED                    5 // Minimum speed for scrolling text
#define SCROLL_MAX_SPEED                   42 // Maximum speed for scrolling text

#define SECRETS_FILENAME                   "/secrets.txt"
/*************************************************************************************************************************************/
/************************************************   G L O B A L   V A R I A B L E S   ************************************************/
/*************************************************************************************************************************************/
String DieZeileVersion="0.9 " + String(__DATE__) + " " + String(__TIME__);

const char *ssidAP       = "Die Zeile";         // The name of the Wi-Fi network that will be created
const char *passwordAP   = "DieZeile";          // The password required to connect to it, leave blank for an open network
const char *OTAName      = "DieZeile";          // A name and a password for the OTA service
const char *OTAPassword  = "hrlbrmpf"; 
const char* mdnsName     = "DieZeile";          // Domain name for the mDNS responder

char currentMessage[MAX_NOF_CHARS_OF_MSG];
char MQ[MAX_NOF_MQMSG][MAX_NOF_CHARS_OF_MSG];   // message queue buffer
char MQD[MAX_NOF_CHARS_OF_MSG];                 // delimiter text shown between messages
char MQS[MAX_NOF_CHARS_OF_MSG];                 // secret message
int  MQcount=0;                                 // number of messages in message queue buffer
int  MQnextAdd=0;                               // next buffer position to be filled with new message
int  MQnextDisplay=0;

#ifdef BUFFER_RECORDING
bool recordDisplayBuffer=false;
uint8_t displayBuffer[64];
#endif

int  lastMode=0;                            // Remember last mode if secret is displayed
bool display_mode_changed  = false;         // Remember if display mode changed
bool connectedAP           = false;         // Remember if connected to an external AP
bool validTime             = false;         // true, when valid time information is available  
unsigned long prevMillis   = millis();      // Initialize current millisecond
struct tm *currentTime;                     // Store current time in this variable

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLOCK_PIN, LOAD_PIN, MAX_NOF_DISPLAYS); // Initialize display HW

ESP8266WebServer server(80);                // Create a web server on port 80
WebSocketsServer webSocket(81);             // Create a websocket server on port 81

File fsUploadFile;                          // A File variable to temporarily store the received file

// operation modes ------------------------
enum { MODE_CLOCK=1,                        // Normal CLock mode
       MODE_MATH_CLOCK=2,                   // Math Clock mode
       MODE_ADDONLY_CLOCK=3,                // Add Only Clock mode (no longer needed)
       MODE_PROGRESS_TIME=4,                // Scrolling Text mode
       MODE_SCROLLING_TEXT=5,               // Scrolling Text mode
       MODE_TEXT_CLOCK=6,                   // Text Clock mode
       MODE_SET_THEORY_CLOCK=7,             // Text Clock mode
       MODE_FONT_CLOCK=8,                   // Clock mode with different fonts (small, bold, classic)
       MODE_PERCENT_CLOCK=9,                // Clock mode with time displayed in percent values
       MODE_TIMER=10                        // timer mode       
       };
const char *operationModes[] = { 
       "MODE_CLOCK",                        // Normal CLock mode
       "MODE_MATH_CLOCK",                   // Math Clock mode
       "MODE_ADDONLY_CLOCK",                // Add Only Clock mode (no longer needed)
       "MODE_PROGRESS_TIME",                // Scrolling Text mode
       "MODE_SCROLLING_TEXT",               // Scrolling Text mode
       "MODE_TEXT_CLOCK",                   // Text Clock mode
       "MODE_SET_THEORY_CLOCK",             // Text Clock mode
       "MODE_FONT_CLOCK",                   // Clock mode with different fonts (small, bold, classic)
       "MODE_PERCENT_CLOCK",                // Clock mode with time displayed in percent values
       "MODE_TIMER"                         // timer mode       
       };

// display modes --------------------------  
enum { DM_NORMAL=0, DM_BLINK=1, DM_GLOW=2, DM_RAMPUP=3, DM_RAMPDOWN=4 };        // display modes
const char *displayModes[] = { "normal", "blink", "glow", "ramp up", "ramp down" };

// clock modes ----------------------------  
enum { CM_MONTH_ASDIGIT=00, CM_MONTH_ASSTRING=1 };                              // display modes
const char *clockModes[] = { "month as digit", "month as string" };             // display mode strings

// timer states ---------------------------  
enum { TS_SET=0, TS_RUNNING=1, TS_PAUSE=2, TS_EXIT=3 } timerState = TS_SET;     // timer states
const char *timerStates[] = { "TS_SET", "TS_RUNNING", "TS_PAUSE", "TS_EXIT" };  // timer state strings
int remainingSeconds=0;                                                         // remaining timer time

// configuration --------------------------  
typedef struct t_config {                   // config struct storing all relevant data in EEPROM
    char MAC[18];                           // 'xx.xx.xx.xx.xx.xx'
    uint8_t brightness;                     // display brightness
    uint8_t mode;                           // operation mode
    uint8_t speed;                          // scrolling speed
    char message[MAX_NOF_CHARS_OF_MSG];     //
    char ssid[MAX_NOF_CHARS_OF_AP];         // SSID
    char password[MAX_NOF_CHARS_OF_AP];     // password
    int secretPeriod;                       // secret period = min distancs between display of secrets 
    int secretWindow;                       // secret window 
    int clockFont;                          // clock fonts (0=small, 1=bold, 2=normal)
    int displayMode;                        // display modes (0=normal, 1=blink, 2=glow, 3=ramp-up, 4=ramp-down
    int timerTime;                          // timer time
    int clockMode;                          // clock mode (0=numeric display of time & date, 1=display month as string) 
    int mathMode;                           // 0=all operations (+-*/)  1=+ only
    int displayPeriod;                      // 
    int displayDuration;                    // duration of display mode (if display mode != normal)
    int timerState;                         // timer state (see enum timer State)
} Config;
Config config;

// secrets --------------------------------  
typedef struct t_Secret {                   // Secret struct to store secret information read from file
  int day;                                  // day
  int month;                                // month
  size_t position;                          // index to file position of secret text
} Secret;
Secret *Secrets=NULL;                       // dynamically loaded secrets (from SPIFFS file secrets.txt)

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

    EEPROM.begin(1024);
    EEPROM.get(0, config);
    EEPROM.end();
    
    eepromMAC=String(config.MAC);

    if(eepromMAC!=WiFiMAC)    // eeprom contains invalid data -> not yet initialized
    {
        Serial.printf("Initilizing default configuration\n");
        sprintf(config.MAC, "%s", WiFiMAC.c_str()); 
        config.brightness = 1;
        config.mode = MODE_CLOCK;
        config.speed = 32;
        strcpy(config.message, "Die Zeile !");
        strcpy(config.ssid, YOURWLANAP);
        strcpy(config.password, YOURPASSWORD);
        config.secretPeriod = 30;                     // minimum seconds before next secret is fetched
        config.secretWindow = 0;                      // time window for display of secret after secretPeriod
        config.clockFont    = 0;                      // 0=small 1=bold 2=classic
        config.displayMode  = DM_NORMAL;              // sub mode for display settings
        config.timerTime    = 0;
        config.clockMode    = CM_MONTH_ASSTRING;      // 0= month as 'xx.xx.' 1=month as string
        config.mathMode     = 0;                      // 0= '+ - * /'         1='+'
        config.displayPeriod = 1000;
        config.displayDuration = 3000;
        config.timerState = TS_SET;
        eepromWriteConfig();
    }
    else
    {
        Serial.printf("Reading configuration from eeprom\n");
        /*
        config.clockFont = 0;                       // 0=small 1=bold 2=classic
        config.displayMode = DM_NORMAL;             // sub mode for display settings
        config.timerTime = 0;
        config.clockMode = 1;                       // 0= month as 'xx.xx.' 1=month as string
        config.mathMode = 0;                        // 0= '+ - * /'         1='+'
        config.displayPeriod = 1000;
        config.displayDuration = 3000;
        config.timerState = TS_SET;
        */
        printConfiguration(config);
    }

    //mqAddAt(0, String(config.message));
}

void eepromWriteConfig(void)
{
    Serial.printf("Writing configuration to eeprom\n");
    printConfiguration(config);
   
    EEPROM.begin(1024);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
}

void printConfiguration(Config config)
{
    Serial.printf("Configuration:\n");
    Serial.printf("MAC:          %s\n",      config.MAC);
    Serial.printf("brightness:   %d\n",      config.brightness);
    Serial.printf("mode:         %d (%s)\n", config.mode, operationModes[config.mode]);
    Serial.printf("speed:        %d\n",      config.speed);
    Serial.printf("secretPeriod: %d sec\n",  config.secretPeriod);
    Serial.printf("secretWindow: %d sec\n",  config.secretWindow);
    Serial.printf("clock font:   %d (%s)\n", config.clockFont, (config.clockFont==2?"classic":(config.clockFont)?"bold":"small"));
    Serial.printf("clock mode:   %d (%s)\n", config.clockMode, clockModes[config.clockMode]);
    Serial.printf("math mode:    %d (%s)\n", config.mathMode, (config.mathMode?"all operations":"add only"));
    Serial.printf("displaymode:  %d (%s)\n", config.displayMode, displayModes[config.displayMode]);
    Serial.printf("timerTime:    %d sec\n",  config.timerTime);
    Serial.printf("timerState:   %d (%s)\n", config.timerState, timerStates[config.timerState]);
    Serial.printf("message:      %s\n",      config.message);
    Serial.printf("ssid:         %s\n",      config.ssid);
    Serial.printf("password:     %s\n\n",    config.password);        
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
        server.streamFile(file, contentType);               // Send it to the client
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
        int argInt=(int)(server.arg(argName.c_str())).toInt();
        
        if     (argName=="B" || argName=="brightness")  setBrightness(argInt);
        else if(argName=="S" || argName=="speed")       setSpeed(argInt);
        else if(argName=="M" || argName=="mode")        setMode(argInt);
        else if(argName=="m" || argName=="dmode")       setDisplayMode(argInt);
        else if(argName=="cf")                          setClockFont(argInt);
        else if(argName=="cm")                          setClockMode(argInt);
        else if(argName=="cM")                          setMathMode(argInt);
        else if(argName=="dm")                          setDisplayMode(argInt);
        else if(argName=="dp")                          setDisplayPeriod(argInt);
        else if(argName=="tt")                          setTimerTime(argInt);
        else if(argName=="ts")                          setTimerState(argInt);
        else if(argName=="T" || argName=="text")        setText(server.arg(argName.c_str()));
        else if(argName=="X" || argName=="save")        setSaveConfig();
        else if(argName=="A" || argName=="mqadd")       mqAdd(server.arg(argName.c_str()));
        else if(argName=="a" || argName=="mqdelimiter") mqDelimiter(server.arg(argName.c_str()));
        else if(argName=="D" || argName=="mqdelete")    mqDelete(argInt);
        else if(argName =="sP")                         setSecretPeriod(argInt);
        else if(argName =="sW")                         setSecretWindow(argInt);
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
                      " f=" + String(config.clockFont) +
                      " sP="+ String(config.secretPeriod) +
                      " sW="+ String(config.secretWindow) +
                      " Message=" + String(config.message) + "\n";
    for(int i=0; i<MAX_NOF_MQMSG; i++)
        settings += "MQ[" + String(i) + "]='" + String(MQ[i]) + "'\n";
    settings += "MQD  ='" + String(MQD) + "'\n";
                     
    server.send(200, "text/plain", settings.c_str());
}

void handleTimer()
{
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

// ------------------------------------------------------------------------------------------------------------------------------------
// When a WebSocket message is received
// Implemented set commands:
//  B brightness of display       T text to scroll                    P set password
//  S speed for scrolling         M display mode (clock, text, ...)   X save configuration
//  I SSID                        m display mode (blinking,...)       sP secret period
//  A add message to queue        cf clock font selection             sW secret window
//  D delete message from queue   cm clock mode                                                     
//  a set/delete delimiter        cM math mode
//  tt timer time                 ts timer state
// ------------------------------------------------------------------------------------------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght)
{ 
    IPAddress ip;
  
    switch(type)
    {
        case WStype_DISCONNECTED:                                    // If the websocket is disconnected
            #ifdef DEBUG_WEBSOCKET
            Serial.printf("[%u] Disconnected!\n", num);
            #endif
            break;
        case WStype_CONNECTED:                                       // If a new websocket connection is established
            ip = webSocket.remoteIP(num);
            #ifdef DEBUG_WEBSOCKET
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            #endif
            if(WiFi.softAPgetStationNum() == 0)                      // If the ESP is connected to an AP
            {
                webSocket.sendTXT(num, "C" + String("SSID: ") + String(WiFi.SSID()) + "<br>IP: " + String(WiFi.localIP().toString()));
                connectedAP = true;
            }
            else                                                     // If a station is connected to the ESP SoftAP
            {
                webSocket.sendTXT(num, "C" + String("SSID: ") + String(ssidAP) + "<br>IP: " + String("192.168.4.1"));
                connectedAP = false;
            }
            //eepromReadConfig();                                      // Read configuration from EEPROM (why?)
            webSocket.sendTXT(num, "b"  + String(config.brightness));
            webSocket.sendTXT(num, "m"  + String(config.mode));
            webSocket.sendTXT(num, "s"  + String(SCROLL_MAX_SPEED - config.speed + SCROLL_MIN_SPEED));
            webSocket.sendTXT(num, "t"  + String(config.message));
            webSocket.sendTXT(num, "i"  + String(config.ssid));
            webSocket.sendTXT(num, "p"  + String(config.password));
            webSocket.sendTXT(num, "cf" + String(config.clockFont));
            webSocket.sendTXT(num, "cm" + String(config.clockMode));
            webSocket.sendTXT(num, "cM" + String(config.mathMode));
            webSocket.sendTXT(num, "SP" + String(config.secretPeriod));
            webSocket.sendTXT(num, "SW" + String(config.secretWindow));
            webSocket.sendTXT(num, "v"  + String(DieZeileVersion));
            clearDisplay();
            strcpy(currentMessage, config.message);                  // Copy config message in case startAP() has overwritten it
            display_mode_changed = true;                             // Set display_mode_changed for clocks to be set at once
            break;
        case WStype_TEXT: {                                          // If new text data is received
            #ifdef DEBUG_WEBSOCKET
            Serial.printf("[%u] get Text: %s\n", num, payload);
            #endif
            switch(payload[0]) {
              case 'B': setBrightness((int)strtol((const char *)&payload[1], NULL, 10));  break;
              case 'S': setSpeed((int)strtol((const char *)&payload[1], NULL, 10));       break;
              case 'T': {
                          String Text=(const char *)&payload[1];
                          setText(Text);
                          webSocket.sendTXT(num, "m" + String(config.mode));
                        }
                        break;
              case 'M': setMode((int)strtol((const char *)&payload[1], NULL, 10));        break;
              case 'm': setDisplayMode((int)strtol((const char *)&payload[1], NULL, 10)); break;
              case 'D': mqDelete((int)strtol((const char *)&payload[1], NULL, 10));       break;
              case 'I': strcpy(config.ssid, (const char *)&payload[1]);                   break;
              case 'P': strcpy(config.password, (const char *)&payload[1]);               break;
              case 'X': setSaveConfig();                                                  break;
              case 'a': mqDelimiter((const char *)&payload[1]);                           break;
              case 's': 
                        if(payload[1] == 'P' )      setSecretPeriod((int)strtol((const char *)&payload[2], NULL, 10));
                        else if(payload[1] == 'W' ) setSecretWindow((int)strtol((const char *)&payload[2], NULL, 10));
                        break;
              case 't': 
                        if(payload[1] == 't' )      setTimerTime((int)strtol((const char *)&payload[2], NULL, 10));   
                        else if(payload[1] == 's' ) setTimerState((int)strtol((const char *)&payload[2], NULL, 10));   
                        else if(payload[1] == 'g' ) {
                          webSocket.sendTXT(num, "Tg" + String(remainingSeconds));
                        }
                        break;
              case 'c': 
                        if(payload[1]=='f')       setClockFont((int)strtol((const char *)&payload[2], NULL, 10));   
                        else if(payload[1]=='m')  setClockMode((int)strtol((const char *)&payload[2], NULL, 10));
                        else if(payload[1]=='M')  setMathMode((int)strtol((const char *)&payload[2], NULL, 10));
                        break;
              case 'd': 
                        if(payload[1]=='m')       setDisplayMode((int)strtol((const char *)&payload[2], NULL, 10));   
                        else if(payload[1]=='p')  setDisplayPeriod((int)strtol((const char *)&payload[2], NULL, 10));
                        break;
              case 'A': {
                          String val=(const char *)&payload[1];
                          int atPos=val.toInt();
                          int index=val.indexOf('=');
                          if(index>0)
                            val.remove(0, index+1);
                          mqAddAt(atPos, val.c_str());
                          // else error (ignored)
                        }
                        break;
           }
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

    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set brightness=%d\n", config.brightness);
    #endif
}

void setSpeed(int newSpeed)
{
    config.speed = SCROLL_MAX_SPEED - (newSpeed - SCROLL_MIN_SPEED);

    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set speed=%d\n", config.speed);
    #endif
}

void setMode(int newMode)
{
    if(config.mode!=newMode) {
      lastMode=config.mode;
      config.mode = newMode;
      if(config.mode == MODE_SCROLLING_TEXT)
      {
        strcpy(currentMessage, "");
        if(MQcount==0) 
          mqAddAt(0, String(config.message));
      }
      display_mode_changed = true;
   
      #ifdef DEBUG_SETCOMMANDS
      Serial.printf("set mode=%d\n", config.mode);
      #endif
    }
    #ifdef DEBUG_SETCOMMANDS
    else Serial.printf("set mode=%d (no change)\n", config.mode);
    #endif
}

void setDisplayMode(int newDisplayMode)
{
    /*
    if(config.displayMode!=DM_NORMAL) {                 // mode change only if normal mode is active
      #ifdef DEBUG_SETCOMMANDS
      Serial.printf("set displaymode=%d (currently not allowed)\n", newDisplayMode);
      #endif
      return;
    }
    */

    switch(newDisplayMode) {
      case DM_NORMAL:
      case DM_BLINK:
      case DM_GLOW:
      case DM_RAMPUP:
      case DM_RAMPDOWN:
          config.displayMode=newDisplayMode;
          break;
      default:
          #ifdef DEBUG_SETCOMMANDS
          Serial.printf("set displaymode=%d (no change)\n", config.displayMode);
          #endif
          config.displayMode=DM_NORMAL;
    }
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set displaymode=%d\n", config.displayMode);
    #endif
}

void setDisplayPeriod(int newPeriod)
{
    config.displayPeriod = newPeriod;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set displayPeriod=%d\n", config.displayPeriod);
    #endif
}

void setSecretPeriod(int newPeriod)
{
    config.secretPeriod = newPeriod;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set secretPeriod=%d\n", config.secretPeriod);
    #endif
}

void setSecretWindow(int newWindow)
{
    config.secretWindow = newWindow;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set secretWindow=%d\n", config.secretWindow);
    #endif
}

void setTimerTime(int time)
{
    config.timerTime=time;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set timerTime=%d\n", config.timerTime);
    #endif
    config.timerState=timerState=TS_SET;
}

void setTimerState(int newState)
{
    config.timerState=newState;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set timerState=%d\n", config.timerState);
    #endif
    switch(newState){
      case TS_RUNNING:  timerState=TS_RUNNING;  break;
      case TS_SET:      timerState=TS_SET;      break;
      case TS_PAUSE:    timerState=TS_PAUSE;    break;
      default:          timerState=TS_EXIT;     
    }
}

void setClockFont(int newFont)
{
    config.clockFont = newFont;
    display_mode_changed = true;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set clockFont=%d\n", config.clockFont);
    #endif
}

void setClockMode(int newMode)
{
    config.clockMode = newMode;
    display_mode_changed = true;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set clockMode=%d\n", config.clockMode);
    #endif
}

void setMathMode(int newMode)
{
    config.mathMode = newMode;
    display_mode_changed = true;
    #ifdef DEBUG_SETCOMMANDS
    Serial.printf("set mathMode=%d\n", config.mathMode);
    #endif
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
    Serial.printf("Restarting 'DieZeile' ...\n");
    delay(2000);
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
// ------------------------------------------------------------------------------------------------------------------------------------
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
// ------------------------------------------------------------------------------------------------------------------------------------
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
// ------------------------------------------------------------------------------------------------------------------------------------
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
      MQnextAdd=(++MQnextAdd)%MAX_NOF_MQMSG;                    // update ring buffer pointer
  }

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// mqAddAt() sets a message at queue position 'index'. 
// ------------------------------------------------------------------------------------------------------------------------------------
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
// ------------------------------------------------------------------------------------------------------------------------------------
void mqDelete(int index)
{
  #ifdef DEBUG_MQ  
  Serial.printf("mqDelete(%d)\n", index);
  #endif
  
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
// ------------------------------------------------------------------------------------------------------------------------------------
void mqDelimiter(String delimiter)
{
  strcpy(MQD, delimiter.c_str());

  #ifdef DEBUG_MQ  
  mqPrint();
  #endif
}

// ------------------------------------------------------------------------------------------------------------------------------------
// printd queue elements for debugging purposes 
// ------------------------------------------------------------------------------------------------------------------------------------
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
  #ifdef BUFFER_RECORDING
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
  #endif
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

    if(currentMessage[0] == 0)
    {
        state = S_IDLE;                     // exit scrolling if string is cleared
    }

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
                        default:
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
    //Serial.printf("clearDisplay()\n");
    strcpy(currentMessage, "");
    //mx.transform(MD_MAX72XX::TSL);            // Cancel scrolling by calling callback one time with empty string
    cbScrollDataSource(0, MD_MAX72XX::TSL);     // rest internal state to S_IDLE without right scroll (prevents display flicker)
    mx.clear();                                 // Clear display
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
        return operators[op2](operators[op1](x, y), z);             // calculate (x op1 y) op2 z
    }
    else
    {
        return operators[op1](x, operators[op2](y, z));             // calculate x op1 (y op2 z)
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
/********************************************   C L O C K   M O D E   F U N C T I O N S   ********************************************/
/*************************************************************************************************************************************/
void showClock(bool monthAsString)
{
    #define MAX_CHARS_CLOCK 13          // 'hh:mm dd.mm.' or 'hh:mm dd.mmm' + terminating \0
    static uint8_t lastTimeString[MAX_CHARS_CLOCK];                        
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    const char *Month[] = { "Jan", "Feb", "Mär", "Apr", "Mai", "Jun", 
                            "Jul", "Aug", "Sep", "Okt", "Nov", "Dez" }; 
    int hour   =currentTime->tm_hour;
    int minutes=currentTime->tm_min;
    int day    =currentTime->tm_mday;
    int month  =currentTime->tm_mon;

    if((currentTime->tm_min != lastMinute) || (display_mode_changed == true))
    {          
        if(display_mode_changed) {
          display_mode_changed = false;
          clearDisplay();

          memset((char *)lastTimeString, 0, MAX_CHARS_CLOCK);
        }
        lastMinute = currentTime->tm_min;

        if(!validTime) {
          hour=minutes=0;
          month=day=0;
        }

        uint8_t timeString[MAX_CHARS_CLOCK];  // "00:00 00.00." or "00:00 DD.MMM"
        timeString[0] = 0x30 + hour / 10;
        timeString[1] = 0x30 + hour - (currentTime->tm_hour / 10) * 10;
        timeString[2] = 147;  // small :
        timeString[3] = 0x30 + minutes / 10;
        timeString[4] = 0x30 + minutes - (minutes / 10) * 10;
        timeString[5] = ' ';
        timeString[6] = 0x30 + day / 10;
        timeString[7] = 0x30 + day - (day / 10) * 10;
        timeString[8] = 138; // small .
        if(monthAsString) {
            timeString[9]  = Month[month][0];
            timeString[10] = Month[month][1];
            timeString[11] = Month[month][2];
        }
        else {
            timeString[9]  = 0x30 + (month + 1) / 10;
            timeString[10] = 0x30 + (month + 1) - ((month + 1) / 10) * 10;
            timeString[11] = 138; // small .
        }

        displayString(0, 12, 63, timeString, lastTimeString);

        //mqAddAt(0, String(timeString)); 
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// displays a clock with selectable fonts: small (with time & date), bold (time only), normal (time only)
// ------------------------------------------------------------------------------------------------------------------------------------
void showFontClock(int font, bool showSeconds)
{
    #define MAX_CHARS_FONTCLOCK 17        // 'hh:mm:ss dd.mm.yy' + terminating \0
    static uint8_t lastSecond = 60;       // initialize for first time with not defined minute value
    static uint8_t lastDigits[17];
    static int lastFont=-1;
    uint8_t digits[MAX_CHARS_FONTCLOCK];  
    int hour   =currentTime->tm_hour;
    int minutes=currentTime->tm_min;
    int seconds=currentTime->tm_sec;
    int day    =currentTime->tm_mday;
    int month  =currentTime->tm_mon+1;
    int year   =(currentTime->tm_year+1900)%100;

    if((seconds!=lastSecond) || (display_mode_changed==true))
    {          
      if(display_mode_changed || lastFont!=font) {
        display_mode_changed = false;
        clearDisplay();

        for(int i=0; i<MAX_CHARS_FONTCLOCK; i++)
          lastDigits[i]=99;       // means not initialized
      }
      lastSecond=seconds;
      lastFont=font;

      if(!validTime) {
        hour=minutes=seconds=0;
        day=month=year=0;
      }
      
      // time
      digits[0] = hour / 10;
      digits[1] = hour - (hour / 10) * 10;
      digits[2] = 10;                               // :
      digits[3] = minutes / 10;
      digits[4] = minutes - (minutes / 10) * 10;
      digits[5] = 10;                               // :
      digits[6] = seconds / 10;
      digits[7] = seconds - (seconds / 10) * 10;
      // date
      digits[8]  = 11;                              // space
      digits[9]  = day / 10;
      digits[10] = day - (day / 10) * 10;
      digits[11]  = 12;                              // .
      digits[12]  = month / 10;
      digits[13] = month - (month / 10) * 10;
      digits[14]  = 12;                              // .
      digits[15]  = year / 10;
      digits[16] = year - (year / 10) * 10;

      switch(font) {
        case 0:  displayString(FONT_SMALL_4x7, 17, 60, digits, lastDigits); break;  // small font   -> display time and date
        case 1:  displayString(FONT_BOLD_5x8,   8, 52, digits, lastDigits); break;  // bold font    -> display time only
        default: displayString(FONT_NORMAL_5x7, 8, 55, digits, lastDigits);         // default font -> display time
      }

      //mqAddAt(0, String(timeString)); 
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  displays the time as percentage values: percentage of hours (24 hours = 100%) : percentage of minutes (60 minutes = 100%)
// ------------------------------------------------------------------------------------------------------------------------------------
void showPercentClock()
{
    #define MAX_CHARS_PERCENTCLOCK 12                       // '00.0% 00.00%' + terminating \0 
    static uint32_t lastMilliSeconds=0;
    static uint8_t lastTimeString[MAX_CHARS_PERCENTCLOCK];
    int hour   =currentTime->tm_hour;
    int minutes=currentTime->tm_min;
    int seconds=currentTime->tm_sec;
    uint32_t actMilliSeconds=millis();

    if((actMilliSeconds>=lastMilliSeconds+3700) || (display_mode_changed==true))           // 3.6 seconds = 0.1% of minutes (use 3700 ms)
    {          
      if(display_mode_changed) {
        display_mode_changed = false;
        clearDisplay();

        for(int i=0; i<11; i++)
          lastTimeString[i]='x'; 
      }
      lastMilliSeconds=actMilliSeconds;

      if(!validTime) {
        hour=minutes=seconds=0;
      }
      
      uint8_t timeString[MAX_CHARS_PERCENTCLOCK];
      float fh=(hour*3600+minutes*60+seconds)/86400.0*100.0;          // hour fraction (24 hours=100%)
      int   ih =(int)fh;                      
      int   ihf=(int)((fh-(float)ih)*10);
      float fm=(minutes*60+seconds)/3600.0*100.0;                     // minutes fraction (60 Minutes = 100%)
      int   im =(int)fm;
      int   imf=(int)((fm-(float)im)*10);

      /*
      String debug  ="fh=" + String(fh) + " ih/ihf=" + String(ih) +"/" + String(ihf) + "  ";
             debug +="fm=" + String(fm) + " im/imf=" + String(im) +"/" + String(imf) + "\n";
      Serial.printf("%s", debug.c_str());
      */
      
      timeString[0]  = 48 + ih / 10;                      // digit    6
      timeString[1]  = 48 + ih - (ih / 10) * 10;          // digit    6
      timeString[2]  = CHAR_DOT_1x7;                      // small .  3
      timeString[3]  = 48 + ihf;                          // digit    6
      timeString[4]  = '%';                               // %        6
      timeString[5]  = CHAR_COLON_5X7;                    // :        7 
      timeString[6]  = 48 + im / 10;                      // digit    6
      timeString[7]  = 48 + im - (im / 10) * 10;          // digit    6
      timeString[8]  = CHAR_DOT_1x7;                      // small .  3
      timeString[9]  = 48 + imf;                          // digit    6
      timeString[10] = '%';                               // %        6

      displayString(0, 11, 60, timeString, lastTimeString);

      //mqAddAt(0, String(timeString)); 
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  displays the time as a bar, i.e. active LED's from left to right display position: 
//      the elapsed time of the day determines the number of active LED's. 
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
// ------------------------------------------------------------------------------------------------------------------------------------
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
                               displaySquare(63, 7,     8, 8, (h12?true:false));    // 12 hours
        for(int i=0; i<2; i++) displaySquare(54, 7-i*5, 6, 3, (i<h4?true:false));   // 4 hours
        for(int i=0; i<3; i++) displaySquare(47, 7-i*3, 3, 2, (i<h?true:false));    // hours
        for(int i=0; i<3; i++) displaySquare(35, 7-i*3, 8, 2, (i<m15?true:false));  // 15 minutes
        for(int i=0; i<2; i++) displaySquare(26, 7-i*5, 3, 3, (i<m5?true:false));   // 5 minutes
        for(int i=0; i<4; i++) displaySquare(22, 7-i*2, 3, 1, (i<m?true:false));    // minutes
                               displaySquare(10, 7,     3, 8, (s30?true:false));    // 30 seconds
        for(int i=0; i<2; i++) displaySquare(6,  7-i*3, 2, 2, (i+3<s5?true:false)); // 5 seconds (2nd col)
        for(int i=0; i<3; i++) displaySquare(3,  7-i*3, 2, 2, (i<s5?true:false));   // 5 seconds
        for(int i=0; i<4; i++) displaySquare(0,  7-i*2, 1, 1, (i<s?true:false));    // seconds

        //Serial.printf("%dx12 + %dx4 + %d : %dx15 + %dx5 + %d\n", h12, h4, h, m15, m5, m);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// display a square of sx *  sy led's at lower left position px,py on the matrtix display
// ------------------------------------------------------------------------------------------------------------------------------------
void displaySquare(uint8_t px, uint8_t py, uint8_t sx, uint8_t sy, bool on)
{
  //Serial.printf("Square %d %d %d %d %s\n", px, py, sx, sy, (on?"on":"off"));    // debug output

  for(uint8_t x=0; x<sx; x++)
    for(uint8_t y=0; y<sy; y++)
      mx.setPoint(py-y, px-x, on); 
}

// ------------------------------------------------------------------------------------------------------------------------------------
// shows the current time as a math formula: hours = a op b op c    minutes = a op b op c
//  op can be one of the 4 basic math operations + - * /. If addonly flag is set only additions are used.
// ------------------------------------------------------------------------------------------------------------------------------------
void showMathClock(bool addonly)
{
    #define MAX_CHARS_MATHCLOCK 12                                     // '0+0+0 0+0+0' + terminating \0
    static uint8_t lastTimeString[MAX_CHARS_MATHCLOCK];
    int hour=currentTime->tm_hour;
    int minutes=currentTime->tm_min;

    if((millis() > prevMillis + MATH_CLOCK_INTERVAL) || (display_mode_changed == true))
    {          
        prevMillis = millis();
        if(display_mode_changed) {
          display_mode_changed = false;
          strncpy((char *)lastTimeString, "x.x.x x.x.x", MAX_CHARS_MATHCLOCK);
          clearDisplay();
        }
  
        uint8_t timeString[MAX_CHARS_MATHCLOCK];
        if(!validTime) {
          hour=minutes=0;
        }

        if(addonly) {
            getFormulaRepresentationAddOnly((char *)(&timeString[0]), hour);
            getFormulaRepresentationAddOnly((char *)(&timeString[6]), minutes);
        }
        else {
            getFormulaRepresentation((char *)(&timeString[0]), hour);
            getFormulaRepresentation((char *)(&timeString[6]), minutes);
        }
        timeString[5]=CHAR_SPACE_4COLS;  // 4 column space

        displayString(0, MAX_CHARS_MATHCLOCK-1, 63, timeString, lastTimeString);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  displays 'chars' characters of the buffer 'digits' on the matrix display. The display starts at position 'column'. 'fontIndex'
//      can be used to determine how the values given in 'digits' are interpreted. For 'fontIndex'==0 the 'digits' are interpreted as 
//      ASCII codes. For 'fontIndex'!=0 the digits are interpreted as decimal values and 'fontIndex' is added as an offset to the 
//      'digit. Each digit in 'digits' is compared to the digit in 'lastDigits'. If the value is the identical the display of the digit 
//      is skipped. On function return 'lastDigits' contains the current string shown on the display. 
// ------------------------------------------------------------------------------------------------------------------------------------
void displayString(int fontIndex, int chars, int column, uint8_t digits[], uint8_t lastDigits[])
{
    #define MAX_DIGIT_COLS 64
    uint8_t buf[MAX_DIGIT_COLS];
    uint8_t *pb;

    //Serial.printf("displayString()\n");
    for(int i=0; i<chars; i++) {
      int cols=mx.getChar(fontIndex+digits[i], MAX_DIGIT_COLS, buf);

      if(digits[i]!=lastDigits[i]) {                // character changed
        //Serial.printf("%02d: %03d != %03d\n", i, digits[i], lastDigits[i]);
        pb=buf;
        for(int j=0; j<cols; j++) {                 // display char columns
          mx.setColumn(column, *pb++);
          column--;
        }
      }
      else {
        //Serial.printf("%02d: %03d == %03d\n", i, digits[i], lastDigits[i]);
        column-=cols;
      }

      lastDigits[i]=digits[i];
      
      column--;

      if(column<0)
        return;
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  display the current time in 5 minute intervals as a natural language sentence on the display in scrolling mode
// ------------------------------------------------------------------------------------------------------------------------------------
void showTextClock(bool longFormat)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    String timeText;
    static String HourName[] = { "Zwölf", "Eins", "Zwei", "Drei", "Vier", "Fünf", "Sechs", 
                                 "Sieben", "Acht", "Neun", "Zehn", "Elf" }; 

    if(validTime) {
        if(((currentTime->tm_min != lastMinute) || (display_mode_changed == true)) && validTime) {          
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
    }
    else  {
      timeText="no time information available";

      mqAddAt(0, timeText); 
      #ifdef DEBUG_CLOCK
      Serial.printf("Time: %sd\n", timeText.c_str());
      #endif
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  displays a text in scrolling mode
// ------------------------------------------------------------------------------------------------------------------------------------
void scrollText(uint8_t speed)
{
    static uint32_t prevTime = 0;

    if(millis() - prevTime >= speed)
    {
        mx.transform(MD_MAX72XX::TSL); // Scroll along - the callback will load all the data
        prevTime = millis();           // Starting point for next time
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  implements an exclusive timer/stop watch mode. The timer can be set to different states
// ------------------------------------------------------------------------------------------------------------------------------------
void modeTimer(int secTimerTime, int font)
{
    #define MAX_CHARS_TIMER 12                                    // 'xx 00:00:00' + terminating \0
    static uint8_t lastTimeString[MAX_CHARS_TIMER];               // buffer to store last printed time string
    static bool timerCountdown=0;                                 // true=countdown mode, false=stop watch mode
    uint8_t timeString[MAX_CHARS_TIMER];                          // buffer for time string to be printed on display
    uint32_t actMillis=millis();                                  // actual milliseconds (since 1.1.1970)
    static uint32_t lastMillis=0;                                 // milliseconds at last update                       
    uint32_t secDeltaTime=(actMillis-lastMillis)/1000;            // seconds since last update

    switch(timerState) {
      case TS_SET: {
                if(display_mode_changed) {
                  display_mode_changed = false;
                  memset(lastTimeString, ' ', MAX_CHARS_TIMER-1);
                  lastTimeString[MAX_CHARS_TIMER-1]=0;
                  clearDisplay();
                }
                remainingSeconds=secTimerTime;                    // set timer time
                if(remainingSeconds==0) timerCountdown=false;     // timer time==0  --> stop watch mode
                else                    timerCountdown=true;      // timer time>0   --> count down mode          

                timerTimeString(remainingSeconds, timeString, MAX_CHARS_TIMER, timerCountdown, font);
                displayString(0, MAX_CHARS_TIMER-1, 63, timeString, lastTimeString);

                #ifdef DEBUG_TIMER
                Serial.printf("Timer TS_SET     %d sec %s\n", remainingSeconds, (timerCountdown?"countdown mode":"stop watch mode"));               
                #endif
                timerState=TS_PAUSE;
            }
            break;

      case TS_RUNNING: {
                if(secDeltaTime>=1) {                                                 // new second -> dispalay needs to be updated
                  if(timerCountdown) remainingSeconds-=secDeltaTime;                  // do count down (timer mode)
                  else               remainingSeconds+=secDeltaTime;                  // do count up (stop watch mode)

                  #ifdef DEBUG_TIMER
                  Serial.printf("Timer TS_RUNNING %d sec %s\n", remainingSeconds, (timerCountdown?"remaining":"elapsed"));
                  #endif
                  lastMillis=actMillis;                                               // save time of last update


                  if(0<=remainingSeconds || remainingSeconds<=354460) {                // do count up/down 354460sec = 99hours+59minutes
                    timerTimeString(remainingSeconds, timeString, MAX_CHARS_TIMER, timerCountdown, font);
                    displayString(0, MAX_CHARS_TIMER-1, 63, timeString, lastTimeString);
                  }
                  else timerState=TS_SET;                                             // count down or count up end reached
                }
            }
            break;

      case TS_PAUSE: {    // do nothing
                if(secDeltaTime>=1) {
                  lastMillis=actMillis;
                  #ifdef DEBUG_TIMER
                  Serial.printf("Timer TS_PAUSE   %d sec %s\n", remainingSeconds, (timerCountdown?"remaining":"elapsed"));
                  #endif
                } 
            }
            break;

      case TS_EXIT: {    // timer mode ended
                  #ifdef DEBUG_TIMER
                  Serial.printf("Timer TS_EXIT\n");
                  #endif
                  config.mode=lastMode;     
                  display_mode_changed = true;
                  clearDisplay();
                  timerState=TS_SET;
            }
            break;      
      
      default:
            timerState=TS_SET;
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  print the string for timer mode display
// ------------------------------------------------------------------------------------------------------------------------------------
void timerTimeString(uint32_t remainingSeconds, uint8_t *timeString, int size, bool countdown, int font)
{
    int fontOffset=0;
    char space=' ';
    
    int rHour= remainingSeconds/3600;
    int rMin =(remainingSeconds-rHour*3600)/60;
    int rSec = remainingSeconds-rHour*3600-rMin*60;

    switch(font) {
      case 0:  { fontOffset=FONT_SMALL_4x7;  space=CHAR_SPACE_22COLS; } break;   // small font   
      case 1:  { fontOffset=FONT_BOLD_5x8;   space=CHAR_SPACE_10COLS; } break;   // bold font    
      default: { fontOffset=FONT_NORMAL_5x7; space=CHAR_SPACE_10COLS; }          // default font 
    }

    timeString[0]=CHAR_HOURGLASS;                       // set first char to 'hour glass' logo
    if(countdown) timeString[1]=CHAR_ARROW_DOWN_5x7;    // set second char to 'arrow down' (countdown)
    else          timeString[1]=CHAR_ARROW_UP_5x7;      // set second char to 'arrow up' (stop watch)
    timeString[2] =space;
    timeString[3] =fontOffset + rHour/10;               // hour first digit (tens)
    timeString[4] =fontOffset + rHour - (rHour/10)*10;  // hour second digit (ones)
    timeString[5] =CHAR_COLON_2X8;                      // small :
    timeString[6] =fontOffset + rMin/10;                // minute first digit (tens)
    timeString[7] =fontOffset + rMin - (rMin/10) * 10;  // minute second digit (ones)
    timeString[8] =CHAR_COLON_2X8;                      // small :
    timeString[9] =fontOffset + rSec/10;                // second first digit (tens)
    timeString[10]=fontOffset + rSec - (rSec/10) * 10;  // second second digit (ones)
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  handler for the different display modes (normal, blink, glow, ramp-up, ramp-down)
// ------------------------------------------------------------------------------------------------------------------------------------
void handleDisplayMode(uint32_t msPeriod, int count, int mode)
{
    static int prevMode = DM_NORMAL;
    static uint32_t prevTime = 0;
    static int actCount=0;
    static bool on=true;
    uint32_t actTime=millis();
    static int intensity = 0;
    static int increment = 1;

    if(mode==DM_NORMAL) {
      if(prevMode!=mode) {
        intensity=config.brightness;
        mx.control(MD_MAX72XX::INTENSITY, config.brightness);   // reset to orginial brightness
        prevMode=mode;
        //Serial.printf("Display mode set to normal\n");
      }
      prevTime=actTime;
      return;
    }
      
    if(msPeriod<500) msPeriod=500;                          // limit period to at least 500ms
    switch(mode) {
      case DM_BLINK:    msPeriod=msPeriod/2;  break;
      case DM_GLOW:     msPeriod=msPeriod/30; break;
      case DM_RAMPUP:
      case DM_RAMPDOWN: msPeriod=msPeriod/15; break;
    }

    if((actTime >= prevTime+msPeriod))
    {
        if(prevMode!=mode) {
          //intensity=config.brightness;
          increment=1;
          actCount=count;
          on=true;
          //Serial.printf("Display mode set to %d with period=%dms, count=%d times\n", mode, msPeriod, count);
        }

        switch(mode) {
          case DM_BLINK: {
            if(on) {                                              // show display with max brightness
              intensity=15;
              on=false;
            }          
            else {                                                // show display with min brightness
              intensity=0;
              on=true;
              actCount--;
            }
            //Serial.printf("Display blink %s (%02d)\n", (on?"on":"off"), actCount);
          }
          break;

          case DM_GLOW: {
            if(intensity>15)  increment=-1;
            if(intensity<1) { increment=1; actCount--; }
            intensity+=increment;
            //Serial.printf("Display glow %d (%02d)\n", intensity, actCount);
          }
          break;

          case DM_RAMPUP: {
              if(intensity<15) intensity++;
              else { intensity=1; actCount--; }
              //Serial.printf("Display rampup %d (%02d)\n", intensity, actCount);            
          }
          break;

          case DM_RAMPDOWN: { 
              if(intensity>1) intensity--;
              else { intensity=15; actCount--; }
              //Serial.printf("Display rampdown %d (%02d)\n", intensity, actCount);                       
          }
          break;
        }

        mx.control(MD_MAX72XX::INTENSITY, intensity);             // set intensity according to mode 
        prevTime = actTime;                                       // set starting time for next time
        prevMode = mode;

        if(actCount==0)
          config.displayMode=DM_NORMAL;
          
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
  int nofLines=0;
  int iLine=0;

  Serial.printf("\nLooking for secrets...\n");
  if(!SPIFFS.exists(SECRETS_FILENAME)) {
    Serial.printf("could not find secrets file %s\n", SECRETS_FILENAME);
    return false;
  }

  Serial.printf("found secrets.txt, reading ...\n");

  secretFile=SPIFFS.open(SECRETS_FILENAME, "r");
  nofLines=0;                                         // determine the numbrer of lines in the file
  while(secretFile.available()) {                     // still chars to read
    char inChar=secretFile.read();    
    if(inChar=='\n')                                  // new line found
      nofLines++;
  }
  secretFile.close();  

  Secrets=(Secret *)malloc(++nofLines*sizeof(Secret));  
  if(Secrets==NULL) {                                 // malloc failed
    Serial.printf("malloc() failed in line %d\n", __LINE__);
    return false;                                     //   return
  }

  secretFile=SPIFFS.open(SECRETS_FILENAME, "r");
  #define MAX_FILE_LINESIZE 500
  char buffer[MAX_FILE_LINESIZE];
  char secret[MAX_FILE_LINESIZE];

  iLine=0;                                                    // scan and parse all lines
  while(secretFile.available() && iLine<nofLines-1) {         // if still chars to read from file 
    Secrets[iLine].position=secretFile.position()+9;          // store file position of secret text
    readToEndOfLine(secretFile, buffer, MAX_FILE_LINESIZE);   // read entire line
    sscanf(buffer, "%d, %d, %[^\n]s\n",                       // 'parse' line 
           &(Secrets[iLine].day), 
           &(Secrets[iLine].month), secret);

    if(iLine>0 && (iLine%100)==0)
      Serial.printf("%04d,%c", iLine, (iLine%1000?' ':'\n'));
    #ifdef DEBUG_SECRETS_MESSAGES
    Serial.printf("%02d: %02d %02d %s\n", iLine+1, Secrets[iLine].day, Secrets[iLine].month, secret);
    #endif
    
    iLine++;
  }
  Secrets[iLine].day=-1;                              // add a terminating line to list of secrets 
  Secrets[iLine].month=-1;
  
  #ifdef DEBUG_SECRETS_MESSAGES
  Serial.printf("%02d: %02d %02d (terminating secret)\n", iLine+1, Secrets[iLine].day, Secrets[iLine].month);
  #endif

  Serial.printf("%04d secrets indexed.\n", nofLines);

  secretFile.close();
  return true;
}

// ------------------------------------------------------------------------------------------------------------------------------------
//  displays randomly secrets 
// ------------------------------------------------------------------------------------------------------------------------------------
void handleSecret(int minDistanceSeconds, int widthTimeframeSeconds)
{
    #define MAX_POOL_ELEMENTS 100
    static unsigned long nextMillis = 0;
    int secretsPool[MAX_POOL_ELEMENTS];
    int poolIndex=0;

    if( MQS[0]!='\0' || Secrets==NULL || widthTimeframeSeconds==0)
      return;
      
    if (millis() > nextMillis)                                                  // next lookup time reached
    {       
      int day=currentTime->tm_mday;
      int month=currentTime->tm_mon+1;
     #define MAX_FILE_LS 500
      char buffer[MAX_FILE_LS];

      int index=0;
      while(Secrets[index].day!=-1)                                             // secret list end not reached
      {
        if( ((Secrets[index].day==0)   || (Secrets[index].day==day))     &&     // specific day or wildcard found
            ((Secrets[index].month==0) || (Secrets[index].month==month)) ) {    // specific month or wildcard found
          secretsPool[poolIndex]=index;                                         // add to secrets pool
          if(poolIndex<MAX_POOL_ELEMENTS) poolIndex++;
        }
        index++;
      }

      if(poolIndex>0) {                                                         // at least one secret found
        File secretFile=SPIFFS.open(SECRETS_FILENAME, "r");
        
        #ifdef DEBUG_SECRETS
        Serial.printf("%d matching secrets\n", poolIndex);    
        for(int i=0; i<poolIndex; i++) 
          if(secretFile.seek(Secrets[secretsPool[i]].position)) {
            readToEndOfLine(secretFile, buffer, MAX_FILE_LS);
            Serial.printf("  %02d: %s\n", i+1, buffer);        
          }
        #endif

        int selected=random(0, poolIndex-1);                                    // random selection from pool
        buffer[0]=0;
        if(secretFile.seek(Secrets[secretsPool[selected]].position))            // seek to selected secret entry in secrets file
          readToEndOfLine(secretFile, buffer, MAX_FILE_LS);
        secretFile.close();

        lastMode=config.mode;                                                   // save current mode (if not BLINK) and
        config.mode=MODE_SCROLLING_TEXT;                                        //  switch to text scrolling mode
      }
      
      int waitSeconds=random(minDistanceSeconds, minDistanceSeconds+widthTimeframeSeconds);
      nextMillis=millis()+waitSeconds*1000;

      #ifdef DEBUG_SECRETS
      if(buffer[0]=='\0') 
        Serial.printf("no matching secret found\n");
      else {
        Serial.printf("new secret: '%s'\n", buffer);
        snprintf(MQS, MAX_NOF_CHARS_OF_MSG,                                     // add secret to display with leading/trailing spaces
                 "        %s    --     %s        ", buffer, buffer);    
      }
      Serial.printf("next secret lookup in %d seconds\n", waitSeconds);
      #endif
    }          
}

int readToEndOfLine(File in, char *buffer, int buf_size)
{
    int len=0;
    bool eolFound=false;

    while(len<buf_size && !eolFound) {    
      char inChar=in.read();                          // read nect char from file
      if(inChar=='\n' || inChar==-1) {    
        eolFound=true;                                // end of line or end of file found
        buffer[len]=0;                        
      }
      else if(inChar!='\r')                           // copy char to buffer (except \CR char)
        buffer[len++]=inChar;
    }

    return len;
}

/*************************************************************************************************************************************/
/*************************************************   S E T U P   F U N C T I O N S   *************************************************/
/*************************************************************************************************************************************/

// ------------------------------------------------------------------------------------------------------------------------------------
// Start the LED Matrix Display
// ------------------------------------------------------------------------------------------------------------------------------------
void startDisplay()
{
    mx.begin();
    mx.setShiftDataInCallback(cbScrollDataSource);
    mx.setShiftDataOutCallback(cbScrollDataSink);
    mx.setFont(charset);
    mx.control(MD_MAX72XX::INTENSITY, config.brightness);
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Start Station mode and try to connect to given Access Point
// ------------------------------------------------------------------------------------------------------------------------------------
bool startSTA(String ssid, String password)
{
    String msg="Connecting to '" + ssid + "' ...";
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
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
        Serial.printf("Connected to '%s' with IP address '%s'\n", 
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str()
                      );                 // Tell us what network we're connected to
        connectedAP = true;
        
        prevMillis=millis();
        mqAddAt(0, String(WiFi.localIP().toString()));
        while(millis() < prevMillis + 8000) {
          delay(20);
          scrollText(1);
        }
        mqDelete(0);
    }
    else
    {
        connectedAP = false;
        Serial.printf("Connection failed\n");
    }

    clearDisplay();
    return connectedAP;
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Start Access Point mode
// ------------------------------------------------------------------------------------------------------------------------------------
void startAP(void)
{
    const char initMessage[] = "Bitte mit \"Die Zeile\" verbinden (Passwort \"DieZeile\") und IP 192.168.4.1 aufrufen.";

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidAP, passwordAP);                    // Start the access point
    Serial.println();
    Serial.print("Access Point \"");
    Serial.print(ssidAP);
    Serial.println("\" started\r\n");
    Serial.println("Waiting for connection ...");

    mqAddAt(0, String(initMessage));
    while(WiFi.softAPgetStationNum() < 1)               // Wait for the Wi-Fi to connect
    {
        delay(20);
        scrollText(1);
    }
    mqDelete(0);
    Serial.print("Station connected to AP \"Die Zeile\"\n\n");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Start MDNS to enable .local instead of IP address
// ------------------------------------------------------------------------------------------------------------------------------------
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

// ------------------------------------------------------------------------------------------------------------------------------------
// Start the SPIFFS and list all contents
// ------------------------------------------------------------------------------------------------------------------------------------
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

// ------------------------------------------------------------------------------------------------------------------------------------
// Start the OTA service
// ------------------------------------------------------------------------------------------------------------------------------------
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

// ------------------------------------------------------------------------------------------------------------------------------------
// Start a WebSocket server
// ------------------------------------------------------------------------------------------------------------------------------------
void startWebSocket()
{
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);          // If there's an incomming websocket message, go to function 'webSocketEvent'
    Serial.println("WebSocket server started.");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Start a HTTP server with a file read handler and an upload handler
// ------------------------------------------------------------------------------------------------------------------------------------
void startServer()
{
    server.on("/set", handleSet);              // If a POST request is sent to the /set address
    server.on("/edit.html",  HTTP_POST, [](){  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", ""); 
    }, handleFileUpload);                      // Go to 'handleFileUpload'

    server.on("/timer.html",  HTTP_POST, [](){ // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", ""); 
    }, handleTimer);                           // Go to 'handleTimer'

    server.onNotFound(handleNotFound);         // If someone requests any other file or page, go to function 'handleNotFound'
                                               // and check if the file exists

    server.begin();                            // Start the HTTP server
    Serial.println("HTTP server started.");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Start a NTP request and set time
// ------------------------------------------------------------------------------------------------------------------------------------
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
    Serial.printf("\n******************************************\n");
    Serial.printf("DieZeile Version %s\n", DieZeileVersion.c_str());
    
    eepromReadConfig();          // Read configuration from EEPROM
    startDisplay();              // Start the LED Matrix Display
    startSPIFFS();             // Start the SPIFFS and list all contents

    if(startSTA(String(config.ssid), String(config.password))) {    // Start WiFi Station mode and connect to AP with credentials from config struct
        startMDNS();                                                // Start MDNS when connection succeeded
        //strcpy(currentMessage, config.message);
    }
    else if(startSTA(String(YOURWLANAP2), String(YOURPASSWORD2))) { // try alternative AP
        startMDNS();                                                // Start MDNS when connection succeeded
        //strcpy(currentMessage, config.message);
    }
    else {                                                          // connection attempts failed
        startAP();                                                  // start your own access point
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

    if(config.mode==MODE_TIMER) {                                     // TIMER mode active?
      modeTimer(config.timerTime, config.clockFont);                  // handle timer mode
      return;                                                         // TIMER MODE is exclusive, do nothing else and return 
    }
      
    if(config.secretWindow!=0)                                        // secrets enabled?
      handleSecret(config.secretPeriod, config.secretWindow);         // look up for a secret to display
    
    switch(config.mode)
    {
      case MODE_CLOCK:
          showClock(config.clockMode);
          break;
      case MODE_MATH_CLOCK:
          showMathClock(config.mathMode);
          break;
      case MODE_PROGRESS_TIME:
          showProgressClock();
          break;
      case MODE_SET_THEORY_CLOCK:
          showSetTheoryClock();
          break;
      case MODE_FONT_CLOCK:
          showFontClock(config.clockFont, true);
          break;
      case MODE_PERCENT_CLOCK:
          showPercentClock();
          break;
      case MODE_TEXT_CLOCK:
          showTextClock(false);
          break;
      default:
          break;
    }

    handleDisplayMode(config.displayPeriod, 5, config.displayMode);   // handle display modes 
    
    switch(config.mode)                                               // do scrolling if required     
    { 
      case MODE_TEXT_CLOCK:
      case MODE_SCROLLING_TEXT:
          scrollText(config.speed);
          break;
    }
}
