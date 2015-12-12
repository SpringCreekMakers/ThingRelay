#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WiFiMulti.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h> 
#include <ESP8266WebServer.h>

//Initializing Objects 
ESP8266WiFiMulti wifiMulti;
DNSServer dnsServer;
ESP8266WebServer server(80);
WiFiServer meshServer(4011);


//Defining Pins 
#define led 5
const int DPin12 = 12;

//Constaint Variables (these don't change)
const byte DNS_PORT = 53;

//Default Variables (can be changed)
IPAddress apIP(192, 168, 1, 1);
String iotNetworkPrefix = "New Thing";
String thingName = "New Thing";
String apSSID = "New Thing";
char* apPASS = "default";
String dnsDomain = "default";
String rawSerial = "serial_monitor"; //By default should be disabled unless testing. Can be: disabled, serial_monitor, http_monitor, serial_pin_relay, serial_mesh_relay

//Global Variables 
String stage; //Can be one of the following: new, configured, invalid_APs
String role; //Can be one of the following: access_point, client, unknown
String AProle; //Can be one of the following: isolated, connected, unknown
char esids[2];
char epass[2];
bool esid;

//Helper Functions 
void setWifiRole() { //Making sure we're in the right WiFi mode based on the devices set role
  if (stage == "new") {
    role = "access_point";
    AProle = "isolated";
  }
  if (role == "access_point") {
    if (AProle == "isolated") {
      WiFi.mode(WIFI_AP);
    }
    else if (AProle == "connected") {
      WiFi.mode(WIFI_AP_STA);
    }
  } 
  else if (role == "client") {
    WiFi.mode(WIFI_STA);
  }
}

bool testWifi() { //We're testing to see if the Access Points we have in memory are valid or accessable
  int c = 0;
  Serial1.println("Connecting Wifi... hopefully they're still there :/ ");  
  while ( c < 20 ) {
    if(wifiMulti.run() == WL_CONNECTED) { return true; } 
    delay(500);
    Serial1.print(WiFi.status());    
    c++;
  }
  Serial1.println("Nobody answered. Guess we better find new friends :( ");
  stage = "invalid_APs";
  return false;
} 

String wifiScan() { //Locating accessiable Access Points. Depending on my role this can be another esp8266 or a web connected router
  Serial1.println("Looking for available WiFi networks. Who's all out there?");
  int n = WiFi.scanNetworks();
  Serial1.println("All done");
  if (n == 0)
    Serial1.println("No networks found. I feel so along :(");
  else
  {
    Serial1.print(n);
    Serial1.println(" networks found. I'm not the only one!!!");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial1.print(i + 1);
      Serial1.print(": ");
      Serial1.print(WiFi.SSID(i));
      Serial1.print(" (");
      Serial1.print(WiFi.RSSI(i));
      Serial1.print(")");
      Serial1.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
     }
  }
  Serial1.println(""); 
  String st = "<ul>";
  for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<li>";
      st +=i + 1;
      st += ": ";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      st += "</li>";
    }
  st += "</ul>";
  delay(100);
  return st;
}

String macPrefix() { //Returns the last two HEX characters of the devices mac address
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  return macID;
}

String chipID() { //Returns the ESP8266 Chip ID 
  uint32_t chipID;
  chipID = ESP.getChipId();
  return String(chipID);
}

void handleRawSerial() { //Determines how raw serial data is handled
  if (rawSerial != "disabled") {
    if (rawSerial == "serial_monitor") {
      if (Serial.available() > 0) {
        int i = Serial.read();
        Serial1.print(i);
        Serial1.print(" ");
      }
    }
  }
}

void pinStateMonitor() {
  static int buttonState = 0;         // current state of the button
  static int lastButtonState = 0;     // previous state of the button

  // read the pushbutton input pin:
  buttonState = digitalRead(DPin12);

  // compare the buttonState to its previous state
  if (buttonState != lastButtonState) {
    if (buttonState == HIGH) {
      Serial1.println("Digital pin 12 is HIGH");
    } else {
      Serial1.println("Digital pin 12 is LOW");
    }
    // Delay a little bit to avoid bouncing
    delay(50);
  }
  // save the current state as the last state,
  //for next time through the loop
  lastButtonState = buttonState;
}

void tempMem() {
static  int address = 0;
char value;

  // read a byte from the current address of the EEPROM
  value = EEPROM.read(address);

  Serial1.print(address);
  Serial1.print("\t");
  Serial1.print(value, DEC);
  Serial1.println();

  // advance to the next address of the EEPROM
  address = address + 1;

  // there are only 512 bytes of EEPROM, from 0 to 511, so if we're
  // on address 512, wrap around to address 0
  if (address == 512)
    address = 0;

  delay(500);
}

//Memory Functions
  // first byte is a start byte 224
  // next are: variable key (1byte), location of first byte(3), and lenth(3) - blockIDs
    //N - Settings Name
    //S - Settings Security
    //R - Setting Role 
    //C - Settings Controller
    //W - Settings WiFi Networks
    //D - Settings Pins
    //P - Settings Power
    //A - Settings Advanced
char blockStart = 2;
int blockSize = 512; //Size of EEPROM in Bytes
int blockRefStart = 1; //Hard coded location (byte) of the start to the block reference section
int blockRefLength = 3; //Each BlockID Ref is currently 3bytes
int blockIDCount = 8; //Currently 8 block IDs
char blockIDs[9] = {'N', 'S', 'R', 'C', 'W', 'D', 'P', 'A'};
int blockRefStop = blockRefLength * blockIDCount + blockRefStart;

bool initBlockRef() { //Initializes the EEPROM block if: it is blank, is corrupt, or is being refreshed
  if (EEPROM.read(0) != blockStart) {// ASCII lowercase Alpha
    int i = 0;
    for (int x = blockRefStart; (x <= blockRefStop) && (i <= (blockIDCount - 1));x += blockRefLength) {
      EEPROM.write(x, blockIDs[i]);
      EEPROM.commit();
      for (int j = 1; j <= 2; j++) {
        char a = 0;
        EEPROM.write(x + j, a);
        EEPROM.commit();
      }
      i++;
    }
    EEPROM.write(0, blockStart);
    EEPROM.commit();
    return true; 
  }
  return false;
}

int availableBlock(char blockID, int bytesReq) { //Locates the first available big enough block
  char _blockID = blockID;
  int _bytesReq = bytesReq;
   
  
  bool _currentBlock = false; //Is there a current block 
  char _currentBlockLength;
  char _currentBlockStart;
  bool _currentBlockAvailable = false; //Can we use the current block
  char _availableBlock;
  
  if (EEPROM.read(0) == blockStart) { // 11100000 is Binary for ASCII lowercase Alpha 
    for (int x = blockRefStart; x <= blockRefStop; x += blockRefLength) { //Checking to see if there is a current block and if so what it's length is
      if (EEPROM.read(x) == _blockID) {
        _currentBlock = true;
        _currentBlockStart  = (EEPROM.read(x + 1));
        _currentBlockLength = (EEPROM.read(x + 2));
        if (_currentBlockLength >= _bytesReq) { //If this is true we can use the current block
          _currentBlockAvailable = true;
          _availableBlock = _currentBlockStart;
          return _availableBlock;
        }
        break;
      }
    }
    if ((_currentBlockAvailable == false) || (_currentBlock == false)) {
      //Since current block is not big enough or is not available we must locate one that is
      char _blocks[blockIDCount * 2];
      int _blocksPointer = 0;
      for (int x = blockRefStart; x <= blockRefStop; x += blockRefLength) {
        char _startByte;
        char _blockLength;
        _startByte = (EEPROM.read(x + 1));
        _blockLength = (EEPROM.read(x + 2));
        _blocks[_blocksPointer] = _startByte;
        _blocksPointer++;
        _blocks[_blocksPointer] = _blockLength;
        _blocksPointer++;
      }
      char _highestStartByte = blockRefStop + 1;
      char _highestTotalBytes = 0;
      for (int x = 0; x <= (blockIDCount * 2); x += 2) {
        if (_blocks[x] > _highestStartByte) {
          _highestStartByte = _blocks[x];
          _highestTotalBytes = _blocks[x + 1];
        }
      }
      Serial1.println(_highestStartByte);
      if (((_highestStartByte - 1) + _highestTotalBytes + _bytesReq) < blockSize) {
        _availableBlock = _highestStartByte + _highestTotalBytes;
        return _availableBlock;
      }
    }
    return -1;
  }
  return -1;
}

bool setBlock() {
  
}

bool setDeviceName(String prefix, String deviceName) {
  prefix.trim();
  String _prefix = prefix;
  deviceName.trim();
  String _deviceName = deviceName;
  char _blockID = 'N'; //Block ID for Settings Name
  int _bytesReq = 7; // N,P,x,x,(char is Prefix) + U,x,x, (char is Name) 
  initBlockRef();
  _bytesReq += _prefix.length();
  _bytesReq += _deviceName.length();
  int _availableBlock = availableBlock(_blockID, _bytesReq);
  Serial1.println(_availableBlock);
  return true;
}

void setSec() {
  
}

void setRole() {
  
}

void setCont() {
  
}

void setWifi() {
  
}

void setPins() {
  
}

void setPower() {
  
}

void setAdv() {
  
}

bool getSettings() { //Checks and retrieves data from EEPROM memory
  // first byte is a start byte 224
  // next are variable key (2byte), location of first byte(3), and lenth(3) - blockIDs
    //N - Settings Name
    //S - Settings Security
    //R - Setting Role 
    //C - Settings Controller
    //W - Settings WiFi Networks
    //D - Settings Pins
    //P - Settings Power
    //A - Settings Advanced
    
  
  Serial1.println("Determining if there is any current settings in my itty bitty 512byte hard-drive");
   
  /** if (EEPROM.read(0) == blockStart) { // 11100000 is Binary for ASCII lowercase Alpha
    
    int byteIndex = 0; // A variable used to track my current location in memory. 
    Serial1.println("Yep, they're there. Reading current settings");
    Serial1.println("Trying to figure out WHAT I am....");
    byteIndex += 1; //Moving to the next byte
    switch (EEPROM.read(byteIndex)) { //Determining Role
    case 65:
      //Role: Access Point (65 is DEC for ASCII capital letter A)
      role = "access_point";
      break;
    case 67:
      //Role: Client (67 is DEC for ASCII capital letter C)
      role = "client";
      break;
    default:
      //Role: Unknown
      role = "unknown";
    break;
    }
    Serial1.println("I'm a: " + role);
    // Name 32chars, Type I.E input output bidirectional 1char, ID 16char, Reporting webaddress 50char
    byteIndex += 1;
    if (role == "access_point") { //If device is an Access Point determine type otherwise move on
      switch (EEPROM.read(byteIndex)) {
      case 73:
        //APRole: Isolated (65 is DEC for ASCII capital letter I)
        AProle = "isolated";
        break;
      case 67:
        //APRole: Connected (67 is DEC for ASCII capital letter C)
        AProle = "connected";
        break;
      default:
        //APRole: Unknown
        AProle = "unknown";
      break;
      }
    }
    if ((role == "access_point" && AProle == "connected") || (role == "client")) { //If device is a connected Access Point or client determine if there 
      //is any stored APs if not move on
      Serial1.println("Looking for Access Points I remember. My programming allows me to remember up to 3! :)");
      //Determining if there are stored Access Points
      if (EEPROM.read(byteIndex) == 49) {
        esid = true;
         byteIndex += 1;
         
          for (int i = byteIndex; i < byteIndex + 32; ++i) {
            esids[0] += char(EEPROM.read(i));
          }
          byteIndex += 32;
          for (int i = byteIndex; i < byteIndex + 64; ++i) {
            epass[0] += char(EEPROM.read(i));
          }
          byteIndex += 64;
          Serial1.print("SSID1: ");
          Serial1.println(esids[0]);
          wifiMulti.addAP(&esids[0], &epass[0]);
      }
      else {
        goto memoryCheckCompleted; //Using a goto statement is frowned on and can make code hard to debug. But can be useful on rare ocasions.
        // Here it's used to skip the next two check if there is no Access Points in memory. This is determined by a DEC 49, which is a 1 or True.
      }
      if (EEPROM.read(byteIndex + 1) == 49) {
         byteIndex += 1;
          for (int i = byteIndex; i < byteIndex + 32; ++i) {
            esids[1] += char(EEPROM.read(i));
          }
          byteIndex += 32;
          for (int i = byteIndex; i < byteIndex + 64; ++i) {
            epass[1] += char(EEPROM.read(i));
          }
          byteIndex += 64;
          Serial1.print("SSID2: ");
          Serial1.println(esids[1]);
          wifiMulti.addAP(&esids[1], &epass[1]);
      }
      else {
        goto memoryCheckCompleted; 
      }
      if (EEPROM.read(byteIndex + 1) == 49) {
         byteIndex += 1;
          for (int i = byteIndex; i < byteIndex + 32; ++i) {
            esids[2] += char(EEPROM.read(i));
          }
          byteIndex += 32;
          for (int i = byteIndex; i < byteIndex + 64; ++i) {
            epass[2] += char(EEPROM.read(i));
          }
          byteIndex += 64;
          Serial1.print("SSID3: ");
          Serial1.println(esids[2]);
          wifiMulti.addAP(&esids[2], &epass[2]);
      }
    }
    memoryCheckCompleted:
    return true;
  }
  else {
    return false;
  }**/
  return false;
}

// HTML Content
//Reusable Code Blocks 
String headTagContent(String title) { //HTML code for the head section of devices webpages. Made into function so it can be reusable instead of repeating code
  Serial1.print("Received a HTTP request for: ");
  Serial1.println(server.uri());
  String content = "<html>\n <head>\n <title>";
  if (stage == "new") {
    content += "A New Thing - " + title + " (" + macPrefix() + ")</title>\n";
  }
  else {
    content += iotNetworkPrefix + " " + thingName + " - " + title +"</title>\n";
  }
  content += "<style>\n body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\n </style>\n";
  content += "</head>\n";
  return content;
}

String collectSettingsName() {
  String content;
  content += "<form method='post' action=''>\n";
  content += "<h2>Please choose a prefix for your IoT Network</h2>\n<h2>This could be something as simple as 'My House'. 10 Characters Max<br>";
  content += "IoT Network Prefix:<br><input type='text' name='prefix' value='My House' maxlength='10'><br>Please choose a name for this device.";
  content += "This is so you can identify the device from others in your network, something like 'Motion Sensor'. 15 Caracters Max<br> Unique Name:<br>";
  content += "<input type='text' name='deviceName' value='Motion Sensor' maxlength='15'><br>";
  content += "<input type='submit' value='Submit'>\n </form>"; 
  return content; 
}

String displaySettingsName() {
  //html code here
}

String collectSettingsSec() {
  String content;
    content += "It worked! It just didn't like not having content!";
  return content;     
}

String displaySettingsSec() {
            
}

String collectSettingsRole() {
            
}

String displaySettingsRole() {
            
}

String collectSettingsCont() {
            
}
 
String displaySettingsCont() {
            
}

String collectSettingsWifi() {
  
}

String displaySettingsWifi() {
  
}

String collectSettingsPins() {
              
}

String displaySettingsPins() {
              
}

String collectSettingsPower() {
            
}
 
String displaySettingsPower() {
            
}

String collectSettingsAdv() {
            
}

String displaySettingsAdv() {
            
}

String displayNotFound() {
  
}
//HTTP Request Helper Functions
String settingsCDSwitch(String page) { //Determines the Settings collection or display page to return based on request uri
  String _pageUri = "/settings/" + page;
  String _page = page;
  String _content;
  
  //Collect-Display
  if (stage.startsWith("new")) {
    stage = "new_" + _page;
    if (_page == "name") _content = collectSettingsName();
    if (_page == "sec") _content = collectSettingsSec();
    if (_page == "role") _content = collectSettingsRole();
    if (_page == "cont") _content = collectSettingsCont();
    if (_page == "wifi") _content = collectSettingsWifi();
    if (_page == "pins") _content = collectSettingsPins();
    if (_page == "power") _content = collectSettingsPower();
    if (_page == "adv") _content = collectSettingsAdv();
  }
  else {
    if (_page == "name") _content += displaySettingsName();
    if (_page == "sec") _content += displaySettingsSec();
    if (_page == "role") _content += displaySettingsRole();
    if (_page == "cont") _content += displaySettingsCont();
    if (_page == "wifi") _content += displaySettingsWifi();
    if (_page == "pins") _content += displaySettingsPins();
    if (_page == "power") _content += displaySettingsPower();
    if (_page == "adv") _content += displaySettingsAdv(); 
    _content += "<h2>If you would like to change these settings please fill out the form below</h2><br>";
    if (_page == "name") _content += collectSettingsName();
    if (_page == "sec") _content += collectSettingsSec();
    if (_page == "role") _content += collectSettingsRole();
    if (_page == "cont") _content += collectSettingsCont();
    if (_page == "wifi") _content += collectSettingsWifi();
    if (_page == "pins") _content += collectSettingsPins();
    if (_page == "power") _content += collectSettingsPower();
    if (_page == "adv") _content += collectSettingsAdv();  
  }
  return _content;  
}

String settingsHSNav(String page, bool success) { //Determines the Setting Nav buttons to display based on the success or failure of supplied variables
  String _page = page;
  String _nextPage;
  bool _success = success;
  String _content;

  if (_page == "name") _nextPage = "sec";
  if (_page == "sec") _nextPage = "role";
  if (_page == "role") _nextPage = "cont";
  if (_page == "cont") _nextPage = "wifi";
  if (_page == "wifi") _nextPage = "pins";
  if (_page == "pins") _nextPage = "power";
  if (_page == "power") _nextPage = "adv";
  //Handle Submit Nav
  if (_success == true) {
    if ((_page == "adv") && (stage == "new_adv")) {
      stage = "configured";
      _content += "<h2>All Done!<br>Would you like to return to the <a href='../'>Setting Page</a> or <a href='../../'>Home Page</a></h2>";
    } 
    else {
      if (stage.startsWith("new")) {
        _content += "Please proceed to the next page<br><form method='get' action='../" + _nextPage + "/'><input type='submit' value='Next'>\n </form>";
      }
      else {
        _content += "<h2>Would you like to return to the <a href='../'>Setting Page</a> or <a href='../../'>Home Page</a></h2>";
      }
    }
  }
  else {
    _content += "<h2>Sorry something went wrong please try again</h2><br>";
    if (_page == "name") _content += collectSettingsName();
    if (_page == "sec") _content += collectSettingsSec();
    if (_page == "role") _content += collectSettingsRole();
    if (_page == "cont") _content += collectSettingsCont();
    if (_page == "wifi") _content += collectSettingsWifi();
    if (_page == "pins") _content += collectSettingsPins();
    if (_page == "power") _content += collectSettingsPower();
    if (_page == "adv") _content += collectSettingsAdv(); 
  }
  return _content;
}

//Webpages
void handleRoot() { //The web servers home page 
  String content = headTagContent("Home");
    content += "<body>\n <h1>Hello I'm " + thingName;
    content += (stage == "new")? " " + macPrefix():"";
    content += "</h1>\n";
    if (stage == "new") {
      content += "<h4>I have not been configured!";
    } 
    else if (stage.startsWith("new")) {
      content += "<h4>I have not been completely configured!";
    }
    if (stage.startsWith("new")) content += " Please take the time to do so now in the settings page.</h4>";  
    content += "<h3>Please choose from the following actions:</h3>\n";
    content += "<a href='status/'><h3>Thing Status</h3></a>\n";
    content += "<a href='devices/'><h3>IoT Network Devices</h3></a>\n";
    content += "<a href='wifinetworks/'><h3>WiFi Networks</h3></a>\n";
    content += "<a href='pincontrol/'><h3>Pin Control</h3></a>\n";
    content += "<a href='data/'><h3>Raw Data</h3></a>\n";
    content += "<br><a href='settings/'><h3>Settings</h3></a>\n";
    //content += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><br><label>Password: </label><input name='pass' length=64><br><input type='submit'></form>";
    content += "</body>\n </html>";
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handleSettings() { //Responds with current settings and allows a user to update or add settings
// Name - Prefix(10), device name(15) UID Prefix(4), Device Role Prefix(2) 
// Security - HTTPS, Password Protected(64), open
// Device Role - Controller, Access Point, Router, or Client
//  IF Access Point - Connected or Isolated
//  IF Client - 
// Mesh Role - Client, Router, Access Point, Disabled
// Controller - web API, Controller Device 
// Wifi Networks
// Power Management
// Pin settings
  String content = headTagContent("Settings");
      content += "<body>\n";
      //Settings Home Page 
        if ((stage.startsWith("new")) && (server.uri() == "/settings/")) {
          content += "<h1>Hello I'm a New Thing</h1>\n <h2>Please take a moment and answer the following questions:</h2>\n";
          content += "<h3>Don't worry there are only one or two questions per page.";
          content += "My webpages can't be to big, I mean I'm only a few chips that add up to maybe the size of a quarter!</h3>\n";
          content += "<form method='get' action='name/'><center><input type='submit' value='Start'></center></form>\n";
        }
        else if (stage == "invalid_APs") {
          
        }
        else if (stage == "configured") {
          content += "<h2>Settings</h2>\n <h3>Please choose from the following settings</h3><br>";
          content += "<ul><li><a href='/name/'>IoT Network Prefix/Device Name</a></li>";
          content += "<li><a href='/sec/'>Security</a></li><li><a href='/role/'>Device Role</a></li><li><a href='/cont/'>Controller</a></li>";
          content += "<li><a href='/pins/'>Pin Settings</a></li><li><a href='/power/'>Power Management</a></li><li><a href='/adv/'>Advanced</a></li>";
        }
        
      //Settings - Name
        //Collect-Display
          if ((server.uri() == "/settings/name/") && (server.args() == 0)) {
            content += settingsCDSwitch("name");
          }
        //Handle Submit
          if ((server.uri() == "/settings/name/") && (server.args() > 0)) {
            if ((server.arg("prefix") != NULL) && (server.arg("deviceName") != NULL)) {
              if (setDeviceName(server.arg("prefix"),server.arg("deviceName"))) {
                content += "<h2>The IoT Network Prefix and Unique Name have been updated :) </h2>\n";
                content += settingsHSNav("name", true);
              }
              else {
                content += settingsHSNav("name", false);
              }
            }
            else {
              content += settingsHSNav("name", false);
            }
          }
      //Security
        //Collect-Display
          if ((server.uri() == "/settings/sec/") && (server.args() == 0)) { 
            content += settingsCDSwitch("sec");
          }
        //Handle Submit
          if ((server.uri() == "/settings/sec/") && (server.args() > 0)) {
            content += "we have an issue";
          }
      //Device Role
       //Collect-Display
        if ((server.uri() == "/settings/role/") && (server.args() == 0)) {
          content += settingsCDSwitch("role");
        }
        //Handle Submit
          if ((server.uri() == "/settings/role/") && (server.args() > 0)) {
            
          }
      //Controller
       //Collect-Display
        if ((server.uri() == "/settings/cont/") && (server.args() == 0)) {
          content += settingsCDSwitch("cont");
        }
        //Handle Submit
          if ((server.uri() == "/settings/cont/") && (server.args() > 0)) {
            
          }
      //Wifi Settings 
       //Collect-Display
        if ((server.uri() == "/settings/wifi/") && (server.args() == 0)) {
          content += settingsCDSwitch("cont");
        }
        //Handle Submit
          if ((server.uri() == "/settings/wifi/") && (server.args() > 0)) {
            
          }
      //Pin Settings
       //Collect-Display
        if ((server.uri() == "/settings/pins/") && (server.args() == 0)) {
          content += settingsCDSwitch("pins");
        }
        //Handle Submit
          if ((server.uri() == "/settings/pins/") && (server.args() > 0)) {
            
          }
      //Power Management
       //Collect-Display
        if ((server.uri() == "/settings/power/") && (server.args() == 0)) {
          content += settingsCDSwitch("power"); 
        }
        //Handle Submit
          if ((server.uri() == "/settings/power/") && (server.args() > 0)) {
            
          }
      //Advance
       //Collect-Display
        if ((server.uri() == "/settings/adv/") && (server.args() == 0)) {
          content += settingsCDSwitch("adv"); 
        }
        //Handle Submit
          if ((server.uri() == "/settings/adv/") && (server.args() > 0)) {
                   
          }
      //Footer    
      content += "</body>\n </html>";
   Serial1.println("Sending HTML Response code '200 OK'");
   server.send(200, "text/html", content);
}

void handleStatus() { //Responds with device status and settings
  String content = headTagContent("Device Status");
    content +=
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handleDevices() { //Responds with a list of IoT Devices
  String content = headTagContent("IoT Devices");
    content +=
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handleWiFiNetworks() { //Responds with a list of available networks
  String content = headTagContent("WiFi Networks");
    content += "<body>\n <h1>Available WiFi Networks<h1>\n";
    content += "<a href='../'><center><h4>Home</h4></center></a><br>";
    content += wifiScan();
    content += "\n <h4>To add a WiFi Network to the list of networks this device can attach to,\n";
    content += "please check out the WiFi Network section of the <a href='../settings/'>Settings page</a></h4>";
    content += "</body>\n </html>";   
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handlePinControl() { //Responds with a pin control panel
  String content = headTagContent("Pin Control");
    content +=
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handleData() { //Responds with raw data pages
  String content = headTagContent("Data");
    content +=
    //content += "<a href='commdata/'><h3>Raw Serial Data</h3></a>\n";
    //content += "<a href='meshcomm/'><h3>Raw Mesh Network Communications</h3></a>\n";
  Serial1.println("Sending HTML Response code '200 OK'");
  server.send(200, "text/html", content);
}

void handleNotFound() { //Response for invalid web address - 404 Not Found
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }
  Serial1.println("Sending HTML Response code '404 Not Found'");
  server.send ( 404, "text/plain", message );
}

//Available Services 
void launchDNS() { //Launching a DNS Server to allow direct access to the HTTP Server when this Thing is a Access Point
  Serial1.println("Now I'm making it so you don't have to enter an IP every time you want to see my webpage");
  Serial1.println("This will only work if I'm your one and only ;) Well okay just your Access Point");
  // modify TTL associated  with the domain name (in seconds)
  // default is 60 seconds
  dnsServer.setTTL(60);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is DNSReplyCode::NonExistentDomain
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server for a specific domain name
  String domain = (dnsDomain == "default")? "thing" + macPrefix() + ".local": dnsDomain + ".local";
  dnsServer.start(DNS_PORT, domain, apIP);
  Serial1.println("So as long as we're connected (as your Access Point) you can see my web page at: ");
  Serial1.print("http://");
  Serial1.println(domain);
}

void launchWebServer() { //Luunching an internal Web Server to host a basic html website
  Serial1.println("Setting things up to show the world! Okay, maybe just you. But you never know!");
  Serial1.println("Starting HTTP Server");
  server.on("/", handleRoot);
  //Settings
  server.on("/settings/", handleSettings);
  server.on("/settings/name/", handleSettings);
  server.on("/settings/sec/", handleSettings);
  server.on("/settings/role/", handleSettings);
  server.on("/settings/cont/", handleSettings);
  server.on("/settings/wifi/", handleSettings);
  server.on("/settings/pins/", handleSettings);
  server.on("/settings/power/", handleSettings);
  server.on("/settings/adv/", handleSettings);
  
  server.on("/status/", handleStatus);
  server.on("/devices/", handleDevices);
  server.on("/wifinetworks/", handleWiFiNetworks);
  server.on("/pincontrol/", handlePinControl);
  server.on("/data/", handleData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial1.print("HTTP server started. Check it out at: " );
  Serial1.println((role == "access_point") ? WiFi.softAPIP() : WiFi.localIP());
  if (role == "access_point") {
    launchDNS();       
  }
}

void launchAP() { //Setting up as an Access Point to provide an Access Point for clients and users 
  Serial1.println("Making it so you can connect directly to me! I like making new connections!");
  String lap;
  (stage == "new")? lap = apSSID + " - " + macPrefix(): apSSID;
  char lapSSID[35];
  lap.toCharArray(lapSSID, sizeof(lapSSID));
  (apPASS == "default")? WiFi.softAP(lapSSID): WiFi.softAP(lapSSID, apPASS);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  delay(1000);
  Serial1.println("It worked! You should be able to connect to me at the following: ");
  Serial1.print("AP IP Address: ");
  Serial1.println(WiFi.softAPIP());
  Serial1.print("AP SSID: ");
  Serial1.println(lapSSID);
  Serial1.print("AP Pass: ");
  Serial1.println((apPASS == "default")? "Open Network": apPASS);
}

//Required Functions
void setup() { //Runs when device first boots. It's a kind of staging area. Only ran once at start up
  
  Serial1.begin(9600); //This is how fast I talk. If you are seeing strange characters in your serial monitor this is probably set wrong in your program.
  Serial.begin(9600);
  EEPROM.begin(blockSize); // I have 512Bytes of EEPROM memory. It's kind of like a SSD. Anything placed in here will remain if I lose power.
  delay(10);
  Serial1.println();
  Serial1.println("Thing Relay v0.0.0- By Spring Creek Makers");
  Serial1.println("Starting up....");
  
  // Determine current settings if any
  if (getSettings()) {
    if ((role == "access_point" && AProle == "connected") || (role == "client")) {
      if (esid == true) {
        // test esids
        setWifiRole();
        if ( testWifi()) {
          Serial1.println("We're connected to an Access Point! I feel so... well... connected!");
          Serial1.println(WiFi.localIP());
          launchWebServer();
          if (role == "access_point") {
            launchAP();
          }
        }
        else {
          Serial1.println("Launching as an Access Point so you can update my settings. That way I can find those new friends");
          launchAP();
          launchWebServer();
        } 
      }
      else {
        Serial1.println("Well doesn't look like I have any friends (Access Points/other Things).");
        Serial1.println("So I'm launching as an Access Point so you can update my settings. That way I can find those new friends");
        launchAP();
        launchWebServer();
      }
    }
    else {
      setWifiRole();
      launchAP();
      launchWebServer();
    }
  }
  else {
    Serial1.println("Well looks like I'm a clean slate. You'll need to set things up after I start up my HTTP Server");
    stage = "new";
    setWifiRole();
    launchAP();
    launchWebServer();
  }
  //Initializing pins 
  pinMode(DPin12, INPUT);
  
}

void loop() { //Repeating loop. For functions that involve dynamic data or require user/device input
  dnsServer.processNextRequest();
  server.handleClient();
  pinStateMonitor();
  handleRawSerial();
  tempMem();
}
