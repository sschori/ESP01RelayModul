#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <EEPROM.h>

//Relay
#define RELAY_PORT 0
bool relay = false;

//WiFi
#define MAX_CLIENTS 10
WiFiServer server(80); 
int wifiScanCount = 0;
int32_t ssidVolume[50];
WiFiClient *clients[MAX_CLIENTS] = { NULL };
String *clientsCurrentLine[MAX_CLIENTS] = { NULL };
String *clientsHTTPRequest[MAX_CLIENTS] = { NULL };

//HTTP Protocol
int httpMethod = 0;

//RS232
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

int ssid_adr = 0;  // len = 40  + 5 jeweils Abstand geht gut zum Rechnen
int password_adr = 45; // len = 30
int ip_adr = 80; // len = 15
int subnet_adr = 100; // len = 15
int defaultgw_adr = 120; // len = 15
int dns_adr = 140; // len = 15
int ipType_adr = 160; //len = 1
int powerOn_adr = 165; //len = 1
//next at 170

String ssid = "";
String password = "";
String ipType = "";
String ip = "";
String subnet = "";
String defaultgw = "";
String dns = "";
String powerOn = "";

//EEPROM Struct
  struct 
  { 
    uint val = 0;
    char str[80] = "";
  } data;

void setup()
{
  EEPROM.begin(256);

  inputString.reserve(200);
  ssid.reserve(40);
  password.reserve(30);
  ipType.reserve(1);
  ip.reserve(15);
  subnet.reserve(15);
  defaultgw.reserve(15);
  dns.reserve(15);
  powerOn.reserve(1);  
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 

  pinMode(RELAY_PORT, OUTPUT);
  digitalWrite(RELAY_PORT, HIGH); 
 
  Serial.begin(115200);

  readValuesFromEeprom();

  if (powerOn == "1")
  {
    digitalWrite(RELAY_PORT, LOW); 
    relay = true;
  }

  wifiScanCount = scanWiFi();

  if (!startWiFiClient())
  {
    startWifiAccessPoint();
  }

  server.begin();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    startWiFiClient();
  }
    
  WiFiClient client = server.available();

  checkRS232();

  //Client zwischenspeichern
  if (client) 
  {
    for (int i=0 ; i<MAX_CLIENTS ; ++i) 
    {
        if (NULL == clients[i]) 
        {
          clients[i] = new WiFiClient(client);
          clientsCurrentLine[i] = new String();
          break;
        }
     }
  }

  //Clients durcharbeiten
  for (int i=0 ; i<MAX_CLIENTS ; ++i) 
  {
      if (clients[i] != NULL && clients[i]->connected())
      {
        if (clients[i]->available()) 
        {
          char c = clients[i]->read();

          if (c != '\r' && c != '\n') //Zeile lesen
          {  
            *clientsCurrentLine[i] += c;
            digitalWrite(LED_BUILTIN, LOW); 
          }
          if (c == '\n') //Zeile abgeschlossen
          {
            //Serial.println(*clientsCurrentLine[i]);  //debug out of http protocol lines
            if (clientsCurrentLine[i]->startsWith("GET "))
            {
              clientsHTTPRequest[i] = clientsCurrentLine[i];
            }
            answerRequest(*clientsCurrentLine[i], *clients[i], *clientsHTTPRequest[i]);
            clientsCurrentLine[i] = new String();
          }
        }
      }
      else if (clients[i] != NULL)
      {
        clients[i]->flush();
        clients[i]->stop();
        delete clients[i];
        clients[i] = NULL;
        delete clientsCurrentLine[i];
        clientsCurrentLine[i] = NULL;
        delete clientsHTTPRequest[i];
        clientsHTTPRequest[i] = NULL;
      }
  }
}

void answerRequest(String currentLine, WiFiClient client, String httpRequest)
{ 
  if (currentLine.length() == 0) //Ende des HTTP Request
  {
      if (httpRequest.startsWith("GET / ")) // Startseite
      {
        sendStartPage(client);
        httpMethod = 1;
      } 
      else if (httpRequest.startsWith("GET /ON ")) // Startseite
      {
        digitalWrite(RELAY_PORT, LOW);
        relay = true;
        sendStartPage(client);
        httpMethod = 1;
      } 
      else if (httpRequest.startsWith("GET /OFF ")) // Startseite
      {
        digitalWrite(RELAY_PORT, HIGH);
        relay = false;
        sendStartPage(client);
        httpMethod = 1;
      }     
      else if (httpRequest.startsWith("GET /settings ")) // Startseite
      {
        wifiScanCount = scanWiFi();
        sendSettingsPage(client);
        httpMethod = 1;
      }   
      else if (httpRequest.startsWith("GET /save?"))  //Nach Save Request
      {
        saveSettings(httpRequest);
        sendSavedPage(client);
        httpMethod = 5;
      } 
  
      if (httpMethod == 1 || httpMethod == 5)
      {
        client.flush();
        client.stop(); 
        if (httpMethod == 5)
        {
          delay(1000);
          ESP.restart();
        }
        httpMethod = 0;
        digitalWrite(LED_BUILTIN, HIGH); 
      }
  }
}

void saveSettings(String inputData)
{
  bool foundStart = false;
  String token = "";

  ssid = "";
  password = "";
  ip = "";
  subnet = "";
  defaultgw = "";
  dns = "";
  ipType = "";
  powerOn = "";
  
  for(int i = 0 ; i < inputData.length() ; i++)
  {
    char c = inputData.charAt(i);
    token += c;

    if (foundStart == false && c == '?')
    {
      foundStart = true;
      token="";
    }
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("ssid="))
    {
      ssid=token.substring(5, token.length()-1);;
      ssid = urldecode(ssid);
      token="";
    } 
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("password="))
    {
      password=token.substring(9, token.length()-1);;
      password = urldecode(password);
      token="";
    }  
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("ipType="))
    {
      ipType=token.substring(7, token.length()-1);;
      ipType = urldecode(ipType);
      token="";
    } 
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("ip="))
    {
      ip=token.substring(3, token.length()-1);;
      ip = urldecode(ip);
      token="";
    }    
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("subnet="))
    {
      subnet=token.substring(7, token.length()-1);;
      subnet = urldecode(subnet);
      token="";
    }     
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("defaultgw="))
    {
      defaultgw=token.substring(10, token.length()-1);;
      defaultgw = urldecode(defaultgw);
      token="";
    }     
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("dns="))
    {
      dns=token.substring(4, token.length()-1);;
      dns = urldecode(dns);
      token="";
    } 
    else if (foundStart == true && (c == '&' || c == ' ') && token.startsWith("powerOn="))
    {
      powerOn=token.substring(8, token.length()-1);;
      powerOn = urldecode(powerOn);
      token="";
    }      
  } 
  saveEEPROM(ssid, ssid_adr, 40);
  saveEEPROM(password, password_adr, 30);
  saveEEPROM(ip, ip_adr, 15);
  saveEEPROM(subnet, subnet_adr, 15);
  saveEEPROM(defaultgw, defaultgw_adr, 15);
  saveEEPROM(dns, dns_adr, 15);
  saveEEPROM(ipType, ipType_adr, 1);
  saveEEPROM(powerOn, powerOn_adr, 1);
  EEPROM.commit(); 

  readValuesFromEeprom();
}

void readValuesFromEeprom()
{
  ssid = readEEPROM(ssid_adr, 40);
  password = readEEPROM(password_adr, 30);
  ip = readEEPROM(ip_adr, 15);
  subnet = readEEPROM(subnet_adr, 15);
  defaultgw = readEEPROM(defaultgw_adr, 15);
  dns = readEEPROM(dns_adr, 15);
  ipType = readEEPROM(ipType_adr, 1);
  powerOn = readEEPROM(powerOn_adr, 1);
}

void saveEEPROM(String in, int adr, int len)
{
  clearDataStructure();
  in.toCharArray(data.str, in.length()+1);
  data.val = len;
  EEPROM.put(adr,data);  
}

String readEEPROM(int adr, int len)
{
  clearDataStructure();
  data.val = len;
  EEPROM.get(adr,data);
  for(int i=0; i < len; i++)
  {
    if(data.str[i] != 255 && data.str[i] != 63)
    {
      return data.str;
    }
  }
  return "";
}

void clearDataStructure()
{
  data.val = 0; 
  strncpy(data.str,"",80);
}

void checkRS232()
{
  ESPserialEvent();
  if (stringComplete) 
  {
    if (inputString.startsWith("setON"))
    {
      digitalWrite(RELAY_PORT, LOW);
      relay = true;
      Serial.println("successful set ON");
    }
    else if (inputString.startsWith("setOFF"))
    {
      digitalWrite(RELAY_PORT, HIGH);
      relay = false;
      Serial.println("successful set OFF");
    }

    stringComplete = false;
    inputString = "";
  }
}

void ESPserialEvent() 
{
  while (Serial.available()) 
  {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') 
    {
      stringComplete = true;
    }
  }
}

int scanWiFi()
{
  int networksFound = WiFi.scanNetworks(false, true);
  for (int i = 0; i < networksFound && i < 50; i++)
  {
    ssidVolume[i] = WiFi.RSSI(i);
  }
  return networksFound;
}

void startWifiAccessPoint()
{
  Serial.println();
  Serial.println("Startup as AccessPoint with Name: Relay-Modul");
    
  IPAddress local_IP(192,168,1,1);
  IPAddress gateway(192,168,1,1);
  IPAddress subnet(255,255,255,0);
  
  WiFi.softAP("Relay-Modul");
  WiFi.softAPConfig(local_IP, gateway, subnet);
}

bool startWiFiClient() 
{
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
 
  WiFi.mode(WIFI_STA);

  if (ipType.startsWith("1"))
  {
    IPAddress ipAddress;
    ipAddress.fromString(ip);
    IPAddress gatewayAddress;
    gatewayAddress.fromString(defaultgw);
    IPAddress dnsAddress;
    dnsAddress.fromString(dns);
    IPAddress subnetAddress;
    subnetAddress.fromString(subnet);
  
	  WiFi.config(ipAddress, gatewayAddress, dnsAddress, subnetAddress); 
  }
 
  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 20) 
  {
      delay(500);
      Serial.print(".");
      count ++;
  }

  if (count >= 20)
  {
    return false;
  }

  Serial.println("");
  Serial.print("WiFi connected to: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  return true;
}

String urldecode(String str)
{
    
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++)
    {
      c=str.charAt(i);
      if (c == '+')
      {
        encodedString+=' ';  
      }
      else if (c == '%') 
      {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } 
      else
      {
        encodedString+=c;  
      }
      
      yield();
    }
   return encodedString;
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <='9')
    {
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f')
    {
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F')
    {
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

void sendSavedPage(WiFiClient client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.println("<HTML><HEAD>");
  client.println("<title>NTP Client</title>");
  sendFavicon(client);
  if (ipType.startsWith("1"))
  {
    client.print("<meta http-equiv=\"refresh\" content=\"10; URL=http://");
    client.print(ip);
    client.println("/\">");
  }
  client.println("</HEAD><BODY>");
  client.println("<h2>SETTINGS saved</h2>");
  if (ipType.startsWith("1"))
  {
    client.println("Please wait, in 10 seconds you will be redirected to the Start Page...");
    client.print("<br><br><a href=\"http://");
    client.print(ip);
    client.println("/\">Start Page</a>");
  }
  else
  {
    client.println("Please enter the IP which has set by DHCP from your Router");
  }

  sendStyle(client);
  client.println("</BODY></HTML>");
  client.println();
  client.flush();
}

void sendFavicon(WiFiClient client)
{
  client.print("<link href=\"data:image/x-icon;base64,AAABAAEAMDAAAAEACACoDgAAFgAAACgAAAAwAAAAYAAAAAEACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAQEABAQEAAUFBQAGBgYABwcHAAgICAAKCgoACwsLAAwMDAANDQ0ADg4OABISEgATExMAFhYWABcXFwAZGRkAHBwcAB0dHgAeHh4AICAgACEhIQAiIiIAIyMjACMjJAAkJCQAJSUlACkpKQAqKioAKysrACwsLAAuLi4ALy8vADAwMAAxMTEAMjIyADQ0NAA2NjYAODg4AD4+PgBGRkYASEhJAElJSQBMTEwAT09PAFBQUABRUVEAVFRUAFZWVgBZWVkAXV1dAGBgYABmZmYAZ2dnAGhoaABpaWkAbGxsAG9vbwBxcXEAdXV1AHp6egB/f38Ag4ODAISEhACJiYkAjo6OAJKSkgCYmJgAmpqaAJubmwCdnZ0AoKCgAKKiogCjo6MApKSkAKenpwCpqakAra2tAK+vrwCwsLAAsrKyALS0tAC8vLwAvb29AMDAwADCwsIAxMTEAMXFxQDHx8YAx8fHAMjIyADJyckAysrKAMvLywDNzc0Azs7OAM/PzwDQ0NAA0tLSANPT0wDU1NQA1tbWANfX1wDY2NgA2dnZANra2gDb29wA3NzcAN7e3gDf398A4ODhAOHh4QDi4uIA4+PjAObm5gDn5+cA6OjoAOvr6wDs7OwA7+/vAPDw8ADx8fEA8vLyAPPz8wD09PQA9vb2APf39wD5+fkA+vr6APv7+wD8/PwA/f39AP7+/gD///8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIOEhISEg4SEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEg4RjKiZPhIOEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhFAGMDUDPISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEe3h7d3t2e3h7cCFOhIRmAmCEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISECQkJCAkICQgKBhJ1g4OEIEaEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEXVxZXVxeWl5YUxVmhIR8B1iEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhD0dSUscNYSEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhINCFBhDgISDhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIOEhISEg4SEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEg4OEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIOEhFRVhISDhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEg4RrEQcGHXOEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhHcPPn13NyqEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISDhE0WhISEcxQrLy8vLy8vLy8vhISEhISEhISEhISEhISEhISEhISEhISDhISEhHpzWywQhISEcRMsLy8vLy8vLy8vhISEhISEhISEhISEhISEhISEhISDhISEhGFFNiMXIjgHQH51NDGEhISEhISEhISEhISEhISEhISEhISEhISEhISDhISEf0suCwEeOlZyeoRiDQgEJ3iEhISEhISEhISEhISEhISEhISEhISEhISEhHtzXDsfAAwtSHqEhISEhIOEhFJkhISDhISEhISEhISEhISEhISEhISEhISDhGNGNyQVIDNEX4OEhISDhISEhISDhISEg4SEhISEhISEhISEhISEhISEhISEhIOEUQEgOVRxeYOEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIJKGg5BhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhD8bSU0lK4OEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEbmltaG5nbmpvWxllhISBCFKEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEBgYGBgYGBgYGBRJ1g4OEIEeEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEdHR0dHR0dHR0bCBPhIRbBWuEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhEwGMDIASoSEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEg4RXKClbhIOEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIOEhISEg4SEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\" rel=\"icon\" type=\"image/x-icon\" />");
}

void sendStartPage(WiFiClient client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.println("<HTML><HEAD>");
  client.println("<title>WiFi Relay</title>");
  sendFavicon(client);
  client.println("</HEAD><BODY>");
  sendStyle(client);
  client.println("<h1>WiFi Relay</h1>");  
  client.println("<hr>");
  client.println("<h2>WiFi Relay:</h2>"); 
  client.print("<input type=\"submit\" value=\"ON\" style=\"width:100px;height:55px\" onClick=\"location.href='/ON'\"");
  if (relay)
  {
    client.print(" disabled");
  }
  client.println(">");
  client.print("&nbsp;");
  client.print("<input type=\"submit\" value=\"OFF\" style=\"width:100px;height:55px\" onClick=\"location.href='/OFF'\"");
  if (!relay)
  {
    client.print(" disabled");
  }
  client.println(">");
  client.println("<br><br><hr>");
  client.print("<br><a href=\"/settings\">Settings</a>");
  client.println("</BODY></HTML>");
  client.println();
  client.flush();
}

void sendSettingsPage(WiFiClient client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.println("<HTML><HEAD>");
  client.println("<title>WiFi Relay</title>");
  sendFavicon(client);
  client.println("</HEAD><BODY>");
  sendStyle(client);
  client.println("<h1>WiFi Relay Settings:</h1>");  
  client.println("<hr>");
  
  client.println("<h2>WiFi:</h2>"); 
  client.println("<form action=\"/save\" method=\"GET\">");
  client.println("<table><tr><td>");
  client.println("SSID:</td><td>");
  client.println("<select name=\"ssid\" size=\"1\">");
  for (int i = 0; i < wifiScanCount; i++)
  {
    client.print("<option");
    if (ssid == WiFi.SSID(i))
    {
      client.print(" selected=\"selected\"");
    }
    client.print(" value=\"");
    client.print(WiFi.SSID(i).c_str());
    client.print("\">");
    client.print(WiFi.SSID(i).c_str());
    client.print("    (");
    client.print(ssidVolume[i]);
    client.print("dBm)");
    client.println("</option>");
  }
  client.println("</select>");
  client.println("</td></tr><tr><td>");
  client.println("Password:</td><td>");
  client.print("<input type=\"password\" name=\"password\" maxlength=\"30\" size=\"30\" value=\"");
  client.print(password);
  client.println("\"></td></tr>");
  client.println("</table>");
  client.println("<br><hr>");
  
  client.println("<h2>IP:</h2>");   
  client.println("<table><tr><td>"); 
  client.println("Type:</td><td>");
  client.println("<select id=\"ipType\" name=\"ipType\" size=\"1\" onchange=\"changeIPType()\">");
  client.print("<option");
  if (ipType.startsWith("1"))
  {
    client.print(" selected=\"selected\"");
  }
  client.print(" value=\"1\">Static</option>");  
  client.print("<option ");
  if (ipType.startsWith("2"))
  {
    client.print(" selected=\"selected\"");
  }
  client.println(" value=\"2\">DHCP</option>");
  client.println("</select>");
  client.println("</td></tr><tr><td>");
  client.println("IP:</td><td>");
  client.print("<input type=\"text\" id=\"ip\" name=\"ip\" minlength=\"7\" maxlength=\"15\" size=\"15\" pattern=\"^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$\" value=\"");
  client.print(ip);
  client.println("\">"); 
  client.println("</td></tr><tr><td>"); 
  client.println("Subnet:</td><td>");
  client.print("<input type=\"text\" id=\"subnet\" name=\"subnet\" minlength=\"7\" maxlength=\"15\" size=\"15\" pattern=\"^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$\" value=\"");
  client.print(subnet);
  client.println("\">");
  client.println("</td></tr><tr><td>");   
  client.println("Default Gateway:</td><td>");
  client.print("<input type=\"text\" id=\"defaultgw\" name=\"defaultgw\" minlength=\"7\" maxlength=\"15\" size=\"15\" pattern=\"^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$\" value=\"");
  client.print(defaultgw);
  client.println("\"/>");  
  client.println("</td></tr><tr><td>");  
  client.println("DNS Server:</td><td>");
  client.print("<input type=\"text\" id=\"dns\" name=\"dns\" minlength=\"7\" maxlength=\"15\" size=\"15\" pattern=\"^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$\" value=\"");
  client.print(dns);
  client.println("\"/>");     
  client.println("</td></tr></table>");
  client.println("<br><hr>");

  client.println("<h2>Relay:</h2>");   
  client.println("<table><tr><td>"); 
  client.println("Relay on Power on:</td><td>");
  client.println("<select id=\"powerOn\" name=\"powerOn\" size=\"1\">");
  client.print("<option ");
  if (powerOn == "1")
  {
    client.print(" selected=\"selected\"");
  }
  client.print(" value=\"1\">ON</option>");  
  client.print("<option ");
  if (powerOn != "1")
  {
    client.print(" selected=\"selected\"");
  }
  client.println(" value=\"2\">OFF</option>");
  client.println("</select>");
  client.println("</td></tr></table><br><hr>");
   
  client.println("<br><input type=\"submit\" value=\"Save\"/>");  
  client.println("&nbsp;");
  client.println("<input type=\"button\" value=\"Cancel\" onClick=\"location.href='/'\">");
  client.println("</form>");
  
  
  client.println("<script>");
  client.println("function changeIPType() {");
  client.println("  if(document.getElementById(\"ipType\").value == 2) {");
  client.println("    document.getElementById(\"ip\").disabled = true");
  client.println("    document.getElementById(\"ip\").value = ''");
  client.println("    document.getElementById(\"subnet\").disabled = true");
  client.println("    document.getElementById(\"subnet\").value = ''");
  client.println("    document.getElementById(\"defaultgw\").disabled = true");
  client.println("    document.getElementById(\"defaultgw\").value = ''");
  client.println("    document.getElementById(\"dns\").disabled = true");
  client.println("    document.getElementById(\"dns\").value = ''");      
  client.println("   }");
  client.println("  else {");
  client.println("    document.getElementById(\"ip\").disabled = false");
  client.println("    document.getElementById(\"subnet\").disabled = false");
  client.println("    document.getElementById(\"defaultgw\").disabled = false");
  client.println("    document.getElementById(\"dns\").disabled = false");      
  client.println("  }");
  client.println("}");
  client.println("changeIPType();");
  client.println("</script>");
  client.println("</BODY></HTML>");
  client.println();
  client.flush();
}

void sendStyle(WiFiClient client)
{
  client.println("<style>");
  client.println("input:invalid {color: red;}");  
  client.println("body ");
  client.println("{");
  client.println("  background: #004270;");
  client.println("  background: -webkit-linear-gradient(to right, #E4E5E6, #004270);");
  client.println("  background: linear-gradient(to right, #E4E5E6, #004270);");
  client.println("  font-family: Helvetica Neue,Helvetica,Arial,sans-serif;");
  client.println("}");
  client.println("input, select");
  client.println("{");
  client.println("  border: none;");
  client.println("  color: black;");
  client.println("  text-align: left;");
  client.println("  text-decoration: none;");
  client.println("  display: inline-block;");
  client.println("  font-size: 16px;");
  client.println("  margin: 4px 2px;");
  client.println("}");
  client.println("input[type=text],input[type=password],select");
  client.println("{");
  client.println("  background-color: white;");
  client.println("  border-radius: 2px;");
  client.println("  padding: 2px 6px;");
  client.println("  border: 1px solid #004270;");
  client.println("}");
  client.println("input[type=text]:disabled");
  client.println("{");
  client.println("  background-color: lightgrey;");
  client.println("  border-radius: 2px;");
  client.println("  padding: 2px 6px;");
  client.println("  border: 1px solid #004270;");
  client.println("}");
  client.println("input[type=submit],input[type=button]");
  client.println("{");
  client.println("  background-color: white;");
  client.println("  border-radius: 4px;");
  client.println("  padding: 16px 32px;");
  client.println("  text-align: center;");
  client.println("  -webkit-transition-duration: 0.4s;");
  client.println("  transition-duration: 0.4s;");
  client.println("  cursor: pointer;");
  client.println("  border: 2px solid #004270;");
  client.println("}");
  client.println("input[type=submit]:hover, input[type=button]:hover");
  client.println("{");
  client.println("  background-color: rgb(80, 123, 153);");
  client.println("  color: white;");
  client.println("}");
  client.println("input[type=submit]:disabled");
  client.println("{");
  client.println("  background-color: #004270;");
  client.println("  color: white;");
  client.println("}");
  client.println("</style>");
}
