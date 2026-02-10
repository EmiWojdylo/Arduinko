//1. czas próbkowania
//2. filtrowanie i zakres
//3. bufor fft
//4. detekcja szczytów
//5. obliczanie IBI 
//6. FFT do oszacowania BPM
//7. przesył danych
//8. obliczenie HRV (SDNN, RMSSD)

// metryki HRV:
// RMSSD - root mean square of successive differences, pomiar w krótkiej skali
// SDNN - standard deviation of normal-to normal Intervals

#include <arduinoFFT.h> // biblioteka do FFT

// konfiguracja
const int SENSOR_PIN = A0;
const int SAMPLE_RATE = 100; // 100 próbek na sekundę
const int FFT_SIZE = 128; // FFT liczone ze 128 próbek
const int IBI_BUFFER_SIZE = 10; // bufor 10 ostatnich odstępów midzy uderzeniami, do obliczania HRV

// czas próbkowania
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 10; // 10ms - 100Hz

// przetwarzanie sygnału
float smoothedSignal = 0; // sygnał po filtrowaniu
float dcOffset = 512; // środek ADC
const float alpha = 0.15; // filtr do usuwania AC
const float dcAlpha = 0.001; // filtr do usuwania DC, baseline drift

// zakres sygnału, zmienny
float signalMax = -1000; // wartości niemożliwe do osiągnięcia przez przefiltrowany sygnał, gwarancja że się zaaktualizują
float signalMin = 1000;

// FFT
double vReal[FFT_SIZE]; // sygnał
double vImag[FFT_SIZE]; // część urojona
uint8_t bufferIndex = 0; // wskaźnik aktualnej próbki FFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SIZE, SAMPLE_RATE); //obiekt FFT

// BPM
float smoothedBPM = 0;

// detekcja uderzeń
float lastSignal = 0;
float peakValue = -1000;
bool lookingForPeak = false;
unsigned long lastBeatTime = 0;
uint16_t currentIBI = 0;
uint16_t lastValidIBI = 0;

// bufor IBI
uint16_t ibiBuffer[IBI_BUFFER_SIZE];
uint8_t ibiIndex = 0;
uint8_t ibiCount = 0;

//HR szybkie
float currentHR = 0;
float smoothedHR = 0;


// HRV
uint16_t sdnn = 0;
uint16_t rmssd = 0;

// debug
unsigned long lastDebugTime = 0;
uint16_t beatCount = 0;
bool debugMode = false;  



void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  
  // inicjalizacja tablic pod FFT
  for (uint8_t i = 0; i < FFT_SIZE; i++) {
    vReal[i] = 0;
    vImag[i] = 0;
  }
  
  for (uint8_t i = 0; i < IBI_BUFFER_SIZE; i++) {
    ibiBuffer[i] = 0;
  }
  
  //Serial.println("=== DEBUG MODE ===");
  delay(3000);
}

void loop() {

// ---------------------1.Czas próbkowania
  // obliczanie czasu próbkowania -> jeśli obecny czas - czas od ostatniej próbki jest równy lub większy niż 
  // 10ms, naliczamy czas od nowa
  unsigned long currentTime = millis();
  if (currentTime - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentTime;
    
    int rawSignal = analogRead(SENSOR_PIN);
    
// ---------------------2. Filtr i adaptacyjny zakres 
    dcOffset = (dcAlpha * rawSignal) + ((1.0 - dcAlpha) * dcOffset); // filtr exponential moving average
    //oblicza 0.1% sygnału na wejściu, sumuje z 99.9% sygnału z poprzedniej iteracji; usuwa toniczne zmiany
    float acSignal = rawSignal - dcOffset; // odejmujemy baseline tak, żeby została tylko oscylująca część sygnału
    smoothedSignal = (alpha * acSignal) + ((1.0 - alpha) * smoothedSignal); //większa alfa, filtr górnoprzepustowy
    //15% sygnału na wejściu + 85% sygnału z poprzedniej iteracji, usuwa drżenie, wysokoczęstotliwościowy szum 
    
    // adaptacyjny zakres sygnału - śledzenie maksimum i minimum 
    if (smoothedSignal > signalMax) signalMax = smoothedSignal;
    if (smoothedSignal < signalMin) signalMin = smoothedSignal;
    signalMax *= 0.9995; // dryft - auto-kalibracja, powolne "zapominanie" starych max, żeby zapobiec pozostawianiu starych wartości max min
    signalMin *= 0.9995;
    

// ---------------------3. Bufor FFT
    vReal[bufferIndex] = smoothedSignal; // zapisujemy przefiltrowany sygnał do bufora FFT
    bufferIndex++; // gdy bufferIndec >= 128, pełne okno do analizy
    
// ---------------------4. Detekcja szczytów
    float signalRange = signalMax - signalMin; //zakres sygnału
    float threshold = signalMin + (signalRange * 0.4); // treshold równy 40% powyżej minimum w aktualnym zakresie sygnału
    

    // wykrywanie rosnącej krawędzi
    if (smoothedSignal > threshold && lastSignal <= threshold && !lookingForPeak) { //gdy sygnał przekracza próg od dołu, szukamy górki
      lookingForPeak = true;
      peakValue = smoothedSignal; // inicjacja peakValue obecną wartością sygnału
    }
    
    //szukamy maksimum, jeśli sygnał rośnie, zapisujemy wyższą wartość i aktualizujemy szczyt
    if (lookingForPeak) {
      if (smoothedSignal > peakValue) {
        peakValue = smoothedSignal;
      }
      
      //jeżeli sygnał spadł o 10% od maksimum, to był szczyt 
      if (smoothedSignal < peakValue * 0.9) {
        lookingForPeak = false;

// ------------ 5. Obliczanie IBI        
        unsigned long now = millis();
        if (lastBeatTime > 0 && (now - lastBeatTime) > 300) { // większe niż 300ms bo tu zaczynają się sensowne wartości dla IBI
          currentIBI = now - lastBeatTime;
          
          //jeśli odstęp między ibi sensowny, aktualizujemy
          if (currentIBI >= 300 && currentIBI <= 2000) { // znowu, 300-2000ms to sensowny zakres dla IBI, 30-200 bpm
            lastValidIBI = currentIBI;
            beatCount++;

            float hr = 60000.0 / currentIBI;   // HR z IBI
              currentHR = hr;

   // lekkie wygładzenie (EMA)
              if (smoothedHR == 0) {
                smoothedHR = hr;
              } else {
                smoothedHR = 0.3 * hr + 0.7 * smoothedHR;
              }

            
            ibiBuffer[ibiIndex] = currentIBI; //nowa wartość do tablici 
            ibiIndex = (ibiIndex + 1) % IBI_BUFFER_SIZE; // modulo dla nadpisywania kolejnych wartośći w tablicy
            if (ibiCount < IBI_BUFFER_SIZE) ibiCount++; // ile wartości jest w buforze
            
            if (ibiCount >= 5) { // jeżeli naliczyło się przynajmniej 5 sensownych wartości IBI, liczymy HRV
              calculateHRV();
            }
            
            //Detekcja uderzen w debugu, aktualnie nieużywane
            if (debugMode) {
              Serial.print(">>> BEAT! IBI=");
              Serial.print(aktualne IBI);
              Serial.print("ms (");
              Serial.print(60000.0 / currentIBI);
              Serial.println(" BPM)");
            }
          }
        }
        lastBeatTime = now;
        peakValue = -1000;
      }
    }
    
    lastSignal = smoothedSignal;
    
// ---------------- 6. FFT
    if (bufferIndex >= FFT_SIZE) { // gdy bufor pełyny wykonujemy FFT
      bufferIndex = 0; // reset indeksu, nowe okno czasowe
      
      FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward); //przygotowanie danych, okno Hamminga robi... coś?
      FFT.compute(FFTDirection::Forward); // zmiana sygnałuu z domeny czasu na domenę częstotliwości
      FFT.complexToMagnitude(); // obliczanie amplitudy każdej częstotliwośći
      
      // zakres interesujących częstotliwości 0.5 - 3.5 Hz (30-200bpm)
      uint8_t minBin = max(1, (int)(0.5 * FFT_SIZE / SAMPLE_RATE));
      uint8_t maxBin = min(FFT_SIZE/2, (int)(3.5 * FFT_SIZE / SAMPLE_RATE));
      
      //szukamy najsilniejszego piku
      double maxMagnitude = 0;
      uint8_t peakBin = minBin;
      
      for (uint8_t i = minBin; i <= maxBin; i++) {
        if (vReal[i] > maxMagnitude) {
          maxMagnitude = vReal[i];
          peakBin = i;
        }
      }
      
      if (maxMagnitude > 5) { //próg amplitudy, udrzuecnie szumu
        float frequency = (peakBin * 1.0 * SAMPLE_RATE) / FFT_SIZE; // zmiana z bin na Hz na BPM
        float currentBPM = frequency * 60.0;
        
        if (currentBPM >= 40 && currentBPM <= 180) {
          if (smoothedBPM == 0) {
            smoothedBPM = currentBPM;
          } else {
            smoothedBPM = (0.3 * currentBPM) + (0.7 * smoothedBPM); // wygładzanie bpm, exponential moving average raz jeszcze
          }
        }
      }
    }
  }
  
  // --------------- debug co 2s
  if (debugMode && currentTime - lastDebugTime > 2000) {
    lastDebugTime = currentTime;
    
    Serial.println("===================================");
    Serial.print("Signal Range: ");
    Serial.print(signalMax - signalMin);
    Serial.print(" (Min: ");
    Serial.print(signalMin);
    Serial.print(", Max: ");
    Serial.print(signalMax);
    Serial.println(")");
    
    Serial.print("Current Signal: ");
    Serial.println(smoothedSignal);
    
    Serial.print("Threshold: ");
    Serial.println(signalMin + ((signalMax - signalMin) * 0.4));
    
    Serial.print("FFT BPM: ");
    Serial.println(smoothedBPM);
    
    Serial.print("Total Beats Detected: ");
    Serial.println(beatCount);
    
    Serial.print("Last IBI: ");
    Serial.print(lastValidIBI);
    Serial.println("ms");
    
    Serial.print("HRV - SDNN: ");
    Serial.print(sdnn);
    Serial.print(", RMSSD: ");
    Serial.println(rmssd);
    Serial.println("===================================");
  }
  
  //-------------- 7. Output 
  if (!debugMode) {
    //Serial.print((int)smoothedBPM);
    Serial.print((int)smooothedHR);
    Serial.print(",");
    Serial.print((int)(smoothedSignal + 100)); // przesunięcie sygnału w górę, żeby nie był ujemny
    Serial.print(",");
    Serial.print(lastValidIBI);
    Serial.print(",");
    Serial.print(sdnn);
    Serial.print(",");
    Serial.println(rmssd);
  }
  
  delay(5);
}

//-------------------- 8. HRV, SDNN, RMSSD
void calculateHRV() {
  uint32_t sumIBI = 0; // inicjalizacja sumy do obliczania średniej
  for (uint8_t i = 0; i < ibiCount; i++) {
    sumIBI += ibiBuffer[i];
  }
  float meanIBI = (float)sumIBI / ibiCount;  //Średnia ibi
   
  float sumSquaredDiff = 0; //inicjalizacja SSD 
  for (uint8_t i = 0; i < ibiCount; i++) { 
    float diff = ibiBuffer[i] - meanIBI;
    sumSquaredDiff += diff * diff;
  }
  sdnn = (uint16_t)sqrt(sumSquaredDiff / ibiCount); // obliczanie Standard Deviation of Normal to Normal Intervals
  
  if (ibiCount > 1) {
    float sumSuccessiveDiff = 0; //inicjalizacja Rot Mean Square of Succesive Difference
    
    for (uint8_t i = 1; i < ibiCount; i++) {
      int16_t diff = (int16_t)ibiBuffer[i] - (int16_t)ibiBuffer[i-1];
      sumSuccessiveDiff += (float)(diff * diff);
    }
    
    rmssd = (uint16_t)sqrt(sumSuccessiveDiff / (ibiCount - 1));
  }
}