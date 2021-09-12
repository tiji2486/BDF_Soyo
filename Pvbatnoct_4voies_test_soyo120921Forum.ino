/* ADAPTATION
    dans le BUT d'effacer le bruit de FOND NOCTURNE.

  - Utiliser la fonction Wattmètre du routeur pour évaluer le bruit de fond.
  - Asservir la puissance d'entrée du micro onduleur venant de la batterie à la puissance demandée.

   La puissance d'entrée est coupée à un seuil max en cas de fort soutirage

*/

#define POSITIVE 1
#define NEGATIVE 0
#define ON 1  // commande positive du MOS de Commande du chargeur
#define OFF 0
byte voltageSensorPin = A3;// pin 26
byte currentSensor1Pin = A5; // pin 28
byte currentSensor2Pin = A4; //pin 27  voie2
byte currentSensor3Pin = A2; //pin 25   voie3 byte batteryvoltagePin = A1;//entrée Vbat / 10  sur pin 25 micro
byte currentSensor4Pin = A0; //pin 23  voie4

float SommeP1 = 0; //somme de P1 sur N periodes
float SommeP2 = 0; //somme de P2 sur N periodes
float SommeP3 = 0; //somme de P3 sur N periodes
float SommeP4 = 0; //somme de P3 sur N periodes

float Plissee1 = 0;
float Plissee2 = 0;
float Plissee3 = 0;
float Plissee4 = 0;

int residuel = -20 ; //  soutirage résiduel en Watt

long cycleCount = 0;
long cptperiodes = 0;
int samplesDuringThisMainsCycle = 0;
unsigned long firingDelayInMicros;

float cyclesPerSecond = 50; // flottant pour plus de précision
long noOfSamplePairs = 0;
byte polarityNow;
boolean beyondStartUpPhase = false;

int capacityOfEnergyBucket = 1000;//1500 ;
int sampleV, sampleI1, sampleI2, sampleI3, sampleI4 ; // Les valeurs de tension et courants sont des entiers dans l'espace 0 ...1023
int lastSampleV;     // valeur à la boucle précédente.
float lastFilteredV, filteredV; //  tension après filtrage pour retirer la composante continue
float prevDCoffset;          // <<--- pour filtre passe bas
float DCoffset;              // <<--- idem
float cumVdeltasThisCycle;   // <<--- idem
float sampleVminusDC;         // <<--- idem float sampleI1minusDC;         // <<--- idem
float sampleI1minusDC;         // <<--- idem
float sampleI2minusDC;         // <<--- idem
float sampleI3minusDC;
float sampleI4minusDC;
float lastSampleVminusDC;     // <<--- idem
float sumP1;  //  Somme cumulée des puissances1 à l'intérieur d'un cycle.
float sumP2;  //  idem P2
float sumP3;  //  idem P3
float sumP4;  //  idem P3

float PHASECAL; //inutilisé
float PowerCal1;  // pour conversion des valeur brutes V et I en joules.
float PowerCal2;
float PowerCal3;
float PowerCal4;

float energyInBucket = 0;
float VOLTAGECAL; // Pour determiner la tension mini de déclenchement du triac.
boolean firstLoopOfHalfCycle;

void setup()
{
  Serial.begin(500000); // pour tests

  PowerCal1 = 0.08;//  Calibration du gain de chaque voie
  PowerCal2 = 0.08 ;
  PowerCal3 = 0.08 ;
  PowerCal4 = 0.08 ;

  VOLTAGECAL = (float)679 / 471; // En volts par pas d'ADC.
  PHASECAL = 1 ;       // NON CRITIQUE

  sumP1 = 0 ;
  sumP2 = 0 ;
  sumP3 = 0 ;
  sumP4 = 0 ;
}


void loop() // Une tension 3 courants sont mesurés à chaque boucle (environ 32 par période){
{
samplesDuringThisMainsCycle++;  // incrément du nombre d'échantillons par période secteur pour calcul de puissance.

// Sauvegarde des échantillons précédents
lastSampleV = sampleV;          // pour le filtre passe haut
lastFilteredV = filteredV;      // afin d'identifier le début de chaque période
lastSampleVminusDC = sampleVminusDC;  // for phasecal calculation

// Acquisition d'une nouvelle serie d'échantillons bruts. temps total :380µS
sampleV = analogRead(voltageSensorPin);
sampleI1 = analogRead(currentSensor1Pin);
sampleI2 = analogRead(currentSensor2Pin);
sampleI3 = analogRead(currentSensor3Pin);
sampleI4 = analogRead(currentSensor4Pin);

// Soustraction de la composante continue déterminée par le filtre passe bas
sampleVminusDC  = sampleV  - DCoffset;
sampleI1minusDC = sampleI1 - DCoffset;
sampleI2minusDC = sampleI2 - DCoffset;
sampleI3minusDC = sampleI3 - DCoffset;
sampleI4minusDC = sampleI4 - DCoffset;
// Un filtre passe haut est utilisé pour déterminer le début de cycle.
filteredV = 0.996 * (lastFilteredV + sampleV - lastSampleV); // Sinus tension reconstituée
// lastFilteredV = zéro en début de cycle
// Détection de la polarité de l'alternance
byte polarityOfLastReading = polarityNow;
if (filteredV >= 0)
  polarityNow = POSITIVE;
else
  polarityNow = NEGATIVE;

if (polarityNow == POSITIVE)
{
  if (polarityOfLastReading != POSITIVE)
  {
    // +++++DEBUT d'une nouvelle sinus juste après le passage à zéro
    // Incrémentation des compteurs
    cycleCount++; //
    cptperiodes++; // pour affichage toutes les 50 périodes de Power
    firstLoopOfHalfCycle = true;

    // mise à jour du filtre passe bas pour la soustraction de la composante continue
    prevDCoffset = DCoffset;
    DCoffset = prevDCoffset + (0.015 * cumVdeltasThisCycle);

    //  Calcul la puissance réelle de toutes les mesures echantillonnées durant
    //  le cycle précedent, et determination du gain (ou la perte) en energie.
    float realPower1 = PowerCal1 * sumP1 / (float)samplesDuringThisMainsCycle;
    float realPower2 = PowerCal2 * sumP2 / (float)samplesDuringThisMainsCycle;
    float realPower3 = PowerCal3 * sumP3 / (float)samplesDuringThisMainsCycle;
    float realPower4 = PowerCal4 * sumP4 / (float)samplesDuringThisMainsCycle;
    float realEnergy = realPower1 / cyclesPerSecond;
    float realEnergy2 = realPower2 / cyclesPerSecond;
    float realEnergy3 = realPower3 / cyclesPerSecond;
    float realEnergy4 = realPower4 / cyclesPerSecond;

    if (beyondStartUpPhase == true)

      //xxxxxxxxxxxxxxxxDEPARTxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

    {
      energyInBucket += realEnergy;//Plissee1;//realEnergy  ;  // intégrateur pour régulation
      // Reduction d'énergie dans le reservoir d'une quantité "safety margin"
      // Ceci permet au système un décalage pour plus d'injection ou + de soutirage
      energyInBucket -= residuel / (cyclesPerSecond)  ;

      // Limites dans la fourchette 1 à +1500 joules ;ne peut être négatif ni nul.
      if (energyInBucket > capacityOfEnergyBucket)
      {
        energyInBucket = capacityOfEnergyBucket;
      }

      if (energyInBucket <= 0)
        energyInBucket = 0 ;
    }
    else

      // Après un reset attendre 100 périodes (2 secondes) le temps que la composante continue soit éliminée par le filtre
      if (cycleCount > 100) // deux secondes
        beyondStartUpPhase = true;
    //*******************************************
    if (energyInBucket <= 1000) {
      firingDelayInMicros =  2000;//99999;
    }
    else
      // fire immediately if energy level is above upper threshold (full power)
      if (energyInBucket >= 1000) {
        firingDelayInMicros = 0;
      }
      else
        // determine the appropriate firing point for the bucket's level
        // by using either of the following algorithms
      {
        // simple algorithm (with non-linear power response across the energy range)
        //        firingDelayInMicros = 10 * (2300 - energyInBucket);
        // complex algorithm which reflects the non-linear nature of phase-angle control.
        firingDelayInMicros = (asin((-1 * (energyInBucket - 500) / 500)) + (PI / 2)) * (10000 / PI);
        // Suppress firing at low energy levels to avoid complications with
        // logic near the end of each half-cycle of the mains.
        // This cut-off affects approximately the bottom 5% of the energy range.
        if (firingDelayInMicros > 5000) {
          firingDelayInMicros = 1000; // never fire
        }
      }
    //**********************************************

    //************   CROISIERE    *****************
    // lissage des puissances sur 10 périodes
    {
      (SommeP1 += realPower1);//P1moy lissée
      (SommeP2 += realPower2);//P2moy lissée
      (SommeP3 += realPower3);//P3moy lissée
      (SommeP4 += realPower4);//P3moy lissée

      //modifier /10 pour une mesures moins rapide augmenter
      if (cptperiodes == 10) // toutes les secondes en debut de sinus envoi datas
      {
        // Toutes les N périodes
        Plissee1 = SommeP1 / (10) ;
        Plissee2 = SommeP2 / (10) ;
        Plissee3 = SommeP3 / (10) ;
        Plissee4 = SommeP4 / (10) ;

        //on envoie sur le port série les données de conso à l'ESp ou à l'onduleur directement
        Serial.print ("  ");
        // Serial.print((realEnergy2), 0);
        Serial.print((Plissee2), 0); //A4  pour le traceur ,toutes les voies du même ordre de grandeur un seul println en fin
        Serial.print ("\t"); // TAB
        // Serial.print((realEnergy), 0);
        Serial.print((Plissee1), 0);// A5
        Serial.print ("\t");
        // Serial.print((realEnergy3), 0);
        Serial.print((Plissee3 ), 0);// A2
        Serial.print ("\t");
        // Serial.print((realEnergy4), 0);
        Serial.print((Plissee4 ), 0);// A0
        Serial.print ("\t");
        Serial.println ('\0'); // fin de string '0' très important pour Wifi !


        cptperiodes = 0;
        SommeP1 = 0;
        SommeP2 = 0;
        SommeP3 = 0;
        SommeP4 = 0;
        Plissee1 = 0;
        Plissee2 = 0;
        Plissee3 = 0;
        Plissee4 = 0;

      }

      // Réinitialisation avant un nouveau cycle secteur.
      sumP1 = 0;// somme des puissances1 instantannées
      sumP2 = 0;// somme des puissances2 instantannées
      sumP3 = 0;// somme des puissances3 instantannées
      sumP4 = 0;// somme des puissances4 instantannées
      samplesDuringThisMainsCycle = 0;
      cumVdeltasThisCycle = 0;// somme des tensions instantannées filtrées
    }  // Fin du processus spécifique au premier échantillon +ve d'un nouveau cycle secteur

    // suite du traitement des échantillons de tension POSITIFS ...
  }
  if (polarityOfLastReading != NEGATIVE)
  {
    firstLoopOfHalfCycle = true;
  }
}
// Fin du processus sur la demi alternance positive (tension)


// Processus pour TOUS les échantillons, positifs et negatifs
//------------------------------------------------------------

float  phaseShiftedVminusDC =
  lastSampleVminusDC + PHASECAL * (sampleVminusDC - lastSampleVminusDC);
float instP1 = phaseShiftedVminusDC * sampleI1minusDC; // PUISSANCE derniers échantillons V x I filtrés

float instP2 = phaseShiftedVminusDC * sampleI2minusDC; // PUISSANCE derniers échantillons V x I filtrés

float instP3 = phaseShiftedVminusDC * sampleI3minusDC; // PUISSANCE derniers échantillons V x I filtrés
float instP4 = phaseShiftedVminusDC * sampleI4minusDC; // PUISSANCE derniers échantillons V x I filtrés

sumP1 += instP1;    // accumulation à chaque boucle des puissances1 instantanées dans une période
sumP2 += instP2;    // accumulation à chaque boucle des puissances2 instantanées dans une période
sumP3 += instP3;    // accumulation à chaque boucle des puissances2 instantanées
sumP4 += instP4;    // accumulation à chaque boucle des puissances2 instantanées
cumVdeltasThisCycle += (sampleV - DCoffset); // pour usage avec filtre passe bas

}

// end of loop()

/* PARAMETRES
    57  précision Vbat
    94  OCR1B 160 =20µS  ;80=10µS
    105  Tension bat mini
    106  Tension bat haut
    109 110 111 Calibration des 3 voies en puissances
    230  gros consommateurs
    237 242 245 limite Pmax MO

*/
