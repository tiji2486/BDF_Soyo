//IP 192.168.0.51  OTA
//#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//************Ota****************************
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//************************************************
//*********************modbus**********************
#define RE D2
#define DE D3
int conso2 ;
int Buffer[16];
byte ReadCommand[] = { 0x24, 0x56 , 0x00 , 0x21 , 0x00 , 0x00 , 0x80 , 0x08};
byte bit4 ;//= conso2 * 256;//variable conso
byte bit5 ;//= bit4+conso2/256 ;// variable conso   var w = b4 + b5;
byte bit7; //= (264-bit4)-bit5;//CRC (264 - bit4 - bit5)= bit7
//*****************************************************************
char ssid[] = "000000000";           // SSID of your home WiFi
char pass[] = "00000000000";// password of your home WiFi

String (bucket);
String inputString = "";
String readmodbus ;
char DataRead ;
IPAddress server1(192, 168, 0, 131);    // the fix IP address of the server
WiFiClient client;                  //
// Set web server port number to 80
WiFiServer server(80);
HTTPClient http;


void setup() {
  Serial.begin(500000);               // only for debug
  WiFi.begin(ssid, pass);             // connects to the WiFi router
  IPAddress ip(192, 168, 0, 51);
  IPAddress dns(192, 168, 00, 1);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, dns, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // Serial.print(".");
  }
  // Print local IP address and start web server
  //Serial.println("");
  // Serial.println("WiFi connected.");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());
  server.begin();
  //**********************modbus******************
  pinMode(RE, OUTPUT);
  digitalWrite(RE , HIGH);
  pinMode(DE , OUTPUT);
  digitalWrite(DE , HIGH);
  //*************************************

  //******* OTA ***************
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("WemosD1_soyo");
  // ArduinoOTA.setPassword("");
  ArduinoOTA.begin();
  //********* Fin OTA ***************

}

void loop ()
{

  ArduinoOTA.handle();
  delay(10);
  clientIP_esp8266 ();
  ModbusInOut ();

}
//****************Fin Loop ****************************************
//*****************************Fonctions*****************************
void bits ()
{

  bit4 = (conso2 / 256);
  if ((bit4 < 0) or (bit4 > 256))
  {
    bit4 = (0);
  }
  bit5 = (conso2) - (bit4 * 256);
  if ((bit5 < 0) or (bit5 > 256))
  {
    bit5 = (0);
  }
  bit7 = (264 - bit4 - bit5);
  if (bit7 > 256)
  {
    bit7 = (8);
  }
}
//***********************************************************************
//************************************************************************************
void ModbusInOut () 
{
  conso2 = bucket.toInt() * 0.5 ; //0.45  ;
  bits ();
  byte ReadCommand[] = { 0x24, 0x56 , 0x00 , 0x21 , bit4 , bit5 , 0x80 , bit7};
  digitalWrite(RE , HIGH);
  digitalWrite(DE , HIGH);
  for (int i = 0; i < 8; i++)
  Serial.write( ReadCommand[i] );
  Serial.println(" ");

  delay(100);
  digitalWrite(RE , LOW);
  digitalWrite(DE , LOW);

}
//***********************************************************
void clientIP_esp8266 ()
{
  client.connect(server1, 80);   
  client.print("\r"); 
  String Mesures = client.readStringUntil('\n');    
  String tab[6];
  int r = 0, t = 0;
  for (int i = 0; i < Mesures.length(); i++)
  {
    if (Mesures.charAt(i) == (9))
    {
      tab[t] = Mesures.substring(r, i);
      r = (i + 1);
      t++;
    }
  }
 
  bucket = tab [5];
  client.flush();
  Serial.print ("Mesures:");
  Serial.println (Mesures);
}
