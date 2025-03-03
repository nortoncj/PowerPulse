#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <PubSubClient.h>
#include "certs.h"
#include <ArduinoJson.h>

#define AWS_IOT_ENDPOINT "a25xd37dcl2y9g-ats.iot.us-east-1.amazonaws.com"
#define DEVICE_NAME "powerPulse"
// #define AWS_IOT_TOPIC "$aws/things/" DEVICE_NAME "/shadow/update"
#define AWS_IOT_TOPIC "powerPulse/pub"
#define AWS_IOT_PUBLISH_TOPIC "powerPulse/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "powerPulse/sub"




class AWSConnector {
  public:
  MQTTClient client = MQTTClient(512);
  WiFiClientSecure net = WiFiClientSecure();
  // PubSubClient client(net);
  bool isConnected = false;

  void messageHandler(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
  }

    void setup() {
        
    
      // Configure WiFiClientSecure to use the AWS IoT device credentials
      net.setCACert(AWS_CERT_CA);
      net.setCertificate(AWS_CERT_CRT);
      net.setPrivateKey(AWS_CERT_PRIVATE);
      


      // Connect to the MQTT broker on the AWS endpoint
      client.begin(AWS_IOT_ENDPOINT, 8883, net);
      
      // client.setServer(AWS_IOT_ENDPOINT, 8883) ;
      
      int retries = 0;
      
      while(!client.connect(DEVICE_NAME) && retries < 10) {
        retries++;
        delay(1000);
      }

      //client.setCallback(messageHandler);

      if(!client.connect(DEVICE_NAME)) {
        Serial.println("AWS IoT connection timeout!");
        isConnected = false;
        return;
      }
      
        Serial.println("Connected to AWS IoT");
        isConnected = true;
       
    }

    void sendMessage(String msg) {
      client.publish(AWS_IOT_TOPIC, msg);
    }

    void loop() {

      client.loop();
    }
};