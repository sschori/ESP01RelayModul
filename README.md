# ESP01RelayModul
Software for the 3€ China Espressif ESP8266 ESP-01 Relay Modul.

Just take the RelayBoard.ino file and compile and upload it with the Arduino IDE to the ESP-01 Board.
Put the ESP-01 back in the Relay Module ans start it up.
The ESP-01 will open a WiFi network. Go in the Network and open http://192.168.1.1
You will land on the StartPage. Go to Settings and enter all Data from your WiFi Network.
It's easier when you set a static IP. Save it and wait 2 minutes.
Go back in your Network and open the URL to the IP you entered. http://yourIP
Enjoy your IoT Relay.

PLEASE don't use the relay for 110V or 220V on power grid! Just for low voltage switching!

SetUp Arduino IDE for Espressif: https://m.heise.de/make/artikel/ESP-Boards-mit-der-Arduino-IDE-programmieren-4130861.html?seite=all

Module from Front View

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/modul1.jpg)

Module from Side View

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/modul2.jpg)

Start Page when view in Browser

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/relay.jpg)

Settings Page to edit parameters

![alt text](https://raw.githubusercontent.com/sschori/ESP01RelayModul/master/settings.jpg)

