#include <Arduino.h>
//---------------------------Firebase Connection and Details----------------------------
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define DATABASE_URL "YOUR_DATABASE_URL"

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;

unsigned long prevFirebaseMillis = 0;

void printError(int code, const String &msg);
void connectWiFi();
void firebaseInit();
void uploadToFirebase(String data);

//---------------------------Canbus Details----------------------------
#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
char temp[128];
#define CAN0_INT 21                             // Set INT to pin 2
MCP_CAN CAN0(SS); 

void readCan();
String formatData();

void setup() {
  Serial.begin(115200);
  
  connectWiFi();
  firebaseInit();
  client.setAsyncResult(result);
  

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.
  pinMode(CAN0_INT, INPUT);
  Serial.println("MCP2515 Library Receive Example...");

}

void loop() {
  readCan();
  if (WiFi.status() == WL_CONNECTED){
    String data = formatData();
    uploadToFirebase(data);
  }
}

void readCan() {
  if(!digitalRead(CAN0_INT)) {                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %d  Data:", rxId, len);
    Serial.print(msgString);
    if((rxId & 0x40000000) == 0x40000000) {    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for(byte i = 0; i < len; i++){
        sprintf(msgString, " 0x%X", rxBuf[i]);
        Serial.print(msgString);
      }
    } 
    Serial.println();
  }

}

void printError(int code, const String &msg)
{
    Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void firebaseInit() {
  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
  ssl.setInsecure();
  initializeApp(client, app, getAuth(noAuth));
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

String formatData(){
  String data = "[";
  for (int i = 0; i < len; i++) {
    data += rxBuf[i];
    data += ",";
  }
  data.remove((data.length() - 1));
  data += "]";
  return data;
}

 void uploadToFirebase(String data) {
  // Set, push and get integer value
    String dir = "/CANBUS/ultra_cap/volt";
    Serial.print("Set Array... ");
    bool status = Database.set<object_t>(client, dir, object_t(data));
    if (status)
        Serial.println("ok");
    else
        printError(client.lastError().code(), client.lastError().message());
}