//1. odczyt sygnału "PPG" z czujnika KY-039
//2. wygładzanie filtrem EMA
//3. wykrycie uderzeń serca
//4. obliczanie IBI
//5. ocena jakości sygnału
//6. przesył danych 

#include <PulseHeartLab.h>  //biblioteka ułatwiająca przetwarzanie

PulseHeartLab sensor(A0);


//używane w 2.
float smoothedY = 0; //zmienna przechowująca wygładzoną wartość sygnału
float alpha = 0.10; // współczynnik filtru Exponential Moving Average, im mniejsze alpha tym silniejsze wygładzanie


//zmienne do liczenia IBI, używane w 4.
volatile uint32_t lastBeatTime = 0;
volatile uint16_t currentIBI = 0;
volatile bool newBeatDetected = false; //flaga do poprawy błędu :)

//funkcja wywołana za każdym razem, gdy wykryje uderzenie (lib)
void beatCb(uint16_t bpm) {
  uint32_t now = millis(); //pobranie aktualnego czasu od startu programuu
  
  if (lastBeatTime != 0){
    currentIBI = now - lastBeatTime; // obliczanie czasu między aktualnym a poprzednim uderzeniem, IBI
  }
 
  lastBeatTime = now; // zapisanie czasu aktualnego uderzenia jako referencji dla następnego
  newBeatDetected = true; // nowe uderzenie

  Serial.println("meep");
}

void setup() {
  Serial.begin(115200);
  
  sensor.onBeat(beatCb); // rejestracja funkcji beatCB jako callback przy wykryciu uderzenia
  sensor.setNotch(50); // ustawienie filrtu Notch na 50 Hz, eliminacja szumu sieci elektrycznej
  sensor.begin(true, true, 100);
  sensor.setRefractory(100);
  sensor.setThresholdAuto(true);
  Serial.print("Baseline ADC: ");
  Serial.println(sensor.getBaseline());
  delay(1500); //czas na kalibrację
  sensor.start();
  
}

void loop() {
  // ----------- 1. --------------
  int raw = sensor.readSignal(); // pobieranie   próbki sygnału
  // if (raw == -1) Serial.println(raw); // przerywa pętle jeśli brak próbki

// ----------- 2. --------------
  smoothedY = (alpha * raw) + ((1.0 - alpha) * smoothedY); // wygładzanie wykresu, alpha * próbka + (1-alpha) * poprzednia wartosc
  //redukuje szum wysokoczęstotliwościowy




// ----------- 5. --------------
  int sqi = sensor.getSQI(); // pobieranie Signal Quality Index, im wyższa wartość tym lepszy sygnał
  String status = "OK"; // domyślny status
  if (sqi < 15) status = "BRAK_PALCA"; // słaby sygnał
  else if (sqi < 50) status = "STABILIZACJA"; // średni sygnał

  uint16_t ibiToSend = 0;
  if (newBeatDetected) {
    ibiToSend = currentIBI; // nadpisanie nową
    newBeatDetected = false; //reset flagi
  }
 

// ----------- 6. --------------
  // format wysyłania danych: BPM, Signal, IBI, Quality, Status
  Serial.print(sensor.getBPM());
  Serial.print(",");
  Serial.print((int)smoothedY); 
  Serial.print(",");
  Serial.print(ibiToSend);
  Serial.print(",");
  Serial.print(sqi);
  Serial.print(",");
  Serial.println(status);

  delay(5);
}