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

String (Bucket);
String Mesures;
String inputString = "";
String readmodbus ;
int capacityOfEnergyBucket = 1000;//1500 ;
int residuel = 30;
float realEnergy;
float energyInBucket = 0;
float cyclesPerSecond = 10;
unsigned long firingDelayInMicros;

char DataRead ;

IPAddress server1(192, 168, 0, 131);    // the fix IP address of the server
WiFiClient client;                  //
// Set web server port number to 80
WiFiServer server(80);
HTTPClient http;


void setup() {
  Serial.begin(4800);               // only for debug
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
  bucketenergy ();
}
//****************Fin Loop ****************************************
//*****************************Fonctions*****************************
void bits ()
{

  bit4 = (conso / 256);
  if ((bit4 < 0) or (bit4 > 256))
  {
    bit4 = (0);
  }
  bit5 = (conso) - (bit4 * 256);
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
  conso = energyInBucket.toInt() * 0.5 ; //0.45= 450W , 0.5 = 500W coefficient de limitation,  a ajuster selon l'onduleur
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
 
  Bucket = tab [5];
  client.flush();
 // Serial.print ("Mesures:");
 // Serial.println (Mesures);
}
//************************************************************************************************
void bucketenergy ()
{
  realEnergy = Bucket.toInt() /(cyclesPerSecond);
  energyInBucket += realEnergy;  // intégrateur pour régulation

  // Reduction d'énergie dans le reservoir d'une quantité "safety margin"
  // Ceci permet au système un décalage pour plus d'injection ou + de soutirage
  energyInBucket -= residuel / (cyclesPerSecond)  ;

  // Limites dans la fourchette 1 à +1500 joules ;ne peut être négatif ni nul.
  if (energyInBucket > capacityOfEnergyBucket)
  {
    energyInBucket = capacityOfEnergyBucket;
  }

  if (energyInBucket <= 0)
  {
    energyInBucket = 0 ;
  }
  else

    // Après un reset attendre 100 périodes (2 secondes) le temps que la composante continue soit éliminée par le filtre

    //  if(cycleCount > 100) // deux secondes
    //     beyondStartUpPhase = true;
    //*******************************************
    if (energyInBucket <= 1000)
    {
      firingDelayInMicros =  2000;//99999;
    }
    else
      // fire immediately if energy level is above upper threshold (full power)
      if (energyInBucket >= 1000)
      {
        firingDelayInMicros = 0;
      }
      else      
        // détermine le point  approprié pour le niveau du Bucket
           // en utilisant l'un des algorithmes suivants
        {
         //********** algorithme simple (avec réponse en puissance non linéaire sur toute la plage d'énergie)**********
          //        firingDelayInMicros = 10 * (2300 - energyInBucket);
          
          //*********** algorithme complexe qui reflète la nature non linéaire du contrôle d'angle de phase.***********
          firingDelayInMicros = (asin((-1 * (energyInBucket - 500) / 500)) + (PI / 2)) * (10000 / PI);
         //***********************************************************************************************************
          
          // Supprimer le point à des niveaux d'énergie faibles pour éviter les complications avec
           // logique vers la fin de chaque demi-cycle du secteur.
           // Cette coupure affecte approximativement les 5 % inférieurs de la plage d'énergie.
          if (firingDelayInMicros > 5000) {
            firingDelayInMicros = 1000; 
          }
        }
}
//**********************************************
