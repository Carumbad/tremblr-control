/*
 * Basic example of controlling the Tremblr via MQTT.
 * 
 * Uses WifiManager to create a default AP called "TremblrControl" to configure
 * the wifi details and host/port/topic for an MQTT server.
 * 
 * Sending a 1 or 0 toggles the Tremblr, will add more later ;-)
 * 
*/

#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <RCSwitch.h>
#include <PubSubClient.h>

#ifdef ESP32
  #include <SPIFFS.h>
#endif

//Signals for each button on the remote control
#define RC_UP 16076992
#define RC_MID 16076812
#define RC_DOWN 16076848
#define RC_LEFT 16076803
#define RC_RIGHT 16077568

//PubSubClient 
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//default values
char mqtt_server[40] = "192.168.1.188";
char mqtt_port[6]  = "1883";
char mqtt_topic[60] = "tremblr/";

//flag for saving data
bool shouldSaveConfig = false;

//Instantiate class
RCSwitch mySwitch = RCSwitch();

// WiFiManager, -Local- Global intialization. Once its business is done, there is no need to keep it around
WiFiManager wm;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupSpiffs(){
  //clean FS, for testing
  SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

/*
 * Start of MQTT Handling
 */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the Tremblr if a 1 was received as first character
  if ((char)payload[0] == '1') {
    mySwitch.send(RC_MID, 24);//Ugh, on and off is stateless
    delay(50);
    Serial.println("Turned Tremblr On... probably");
  } else {
    mySwitch.send(RC_MID, 24);
    delay(50);
    Serial.println("Turned Tremblr Off... probably");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_topic, "ONLINE");
      // ... and resubscribe
      client.subscribe(mqtt_topic); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  setupSpiffs();

  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // setup custom parameters
  // 
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 60);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_topic);

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  wm.setConnectTimeout(120);
  if (!wm.autoConnect("TremblrControl")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  // always start configportal for a little while
  // wm.setConfigPortalTimeout(60);
  // wm.startConfigPortal("TremblrControl");

  //if you get here you have connected to the WiFi
  Serial.println("Wifi is connected, ready to play!");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["mqtt_topic"]   = mqtt_topic;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());

  // Transmitter is connected to Arduino Pin #14  
  mySwitch.enableTransmit(14);
  
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
