//////////////////////////////////////////////////////////////////
//  Die Zeile
//  with 8 Matrix 8x8 LED arrays (Display module MAX7219)
//
//  Wifi web server displaying Clock, Math Clock and
//  allowing to scroll a message on the display.
//
//////////////////////////////////////////////////////////////////
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



#define DATA_PIN                           13 // DIN Scroll Matrix
#define LOAD_PIN                           15 // CS  Scroll Matrix
#define CLOCK_PIN                          14 // CLK Scroll Matrix
#define MAX_NOF_DISPLAYS                    8 // Number of used Led matrices 
#define HARDWARE_TYPE     MD_MAX72XX::FC16_HW // Display type FC16

#define CONNECT_AP_TIMEOUT              10000 // Timeout for connecting AP
#define MATH_CLOCK_INTERVAL             30000 // Interval for math clock to display new task

#define CHAR_SPACING                        1 // Number of columns between two characters
#define MAX_NOF_CHARS_OF_MSG              255 // maxlength of 80 on website (i.e. 3 utf-8 chars for €) (maximum length of string = 600)
#define MAX_NOF_CHARS_OF_AP               100 // maxlength of 30 on website (i.e. 3 utf-8 chars for €)

#define MODE_CLOCK                          1 // Normal CLock mode
#define MODE_MATH_CLOCK                     2 // Math Clock mode
#define MODE_ADDONLY_CLOCK                  3 // Add Only Clock mode
#define MODE_PROGRESS_TIME                  4 // Scrolling Text mode
#define MODE_SCROLLING_TEXT                 5 // Scrolling Text mode

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
bool display_mode_changed  = false;      // Remember if display mode changed
bool connectedAP           = false;      // Remember if connected to an external AP
unsigned long prevMillis   = millis();   // Initialize current millisecond
struct tm *currentTime;                  // Store current time in this variable

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLOCK_PIN, LOAD_PIN, MAX_NOF_DISPLAYS); // Initialize display HW

ESP8266WebServer server(80);             // Create a web server on port 80
WebSocketsServer webSocket(81);          // Create a websocket server on port 81

File fsUploadFile;                       // A File variable to temporarily store the received file

struct {                                 // config struct storing all relevant data in EEPROM
    uint8_t brightness;
    uint8_t mode;
    uint8_t speed;
    char message[MAX_NOF_CHARS_OF_MSG];
    char ssid[MAX_NOF_CHARS_OF_AP];
    char password[MAX_NOF_CHARS_OF_AP];
} config;



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
}



/*************************************************************************************************************************************/
/**********************************************************   E E P R O M   **********************************************************/
/*************************************************************************************************************************************/

void eepromReadConfig(void)
{
    EEPROM.begin(512);
    EEPROM.get(0, config);
    EEPROM.end();
}

void eepromWriteConfig(void)
{
    EEPROM.begin(512);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
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
                config.brightness = (int)strtol((const char *)&payload[1], NULL, 10);
                mx.control(MD_MAX72XX::INTENSITY, config.brightness);
            }
            else if(payload[0] == 'S')                               // We get speed for scrolling text
            {
                int new_speed = (int)strtol((const char *)&payload[1], NULL, 10);
                config.speed = SCROLL_MAX_SPEED - (new_speed - SCROLL_MIN_SPEED);
            }
            else if(payload[0] == 'T')                               // We get text to scroll
            {
                clearDisplay();
                strcpy(currentMessage, (const char *)&payload[1]);
                strcpy(config.message, currentMessage); // Copy it into config struct
                config.mode = MODE_SCROLLING_TEXT;
                webSocket.sendTXT(num, "m" + String(config.mode));
            }
            else if(payload[0] == 'M')                               // We get mode
            {
                config.mode = (int)strtol((const char *)&payload[1], NULL, 10);
                clearDisplay();
                if(config.mode == MODE_SCROLLING_TEXT)
                {
                    strcpy(currentMessage, config.message);
                }
                display_mode_changed = true;
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
                eepromWriteConfig();
                delay(200);
                ESP.restart();
            }
            break;
    }
}



/*************************************************************************************************************************************/
/******************************************************   M A T R I X   R O W   ******************************************************/
/*************************************************************************************************************************************/

void cbScrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
{
//  Serial.print("\n cb ");
//  Serial.print(dev);
//  Serial.print(' ');
//  Serial.print(t);
//  Serial.print(' ');
//  Serial.println(col);
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
            p = currentMessage;             // Reset the pointer to start of message
            state = S_NEXT_CHAR;
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
            showLen = (*p != '\0' ? CHAR_SPACING : (MAX_NOF_DISPLAYS * COL_SIZE) / 2); // Set up the inter character spacing
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

void showClock(void)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value

    if((currentTime->tm_min != lastMinute) || (display_mode_changed == true))
    {          
        if(display_mode_changed)              display_mode_changed = false;
        if(currentTime->tm_min != lastMinute) lastMinute = currentTime->tm_min;
        
        char timeString[12] = "00:00 00.00";
        if(connectedAP)
        {
            timeString[0] = 0x30 + currentTime->tm_hour / 10;
            timeString[1] = 0x30 + currentTime->tm_hour - (currentTime->tm_hour / 10) * 10;
            timeString[3] = 0x30 + currentTime->tm_min / 10;
            timeString[4] = 0x30 + currentTime->tm_min - (currentTime->tm_min / 10) * 10;
            timeString[6] = 0x30 + currentTime->tm_mday / 10;
            timeString[7] = 0x30 + currentTime->tm_mday - (currentTime->tm_mday / 10) * 10;
            timeString[9]  = 0x30 + (currentTime->tm_mon + 1) / 10;
            timeString[10] = 0x30 + (currentTime->tm_mon + 1) - ((currentTime->tm_mon + 1) / 10) * 10;
        }
        for(int i = 0; i < 5; i++)
        {
            mx.setChar(63 - (i * 6), timeString[i]);
        }
        mx.setChar(28, timeString[6]);
        mx.setChar(22, timeString[7]);
        mx.setChar(16, 138);                // use small point and display in comressed form to support two dots
        mx.setChar(13, timeString[9]);
        mx.setChar(7,  timeString[10]);
        mx.setChar(1,  138);                // use small point and display in comressed form to support two dots
    }
}

void showProgressClock(void)
{
    static uint8_t lastSecond = 60;     // initialize for first time with not defined minute value
    
    if((currentTime->tm_sec != lastSecond) || (display_mode_changed == true))
    {          
        int dots = (currentTime->tm_hour * 60 + currentTime->tm_min) * (8 * 8 * 8) / (24 * 60);
        if(display_mode_changed)              display_mode_changed = false;
        if(currentTime->tm_sec != lastSecond) lastSecond = currentTime->tm_sec;
        
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


void showMathClock(bool addonly)
{
    if((millis() > prevMillis + MATH_CLOCK_INTERVAL) || (display_mode_changed == true))
    {          
        prevMillis = millis();
        if(display_mode_changed) display_mode_changed = false;
  
        char timeString[12] = "0+0+0 0+0+0";
        if(connectedAP)
        {
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

void scrollText(uint8_t speed)
{
    static uint32_t prevTime = 0;

    if(millis() - prevTime >= speed)
    {
        mx.transform(MD_MAX72XX::TSL); // Scroll along - the callback will load all the data
        prevTime = millis();           // Starting point for next time
    }
}

bool handleSecret(void)
{
    static uint8_t lastMinute = 60;     // initialize for first time with not defined minute value
    static bool inSecret = false;

    if (inSecret)
    {
        scrollText(config.speed);
    }

    if((currentTime->tm_min != lastMinute))
    {   
        if(currentTime->tm_min != lastMinute) lastMinute = currentTime->tm_min;
        if (inSecret) {
            clearDisplay();
            strcpy(currentMessage, config.message);
            inSecret = false;          
        } else {
          long rand_number = random(SECRET_SHOW_PROBABILITY);
          if (rand_number == 0) {
              inSecret = true;
              if (currentTime->tm_mday == 1 && currentTime->tm_mon == 0) {
                  strcpy(currentMessage, "Alles Gute im neuen Jahr!");
              } else if (currentTime->tm_mday == 21 && currentTime->tm_mon == 0) {
                  strcpy(currentMessage, "Weltknuddeltag");
              } else if (currentTime->tm_mday == 11 && currentTime->tm_mon == 1) {
                  strcpy(currentMessage, "Europäischer Tag des Notrufs 112");
              } else if (currentTime->tm_mday == 12 && currentTime->tm_mon == 1) {
                  strcpy(currentMessage, "Tag der Umarmung");
              } else if (currentTime->tm_mday == 15 && currentTime->tm_mon == 1) {
                  strcpy(currentMessage, "Tag des Regenwurms");
              } else if (currentTime->tm_mday == 21 && currentTime->tm_mon == 1) {
                  strcpy(currentMessage, "Internationaler Tag der Muttersprache");
              } else if (currentTime->tm_mday == 29 && currentTime->tm_mon == 1) {
                  strcpy(currentMessage, "Da schaltet man gar nicht so schnell, und schon ist wieder ein Jahr verstrichen.");
              } else if (currentTime->tm_mday == 7 && currentTime->tm_mon == 2) {
                  strcpy(currentMessage, "Tag der gesunden Ernährung");
              } else if (currentTime->tm_mday == 14 && currentTime->tm_mon == 2) {
                  strcpy(currentMessage, "Wie wäre es mit einem schönen Kuchen zur Feier des Tages?");
              } else if (currentTime->tm_mday == 16 && currentTime->tm_mon == 2) {
                  strcpy(currentMessage, "Hoffentlich gibt es heute einen guten Schlaf zum Weltschlaftag!");                  
              } else if (currentTime->tm_mday == 21 && currentTime->tm_mon == 2) {
                  strcpy(currentMessage, "Welttag der Poesie");
              } else if (currentTime->tm_mday == 25 && currentTime->tm_mon == 2) {
                  strcpy(currentMessage, "Tag der Tolkien-Lektüre");
              } else if (currentTime->tm_mday == 1 && currentTime->tm_mon == 3) {
                  strcpy(currentMessage, "Bitte akzeptieren Sie die aktualisierten Nutzungsbedingungen der Zeile - Anleitung: https://www.youtube.com/watch?v=oavMtUWDBTM");
              } else if (currentTime->tm_mday == 15 && currentTime->tm_mon == 4) {
                  strcpy(currentMessage, "Internationaler Tag der Familie");
              } else if (currentTime->tm_mday == 22 && currentTime->tm_mon == 4) {
                  strcpy(currentMessage, "Bist du auch dafür, dass die Zeile die Weltherrschaft übernimmt?");
              } else if (currentTime->tm_mday == 2 && currentTime->tm_mon == 6) {
                  strcpy(currentMessage, "Tag der Franken");
              } else if (currentTime->tm_mday == 20 && currentTime->tm_mon == 6) {
                  strcpy(currentMessage, "Internationaler Schachtag");
              } else if (currentTime->tm_mday == 24 && currentTime->tm_mon == 6) {
                  strcpy(currentMessage, "Tag der Freude");
              } else if (currentTime->tm_mday == 26 && currentTime->tm_mon == 6) {
                  strcpy(currentMessage, "Esperanto-Tag");
              } else if (currentTime->tm_mday == 13 && currentTime->tm_mon == 8) {
                  strcpy(currentMessage, "Tag des Programmierers");
              } else if (currentTime->tm_mday == 10 && currentTime->tm_mon == 9) {
                  strcpy(currentMessage, "Welthundetag");
              } else if (currentTime->tm_mday == 13 && currentTime->tm_mon == 9) {
                  strcpy(currentMessage, "Zeit einen Weltleberkästag einzuberufen! ;)");
              } else if (currentTime->tm_mday == 15 && currentTime->tm_mon == 9) {
                  strcpy(currentMessage, "Internationaler Händewaschtag");
              } else if (currentTime->tm_mday == 19 && currentTime->tm_mon == 9) {
                  strcpy(currentMessage, "Heute zum Welttoilettentag schon auf dem WC gewesen?");
              } else if (currentTime->tm_mday == 21 && currentTime->tm_mon == 3) {
                  strcpy(currentMessage, "Ja Eik, schon wieder ein Jahr älter!!!");
              } else if (currentTime->tm_mday == 11 && currentTime->tm_mon == 10) {
                  strcpy(currentMessage, "Willkommen in der fünften Jahreszeit!");
              } else if (currentTime->tm_mday == 19 && currentTime->tm_mon == 10) {
                  strcpy(currentMessage, "Männertag");
              } else if (currentTime->tm_mday == 24 && currentTime->tm_mon == 11) {
                  strcpy(currentMessage, "Ho ho ho, frohe Weihnachten!");
              } else if (currentTime->tm_mday == 25 && currentTime->tm_mon == 11) {
                  strcpy(currentMessage, "In weniger als 400 Tagen steht wieder Weihnachten vor der Tür!");                  
              } else if (currentTime->tm_mday == 6) {
                  strcpy(currentMessage, "Vielleicht kein Sechser im Lotto, aber immerhin im Datum!");                  
              } else if (currentTime->tm_mday == 7) {
                  strcpy(currentMessage, "Heute schon Kopfrechnen geübt?");
              } else if (currentTime->tm_mday == 20) {
                  strcpy(currentMessage, "Wusstest du, dass die Zeile die Zukunft vorhersagen kann? So weiß sie z. B. schon jetzt welches Datum morgen ist.");
              } else if (currentTime->tm_mday == 21) {
                  strcpy(currentMessage, "Auch die halbe Antwort ist häufig besser als nichts");
              } else if (currentTime->tm_mday == 24) {
                  strcpy(currentMessage, "Heute in weniger als 42 Monaten ist wieder Weihnachten!");
              } else {
                  inSecret = false;
              }
          }          
        }
    }
    return inSecret;
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
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    strcpy(currentMessage, "Verbindungsaufbau zu \"");
    strcat(currentMessage, config.ssid);
    strcat(currentMessage, "\" ...   ");
    Serial.println(String(currentMessage));
    
    prevMillis = millis();
    while((WiFi.status() != WL_CONNECTED) && (millis() < prevMillis + CONNECT_AP_TIMEOUT))
    {
        delay(20);
        scrollText(1);
    }

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
    Serial.begin(115200);        // Start the Serial communication to send messages to the computer
    delay(10);
    Serial.println("\r\n");

    eepromReadConfig();          // Read configuration from EEPROM
    
    Serial.printf("Configuration:\n");
    Serial.printf("brightness: %d\n", config.brightness);
    Serial.printf("mode:       %d\n", config.mode);
    Serial.printf("speed:      %d\n", config.speed);
    Serial.printf("message:    %s\n", config.message);
    Serial.printf("ssid:       %s\n", config.ssid);
    Serial.printf("password:   %s\n\n", config.password);

// Use this with new HW to initialize data area
/*
    config.brightness = 1;
    config.mode = 2;
    config.speed = 32;
    strcpy(config.message, "Die Zeile !");
    strcpy(config.ssid, "Kellerassel");
    strcpy(config.password, "dimWePeA2");
    eepromWriteConfig();
*/
    
    startDisplay();              // Start the LED Matrix Display

    startSPIFFS();               // Start the SPIFFS and list all contents

    if(startSTA())               // Start WiFi Station mode and connect to AP with credentials from config struct
    {
        startMDNS();             // Start MDNS when connection succeeded
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
}



/*************************************************************************************************************************************/
/************************************************************   L O O P   ************************************************************/
/*************************************************************************************************************************************/

void loop() 
{
    MDNS.update();
    webSocket.loop();            // Constantly check for websocket events
    server.handleClient();       // Run the server
    ArduinoOTA.handle();         // Listen for OTA events
    updateTime();                // Update currentTime variable

    if (handleSecret()) return; // Overwrite action when secret should be displayed
    
    switch(config.mode)
    {
        case MODE_CLOCK:
            showClock();
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
        case MODE_SCROLLING_TEXT:
            scrollText(config.speed);
            break;
        default:
            break;
    }
}
