# ThingRelay

## Under Development - No Stable Version 

For use with the ESP8266 and the Arduino libraries located at:
https://github.com/esp8266/Arduino

### TO-DO List

- Code memory functions
	- setDeviceName(), setBlock(), and setBlockRef() functions need to be tested 
- Complete HTML for Settings
- Pin management functions
- Pin/Serial Information Relay (RESTful API, MQTT, Mesh Network(Controller))
	- Research Sparkfun Phant.io, Thingspeak, AWS IoT regarding RESTful API and MQTT
	- Research XBee and ESP8266 Arduino regarding Mesh Network Controller
- Add Mesh Ad-Hoc Network
- Convert to Arduino Libraries
	- Research: http://playground.arduino.cc/Code/Library

### Notes

Currently requires the following patch (http://www.esp8266.com/viewtopic.php?p=21726#p21726) 
to the ESP8266 Arduino DNSServer library in order for Windows and Android browsers to be able to use DNS function. 