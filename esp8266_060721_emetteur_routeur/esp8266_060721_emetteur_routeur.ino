// ***************SERVEUR ESP8266**************
// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//************Ota****************************
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//******************************************
// Replace with your network credentials
const char*ssid      = "00000";
const char* password = "000000000000";
String  (bucket);
String inputString = "";   // variable de stockage des données entrantes
String  Mesures ;  //contient  20 M1 tab M2 tab M3 tab '\0'
boolean stringComplete = false;  // whether the string is complet

//***************************************************************
WiFiServer server(80);
HTTPClient http;

void setup ()
{
  Serial.begin(500000);
  inputString.reserve(200);// reserve 200 bytes for the inputString

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // Serial.print(".");
  }
  // Print local IP address and start web server
  /* Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());*/
  server.begin();

  //*******************ota*******************************
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("pvbatnoc_4voies_wifi");
  ArduinoOTA.begin();
}
//*****************************************************************************
void loop ()
{
  if (Serial.available() > 0)  // routine Rx sollicité par caractères entrants
  {
    while (Serial.available()) // si données disponibles sur le port série
    { // attente bytes reception

char inChar = (char)Serial.read(); byte:

      inputString += inChar;    // add it to the inputString:
      if (inChar == '\0')   // Si fin de fichier 00  ,on sortavec flag fin de string
      {
        stringComplete = true;
        Mesures = inputString ; // on sauve

        String tab[7];// tableau 7 elements string pour contenir les mesures mes[0...4]
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
     
        bucket = tab[0];
        inputString = "";// clear the string
      }
    }
  }


  if  (stringComplete == true) // caractère "\0" reçu
  {
    stringComplete = false; // sinon rst du flag mesures recues
  }
  ArduinoOTA.handle();
  delay(10);
  WiFiClient client = server.available();   // Listen for incoming clients
}
