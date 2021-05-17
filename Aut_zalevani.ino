// Půdní vlhkoměr
// připojení knihovny U8glib
#include "U8glib.h"

// inicializace OLED displeje z knihovny U8glib
U8GLIB_SSD1306_128X32 mujOled(U8G_I2C_OPT_NONE);

// nastavení čísel propojovacích pinů
#define analogPin1 A0
#define vccPin1 5
#define analogPin2 A1
#define vccPin2 6

//LED control
#define ledBluePin 10
#define ledGreenPin 12
#define ledRedPin 9

//Test button to immediatelly run pump
#define tl_test 2


#define cerpadloPin 4
// proměnná pro uložení času kontroly
unsigned long cas = 0;

//time when it started to be dry
unsigned long cas_sucha = 0;

//delay before dry detection and pump start (to provide some hystheresis)
unsigned long zpozdeni = 200; //21 600 000;

//time when pump started
unsigned long cas_spusteni = 0;

//max time to run pump at once
unsigned long max_delka_zalevani = 20;

int vlhkost1 = 100;
int vlhkost2 = 100;

int stav = 0;
int test = 0;
//0 - vlhkost OK
//1 - sucho, cekani dle promenne zpozdeni
//2 - zalevam

void setup() {
  pinMode(cerpadloPin, OUTPUT);
  digitalWrite(cerpadloPin, HIGH);
  // zahájení komunikace po sériové lince
  // rychlostí 9600 baud
  Serial.begin(9600);
  // nastavení datových pinů jako vstupů
  // a napájecího pinu jako výstupu
  pinMode(analogPin1, INPUT);
  pinMode(vccPin1, OUTPUT);
  pinMode(analogPin2, INPUT);
  pinMode(vccPin2, OUTPUT);
  // vypnutí napájecího napětí pro modul
  digitalWrite(vccPin1, LOW);
  digitalWrite(vccPin2, LOW);

  pinMode(tl_test, INPUT_PULLUP);

  pinMode(ledGreenPin, OUTPUT);
  digitalWrite(ledGreenPin, LOW);
  pinMode(ledRedPin, OUTPUT);
  digitalWrite(ledRedPin, LOW);
  pinMode(ledBluePin, OUTPUT);
  digitalWrite(ledBluePin, LOW);

  attachInterrupt(0, tlacitko_test, CHANGE);
  
}

void loop() {
  // pokud je rozdíl mezi aktuálním časem a posledním
  // uloženým větší než 10000 ms, proveď měření
  // pokud je čerpadlo aktivní, interval bude jen 500ms
  static int interval_mereni = 5000;
    if (stav == 2)
    {
        interval_mereni = 550;
    }
    else 
    {
        interval_mereni = 10000;
    }
  
  if (millis() - cas > interval_mereni) {
    // zapneme napájecí napětí pro modul s krátkou pauzou
    // pro ustálení
    digitalWrite(vccPin1, HIGH);
    digitalWrite(vccPin2, HIGH);
    delay(200);
    // načtení analogové a digitální hodnoty do proměnných
    int analog1 = analogRead(analogPin1);
    int analog2 = analogRead(analogPin2);

    // výpis analogové hodnoty po sériové lince
    Serial.print("Analogovy vstup1: ");
    Serial.println(analog1);
    Serial.print("Analogovy vstup2: ");
    Serial.println(analog2);

    vlhkost1 = 100 - analog1/10;
    vlhkost2 = 100 - analog2/10;


    String text = "1:";
    text += String(vlhkost1);
    text += "%";
    text += "  2:";
    text += String(vlhkost2);
    text += "%";   
    Serial.println(text);

    mujOled.firstPage();
    do {
      // vykreslení zadané zprávy od zadané pozice
      vykresliText(0, text);
    } while( mujOled.nextPage() );

    // ukončení řádku na sériové lince
    Serial.println();
    // vypnutí napájecího napětí pro modul
    digitalWrite(vccPin1, LOW);
    digitalWrite(vccPin2, LOW);
    // uložení aktuálního času do proměnné pro další kontrolu
    cas = millis();
  }

  if(!test)
  {
    if(stav == 0)
    {
      if ((vlhkost1 < 20) || (vlhkost2 < 20))
      {
        cas_sucha = millis();
        stav = 1;
      }      
    }
    else if (stav == 1)
    {
      if (millis() - cas_sucha > (zpozdeni * 1000))
      {
        startCerpadlo();
      }
      if ((vlhkost1 > 20) && (vlhkost2 > 20))
      {
        stav = 0;
      }
      
    }
    else if (stav == 2)
    {
      if (millis() - cas_spusteni > (max_delka_zalevani * 1000))
      {
        stopCerpadlo();
      }
      if ((vlhkost1 > 60) || (vlhkost2 > 60))
      {
        stopCerpadlo();
      }
    }
  }
      
  //set LED color according to current status
  setLED();
  //wait before few miliseconds
  delay(300);
  //together with previous 200ms for power on sensors, we get 0,5s loop
}

void startCerpadlo()
{
    //change state
    stav = 2;
    //set output pin to LOW (relay module uses inverted logic)
    digitalWrite(cerpadloPin, LOW);
    //write down time when pump started
    cas_spusteni = millis();
}

void stopCerpadlo()
{
    //set output pin to HIGH (relay module uses inverted logic)
    digitalWrite(cerpadloPin, HIGH);
    //change state    
    stav = 0;
}

//Set LED color according to current state
void setLED()
{

  if(stav == 0)
  {
    digitalWrite(ledGreenPin, HIGH);
    digitalWrite(ledRedPin, LOW);
    digitalWrite(ledBluePin, LOW);
  }
  
  if(stav == 1)
  {
    digitalWrite(ledGreenPin, LOW);
    digitalWrite(ledRedPin, HIGH);
    digitalWrite(ledBluePin, LOW);
  }

  if(stav == 2)
  {
    digitalWrite(ledGreenPin, LOW);
    digitalWrite(ledRedPin, LOW);
    digitalWrite(ledBluePin, HIGH);
  }

}


void tlacitko_test()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200)
  {
      //start pump immediatelly. If already started, stop it
      if (stav == 2)
      {
          stopCerpadlo();
          test = 0;
      }
      else {
          startCerpadlo();
          test = 1;
      }
      setLED();
  }
  last_interrupt_time = interrupt_time;
}

// funkce vykresliText pro výpis textu na OLED od zadané pozice
void vykresliText(int posun, String text) {
  // nastavení písma, další písma zde:
  // https://github.com/olikraus/u8glib/wiki/fontsize
  mujOled.setFont(u8g_font_10x20);
  // nastavení výpisu od souřadnic x=0, y=25; y záleží na velikosti písma
  mujOled.setPrintPos(0, 25);
  // uložení části zprávy - od znaku posun uložíme 15 znaků
  // např. na začátku uložíme znaky 0 až 15
  String vypis;
  vypis = text.substring(posun, posun+15);
  // výpis uložené části zprávy na OLED displej
  mujOled.print(vypis);
}
