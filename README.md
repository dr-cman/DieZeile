![DieZeile](/data/DieZeile.png?raw=true)

An ESP8266 based 64x8 LED matrix display with wlan connectivity and browser based configuration interface. This project is a joint development effort of Werner Gergen, Eik Arnold and Claudio Laloni.

<h2>Operation modes</h2>

'DieZeile' features different operation modes to display the current time/date information, to offer a timer/stop watch function or a scrolling display of user configurable information messages or the current time

<h3>Clock modes</h3>

<h4>Classic</h4>

![](/doc/Classic_digits.jpg?raw=true) month as digits
![](/doc/Classic_string.jpg?raw=true) month as string

<h4>Math (Mathe)</h4>

![](/doc/Mathe_add.jpg?raw=true) add operations only
![](/doc/Mathe_all.jpg?raw=true) all operations (+ - / *)

<h4>Bar (Balken)</h4>

![](/doc/Balken.jpg?raw=true) 

<h4>Set (Mengen)</h4>

![](/doc/Mengen.jpg?raw=true) 

<h4>Font (Font)</h4>

![](/doc/Font_normal.jpg?raw=true) normal font
![](/doc/Font_small.jpg?raw=true) small font
![](/doc/Font_bold.jpg?raw=true) bold font

<h4>Percent (Prozent)</h4>

![](/doc/Percent.jpg?raw=true) 

<h3>Text display modes</h3>

* Text
* Text clock

<h3>Timer/stop watch mode</h3>

![](/doc/Timer.jpg?raw=true) Timer
![](/doc/Stopwatch.jpg?raw=true) Stop watch

<h2>Hardware set-up</h2>

<h3>Required parts</h3>

* 1 x Wemos D1 mini ESP8266 board (e.g. https://www.amazon.de/AZDelivery-D1-Mini-ESP8266-12E-kompatibel/dp/B01N9RXGHY)
* 2 x 32x8 LED display elements (e.g. https://www.amazon.de/gp/product/B079HVW652)
* 5V USB power supply

<h3>Wiring</h3>

The display elements are connected 1:1 (module 1 out to module 2 in)

| Display |  ESP   |
|---------|--------|
| VCC     | 5V     |
| GND     | GND    |
| DIN     | D7     |
| CS      | D8     |
| CLK     | D5     |

<h2>Housing</h2>
coming soon...

<h2>Web Interfaces</h2>

<h3>Configuration Interface</h3>

![](/doc/GUI_DieZeile.jpg?raw=true) 

<h3>Timer/Stop watch interface</h3>

![](/doc/GUI_timer.jpg?raw=true) 

<h2>Build and Flash the System</h2>
The code can be compiled and up-loaded to the ESP with the Arduino IDE 

1. Install Arduino IDE (https://www.arduino.cc/en/software)
2. Add http://arduino.esp8266.com/stable/package_esp8266com_index.json under File -> Preferences -> Settings 'Additional Boards Manager URL's'
3. Under Tools -> Boards -> Boardmanager install the 'esp8266' by ESP8266 Community
4. Under Tools -> Boards select Board 'Wemos D1 R1'
5. Search and install under Tools -> Manage Libraries the library 'MD_MAX722XX' by majicDesigns
6. Get arduinoWebSockets-master.zip from https://github.com/Links2004/arduinoWebSockets 
7. Add arduinoWebSockets-master.zip under Sketch -> Include Library -> Add Zip library
8. Download ESP8266FS-0.5.0.zip from https://github.com/esp8266/arduino-esp8266fs-plugin/releases
9. Copy extracted zip file content into the arduino tools directory (result should look like Arduino/tools/ESP8266FS/tool/esp8266fs.jar)

Now everything is set-up for the build process.

1. Edit the file WlanCredentials.h and enter the name and password of your wlan access point. An optional second AP and password can be specified as well here.
2. Under Tools->Erase Flash select 'All Flash Contents'
3. Start compile and upload (Ctrl-U)
4. After successful compilation and upload set Tools->Erase Flash to 'Sketch + Wifi Settings'
5. Use Tools->ESP8266 Sketch Data Upload to upload all files to the flash file system
6. After reboot of the ESP connect to the GUI via web browser (https://DieZeile.local or https://<ip-address>)
  
Enjoy!
