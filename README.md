![DieZeile](/data/DieZeile.png?raw=true)

An ESP8266 based 64x8 LED matrix display with wlan connectivity and browser based configuration interface. This project is a joint development effort of Werner Gergen, Eik Arnold and Claudio Laloni.

<h2>Featured operation modes</h2>

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

The display elements are connected 1:1 in to out.

| Display |  ESP   |
|---------|--------|
| VCC     | 5V     |
| GND     | GND    |
| DIN     | D7     |
| CS      | D8     |
| CLK     | D5     |

<h3>Housing</h3>
coming soon...

<h3>Building and Flash</h3>
The code can be compiled and up-loaded to the ESP with the Arduino IDE 

1. Install Arduino IDE (https://www.arduino.cc/en/software)
2. Add http://arduino.esp8266.com/stable/package_esp8266com_index.json under 
3. Select ...

to be completed soon...
