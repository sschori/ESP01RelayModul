# ESP01RelayModul
Software for the 3€ China Espressif ESP8266 ESP-01 Relay Modul.

Inspired by the very good article in Elektor Magazine 
https://www.elektormagazine.de/news/eineinfacher2wlanschalterderfunktioniert

Just take the RelayBoard.ino file and compile and upload it with the Arduino IDE to the ESP-01 Board.
Put the ESP-01 back in the Relay Module and start it up.
The ESP-01 will open it's own WiFi network named "Relay-Modul". Go in this Network and open Browser at http://192.168.1.1
You will land on the StartPage. Go to Settings and enter all Data from your WiFi Network.
It's easier when you set a static IP. Save it and wait 1 minute.
Go back in your Network and open the URL to the IP you entered. http://yourIP
Or new! use mDNS direct http://myrelaycard.local
Enjoy your IoT Relay.

PLEASE don't use the relay for 110V or 220V on power grid! Just for low voltage switching!

SetUp Arduino IDE for Espressif: https://m.heise.de/make/artikel/ESP-Boards-mit-der-Arduino-IDE-programmieren-4130861.html?seite=all

Module from Front View

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/images/modul1.jpg)

Module from Side View

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/images/modul2.jpg)

Start Page when view in Browser

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/images/relay.jpg)

Settings Page to edit parameters

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/images/settings.jpg)

