
// GPIO0 is the boot button

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <DHT.h>
#include <DHT_U.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* Globals used for configuration only */
#define EEPROM_SIZE 1024
bool CONFIGMODE = false;
// Set web server port number to 80
WiFiServer server(80);
// client configuration
typedef struct configuration {
  char ssid[32];
  char pwd[32];
  char mqtt[32];
  char mqttuser[32];
  char mqttpwd[32];
} Config;
Config cconfig;

/* Globals used for business logic only */
#define MQTT_VERSION MQTT_VERSION_3_1_1
// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = "sensor2_dht22_s";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
// MQTT: topic
const PROGMEM char* MQTT_SENSOR_TOPIC = "/home/house/sensor1";
// sleeping time
const PROGMEM uint16_t SLEEPING_TIME_IN_SECONDS = 60; // 10 minutes x 60 seconds
// DHT - D1/GPIO5
#define DHTPIN 5

#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

/* Business logic */
// function called to publish the temperature and the humidity
void publishData(float p_temperature, float p_humidity, float p_airquality) {
  // create a JSON object
  // doc : https://github.com/bblanchon/ArduinoJson/wiki/API%20Reference
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  // INFO: the data must be converted into a string; a problem occurs when using floats...
  root["temperature"] = (String)p_temperature;
  root["humidity"] = (String)p_humidity;
  root["airquality"] = (String)p_airquality;
  root.prettyPrintTo(Serial);
  Serial.println("");
  /*
     {
        "temperature": "23.20" ,
        "humidity": "43.70"
     }
  */
  char data[200];
  root.printTo(data, root.measureLength() + 1);
  client.publish(MQTT_SENSOR_TOPIC, data, true);
  yield();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, cconfig.mqttuser, cconfig.mqttpwd)) {
      Serial.println("INFO: connected");
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
}

void businessLogicSetup() {
  dht.begin();

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(cconfig.ssid);
  WiFi.begin(cconfig.ssid, cconfig.pwd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.println("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(cconfig.mqtt, MQTT_SERVER_PORT);
  client.setCallback(callback);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void businessLogicLoop() {
  dht.begin();
  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cconfig.ssid, cconfig.pwd);
    int count = 0;
    while (!CONFIGMODE && WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      count++;
      if(count >= 500) { // if 100 attempts to connect fail, jump back to config mode!
        CONFIGMODE = true;
      }
    }
  }

  // if MQTT lost connection then reconnect
  if (!client.connected()) {
    reconnect();
  }
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read gas level from MQ135
  float aq = analogRead(A0);
  
  if (isnan(h) || isnan(t)) {
    Serial.println("ERROR: Failed to read from DHT sensor!");
  } 
  else {
    publishData(t, h, aq);
  }
  delay(360000);
}


void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT);
  Serial.println("\r\nStart TinyControl\r\n");
  
  // read config from EEPROM
  EEPROM.begin(EEPROM_SIZE);
  // print foo. Empty, because it isn't restored from EEPROM yet
  Serial.printf("before restore: ssid='%s' pwd='%s'\n", cconfig.ssid, cconfig.pwd);
  // read from EEPROM into foo
  EEPROM.get(0, cconfig);
  Serial.printf("after restore: ssid='%s' pwd='%s'\n", cconfig.ssid, cconfig.pwd);

  // business logic setup 
  businessLogicSetup();
}

void startConfigMode() {
  Serial.println("\nStart config mode: ");
  /* Set ESP32 to WiFi Station mode */
  WiFi.softAP("SETUP_HAUSSENSOR01");
  Serial.println(WiFi.softAPIP());
  /* Start config web server */
  server.begin();
}

void handleConfigClient() {
  String cl_ssid;
  String cl_pwd;
  String cl_mqtt;
  String cl_mqttuser;
  String cl_mqttpwd;
  // Variable to store the HTTP request
  String header;
  /* Start webserver for retrieval of configuration */
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) { // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /save") >= 0) {
              Serial.println("Save configuration");
              Serial.println(header);
              cl_ssid = header.substring(header.indexOf("ssid=") + 5, header.indexOf("&password"));
              cl_pwd = header.substring(header.indexOf("&password=") + 10, header.indexOf("&mqtt="));
              cl_mqtt = header.substring(header.indexOf("&mqtt=") + 6, header.indexOf("&mqttuser="));
              cl_mqttuser = header.substring(header.indexOf("&mqttuser=") + 10, header.indexOf("&mqttpwd="));
              cl_mqttpwd = header.substring(header.indexOf("&mqttpwd=") + 9, header.indexOf(" HTTP/1.1"));
              Serial.println(cl_ssid);
              Serial.println(cl_pwd);
              Serial.println(cl_mqtt);
              Serial.println(cl_mqttuser);
              Serial.println(cl_mqttpwd);
              // save config to EEPROM
              EEPROM.begin(EEPROM_SIZE);
              strcpy(cconfig.ssid, cl_ssid.c_str());
              strcpy(cconfig.pwd, cl_pwd.c_str());
              strcpy(cconfig.mqtt, cl_mqtt.c_str());
              strcpy(cconfig.mqttuser, cl_mqttuser.c_str());
              strcpy(cconfig.mqttpwd, cl_mqttpwd.c_str());
              //save to EEPROM
              EEPROM.put( 0, cconfig);
              EEPROM.commit();
              CONFIGMODE = false;
            } 
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>TinyControl configuration</h1>");
            client.println("<form action='/save'>Wlan SSID:<input type='text' name='ssid'><br>Password:<input type='text' name='password'><br>MQTT server address:<input type='text' name='mqtt'><br>MQTT user:<input type='text' name='mqttuser'><br>MQTT pwd:<input type='text' name='mqttpwd'><br><br><input type='submit' value='Submit'></form>");
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void loop() {
  // Check if 'boot' button is pressed for entering configuration mode
  bool bstatus = digitalRead(0); 
  if (bstatus == LOW)
  { 
    delay (1000); 
    Serial.println("\r\Entering configuration mode\r\n");
    startConfigMode();
    CONFIGMODE = true;
  }

  if(CONFIGMODE) {
    handleConfigClient();
    delay (100); 
  }
  else {
    businessLogicLoop();
  }
  
}
