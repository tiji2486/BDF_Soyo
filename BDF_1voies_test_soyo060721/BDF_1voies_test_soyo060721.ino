//Wattmètre 1 voies, transmet sur le port serie la valeur du bucket pour 
// calculer la puissance du BDF à transmettre par voie Modbus a l'inverter SoyoSource
//  Tests et programme effectués à partir des travaux sur le Bruit de fond (Tignous) et le Wattmètre 4 voies ou le PVBATNOCT
#define POSITIVE 1
#define NEGATIVE 0
#define ON 1  // commande positive du MOS de Commande du chargeur
#define OFF 0
byte LEDBAT = 9; // pin 15 micro commande LED rouge
String ledbat_onoff;
byte voltageSensorPin = A3;// pin 26
byte currentSensor1Pin = A5; // pin 28

float SommeP1 = 0; //somme de P1 sur N periodes

int residuel = 10 ; //  soutirage résiduel en Watt
float Plissee1 = 0;

long cycleCount = 0;
long cptperiodes = 0;
int samplesDuringThisMainsCycle = 0;

unsigned long firingDelayInMicros;

float cyclesPerSecond = 50; // flottant pour plus de précision

long noOfSamplePairs = 0;
byte polarityNow;
float energyInBucket = 0;
boolean beyondStartUpPhase = false;

int capacityOfEnergyBucket = 1000;

int sampleV, sampleI1; //,sampleI2,sampleI3,sampleI4 ;   // Les valeurs de tension et courants sont des entiers dans l'espace 0 ...1023
int lastSampleV;     // valeur à la boucle précédente.
float lastFilteredV, filteredV; //  tension après filtrage pour retirer la composante continue

float prevDCoffset;          // <<--- pour filtre passe bas
float DCoffset;              // <<--- idem
float cumVdeltasThisCycle;   // <<--- idem
float sampleVminusDC;         // <<--- idem float sampleI1minusDC;         // <<--- idem
float sampleI1minusDC;         // <<--- idem

float lastSampleVminusDC;     // <<--- idem
float sumP1;  //  Somme cumulée des puissances1 à l'intérieur d'un cycle.

float PHASECAL; //inutilisé
float PowerCal1;  // pour conversion des valeur brutes V et I en joules.
float VOLTAGECAL; // Pour determiner la tension mini de déclenchement du triac.

boolean firstLoopOfHalfCycle;

void setup()
{
  Serial.begin(500000); // pour tests
  //Serial.println("Prod_Effisol  Conso  Prod_SMA  Prod_total  Bucket" );


  PowerCal1 = 0.2185 ;  //  Calibration du gain de chaque voie
  VOLTAGECAL = (float)679 / 471; // En volts par pas d'ADC.

  PHASECAL = 1 ;       // NON CRITIQUE

  sumP1 = 0 ;
  
}


void loop() 
{

  samplesDuringThisMainsCycle++;  // incrément du nombre d'échantillons par période secteur pour calcul de puissance.

  // Sauvegarde des échantillons précédents
  lastSampleV = sampleV;          // pour le filtre passe haut
  lastFilteredV = filteredV;      // afin d'identifier le début de chaque période
  lastSampleVminusDC = sampleVminusDC;  // for phasecal calculation

  // Acquisition d'une nouvelle serie d'échantillons bruts. temps total :380µS

  sampleV = analogRead(voltageSensorPin);
  sampleI1 = analogRead(currentSensor1Pin);
 
  // Soustraction de la composante continue déterminée par le filtre passe bas
  sampleVminusDC  = sampleV  - DCoffset;
  sampleI1minusDC = sampleI1 - DCoffset;
 
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
      float realEnergy = realPower1 / cyclesPerSecond;//Pmoy X 0.02
     
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
       
        //modifier /10 pour une mesures moins rapide augmenter
        if (cptperiodes == 10) // toutes les secondes en debut de sinus envoi datas
        {
          // Toutes les N périodes
          
          Plissee1 = SommeP1 / (10) ;       

          Serial.print ("  ");       
          Serial.print ((energyInBucket), 0);
          Serial.print ("\t");
          Serial.println ('\0'); // fin de string '0' très important pour Wifi !


          cptperiodes = 0;
          SommeP1 = 0;
          Plissee1 = 0;
        }

        // Réinitialisation avant un nouveau cycle secteur.
        sumP1 = 0;// somme des puissances1 instantannées       
        samplesDuringThisMainsCycle = 0;
        cumVdeltasThisCycle = 0;// somme des tensions instantannées filtrées
      }  // Fin du processus spécifique au premier échantillon +ve d'un nouveau cycle secteur

      // suite du traitement des échantillons de tension POSITIFS ...
    }
    if (polarityOfLastReading != NEGATIVE)
    {
      firstLoopOfHalfCycle = true;    }
  }
  // Fin du processus sur la demi alternance positive (tension)
  // Processus pour TOUS les échantillons, positifs et negatifs
  //------------------------------------------------------------

  float  phaseShiftedVminusDC = lastSampleVminusDC + PHASECAL * (sampleVminusDC - lastSampleVminusDC);
  float instP1 = phaseShiftedVminusDC * sampleI1minusDC; // PUISSANCE derniers échantillons V x I filtrés
  

  sumP1 += instP1;    // accumulation à chaque boucle des puissances1 instantanées dans une période
  cumVdeltasThisCycle += (sampleV - DCoffset); // pour usage avec filtre passe bas

}

// end of loop()
