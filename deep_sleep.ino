#include <ESP8266HTTPClient.h>


#include <ESP8266WiFi.h>




// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(2, OUTPUT);
  Serial.begin(115200);

  // init the WiFi connection
  Serial.println();
  Serial.println();
  WiFi.begin("XX", "XX");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.println("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  HTTPClient http;
  http.begin("http://example.com?id=10"); //Specify the URL
  int httpCode = http.GET();
  Serial.println(httpCode);
  http.end();
  delay(1000);
}



// the loop function runs over and over again forever
void loop() {
  Serial.println("Start");
  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);  // wait for a second
  // Deep sleep mode for 1 hour, the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU board) is connected to the RESET pin
  Serial.println("Going to sleep now");
  ESP.deepSleep(3600e6);
}
