import processing.video.*;
import processing.serial.*;


// --- Core Variables ---
Serial myPort; 
int heartRate = 0;
int currentIBI = 0; 
//float sdnn = 0.0; //emi
float rmssd = 0.0; //emi
float hrv = 0.0;
String tab = "HR";
String plotTab = "WAVE";
float lastUpdate = 0;

Movie heartMovie;
float playbackSpeed = 1.0; // prędkość odtwarzania wideo
float videoTime = 0;       // czas odtwarzania wideo w sekundach

// --- Data Logic ---
//IntList ibiHistory = new IntList(); 
//String lastStatus = "Brak_odczytu";
//int signalQuality = 0;
//boolean sensorConnected = false;

// --- Beat Indicator ---
//boolean beatFlash = false;
//float beatFlashTime = 0;

// --- Animation & Plotting Arrays ---
float heartSize = 380;
int[] waveValues = new int[500];
float[] ibiValues = new float[500];
float[] hrvValues = new float[500];



void setup() {
  size(1400, 800);
  
  myPort = new Serial(this, "COM4", 115200);
  myPort.bufferUntil('\n');
  
  //Animacja serca
  heartMovie = new Movie(this, "heart_bg.mp4");
  heartMovie.play();
  lastUpdate = millis() / 1000.0;
  updateSpeed();

  
  // Initialize arrays to mid-points
  for (int i = 0; i < 500; i++) {
    waveValues[i] = 512;
    ibiValues[i] = 800; // Typical 75 BPM starting point
    hrvValues[i] = 0;
  }
  
  textFont(createFont("Arial", 32));
}

void draw() {
  background(40, 0, 0);  
  

  drawTabs();
  drawPlotSection();
  drawRightPanel();
  //drawSignalQuality();
  //drawBeatIndicator();
  
  }



// --- Dynamic GUI Elements ---

// Wywoływane automatycznie, gdy dostępna nowa klatka 
void movieEvent(Movie m) {
  m.read();
}

// Aktualizacja prędkości odtwarzania
void updateSpeed() {
  float videoDuration = heartMovie.duration();
  if (videoDuration > 0 && heartRate > 0) {
    playbackSpeed = videoDuration / (60.0 / heartRate);
  } else {
    playbackSpeed = 1.0;
  }
}

void drawPlotSection() {
  float x = width/2 - 550;
  float y = 440;

  // --- PRZYCISKI NAD WYKRESEM ---
  drawButton(x, y - 60, "WAVE", plotTab.equals("WAVE"));
  drawButton(x + 100, y - 60, "IBI", plotTab.equals("IBI"));
  drawButton(x + 200, y - 60, "HRV", plotTab.equals("HRV"));

  // --- WYBRANY WYKRES ---
  if (plotTab.equals("WAVE")) {
    drawPlot(x, y, waveValues, 200, 850, color(0,255,150), "PULSE WAVEFORM");
  } 
  else if (plotTab.equals("IBI")) {
    drawPlot(x, y, ibiValues, 400, 1200, color(255,200,0), "IBI RHYTHM (ms)");
  } 
  else if (plotTab.equals("HRV")) {
    drawPlot(x, y, hrvValues, 0, 150, color(200,100,255), "HRV STABILITY (SDNN)");
  }

  // --- TEKST POD WYKRESEM ---
  drawMessageUnderPlot(x, y + 180);
}

void drawMessageUnderPlot(float x, float y) {
  fill(255);
  textSize(32);

  String msg = tab.equals("HR") 
    ? getHeartMessage(heartRate) 
    : getHRVMessage(hrv);

  text(msg, x, y, 500, 120);
}


void drawTabs() {
  float boxX = width/2 - 550;
  float boxY = 120;      // lekko niżej, bo nad nim będą guziki
  float boxW = 500;
  float boxH = 220;

  // --- PRZYCISKI NAD BOXEM ---
  drawButton(boxX, boxY - 60, "HR", tab.equals("HR"));
  drawButton(boxX + 100, boxY - 60, "HRV", tab.equals("HRV"));

  // --- BOX ---
  stroke(255);
  strokeWeight(4);
  noFill();
  rect(boxX, boxY, boxW, boxH, 12);

  fill(255);
  if (tab.equals("HR")) {
    textSize(28); text("HEART RATE:", boxX + 30, boxY + 50);
    textSize(120); text(heartRate, boxX + 30, boxY + 170);
  } else {
    textSize(28); text("HRV (Current):", boxX + 30, boxY + 50);
    textSize(90); text(nf(hrv, 0, 1), boxX + 30, boxY + 170);
  }
}


void drawButton(float x, float y, String label, boolean active) {
  pushStyle();

  stroke(255);          // zawsze biała ramka
  strokeWeight(2);
  fill(active ? color(255, 120, 120) : 255);

  rect(x, y, 80, 40, 8);

  fill(0);
  noStroke();
  textSize(22);
  textAlign(CENTER, CENTER);
  text(label, x + 40, y + 20);
  textAlign(LEFT, BASELINE);

  popStyle();
}


void drawRightPanel() {
  float rightX = width/2 + 80;

  float videoW = heartSize;
  float videoH = heartSize * 1.2;

  // --- WYSOKOŚCI ---
  float titleY = 100;                 // napis "Podgląd serca"
  float messageY = 620;               // poziom wiadomości pod wykresem
  float barY = messageY - 10;              // bar jakości na tej samej wysokości
  float videoBottom = barY + 30;      // mały odstęp nad barem
  float videoY = videoBottom - videoH;

  // --- TYTUŁ WYŚRODKOWANY ---
  fill(255);
  textSize(34);
  textAlign(CENTER, BASELINE);
  text("PODGLĄD SERCA:", rightX + videoW/2, titleY);
  textAlign(LEFT, BASELINE);

  // --- ANIMACJA WIDEO ---
  float currentTime = millis() / 1000.0;
  float delta = currentTime - lastUpdate;

  videoTime += delta * playbackSpeed;
  if (heartMovie.duration() > 0) videoTime %= heartMovie.duration();

  heartMovie.jump(videoTime);
  lastUpdate = currentTime;

  image(heartMovie, rightX, videoY, videoW, videoH);


}



//void drawSignalQuality() {
//  float rightX = width/2 + 150;
//  float barY = 670; 
//  fill(255);
//  textSize(20);
//  text("Jakość sygnału:", rightX, barY - 10);
  
//  noFill(); stroke(255); strokeWeight(2);
//  rect(rightX, barY, 300, 25, 5);
  
//  noStroke();
//  if(signalQuality > 70) fill(100, 255, 100);
//  else if(signalQuality > 40) fill(255, 200, 100);
//  else fill(255, 100, 100);
  
//  float barWidth = map(signalQuality, 0, 100, 0, 300);
//  rect(rightX, barY, barWidth, 25, 5);
//}


// --- Universal Plotting Engine ---

void drawPlot(float x, float y, float[] array, float minVal, float maxVal, color c, String label) {
  pushMatrix();
  translate(x, y);
  
  // Find the actual min/max in the data for auto-scaling the Waveform specifically
  float actualMin = minVal;
  float actualMax = maxVal;
  
  if (label.equals("PULSE WAVEFORM")) {
    actualMin = 1023; 
    actualMax = 0;
    for (float v : array) {
      if (v < actualMin) actualMin = v;
      if (v > actualMax) actualMax = v;
    }
    // Add a little padding so it's not touching the edges
    actualMin -= 10;
    actualMax += 10;
  }

  fill(25, 5, 5); stroke(100, 0, 0);
  strokeWeight(4);
  rect(0, 0, 500, 150, 8); 
  fill(180); textSize(14); text(label, 5, -5);
  
  noFill(); stroke(c); strokeWeight(2);
  beginShape();
  for (int i = 0; i < array.length; i++) {
    float py = map(array[i], actualMin, actualMax, 140, 10);
    vertex(i, constrain(py, 10, 140));
  }
  endShape();
  popMatrix();
}

void drawPlot(float x, float y, int[] array, float minVal, float maxVal, color c, String label) {
  float[] f = new float[array.length];
  for(int i=0; i<array.length; i++) f[i] = array[i];
  drawPlot(x, y, f, minVal, maxVal, c, label);
}

// --- Data Handling ---

void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString == null) return;

  String[] data = split(trim(inString), ',');
  if (data.length != 5) return;

  try {
    // 1. BPM z FFT
    heartRate = int(data[0]);
    updateSpeed();

    // 2. Wygładzony sygnał PPG
    int ppg = int(data[1]);
    shiftArray(waveValues, ppg);

    // 3. IBI
    int newIBI = int(data[2]);
    if (newIBI > 300 && newIBI < 2000 && newIBI != currentIBI) {
      currentIBI = newIBI;
      shiftArray(ibiValues, (float)newIBI);
    }

    // 4. HRV
    hrv = float(data[3]);     // SDNN
    rmssd = float(data[4]);  // RMSSD
    shiftArray(hrvValues, hrv);

  } catch (Exception e) {
    println("Parse error");
  }
}


void shiftArray(int[] arr, int val) {
  System.arraycopy(arr, 1, arr, 0, arr.length - 1);
  arr[arr.length-1] = val;
}

void shiftArray(float[] arr, float val) {
  System.arraycopy(arr, 1, arr, 0, arr.length - 1);
  arr[arr.length-1] = val;
}

//float calculateSDNN(IntList list) {
//  if (list.size() < 2) return 0;
//  float avg = 0;
//  for (int i : list) avg += i;
//  avg /= list.size();
//  float dSum = 0;
//  for (int i : list) dSum += pow(i - avg, 2);
//  return sqrt(dSum / list.size());
//}

void drawMessage() {
  fill(255); textSize(24);
  String msg = (tab.equals("HR") ? getHeartMessage(heartRate) : getHRVMessage(hrv));
  text(msg, width/2 - 520, height - 420, 500, 150);
}

String getStatusMessage(String s) {
  if (s.equals("BRAK_PALCA")) return "Przyłóż palec do sensora!";
  if (s.equals("STABILIZACJA")) return "Stabilizacja... Trzymaj nieruchomo.";
  return "Czekam na dane...";
}

String getHeartMessage(int hr) {
  if (hr < 55) return "Niskie tętno (Bradykardia).";
  if (hr <= 100) return "Tętno w normie spoczynkowej.";
  return "Podwyższone tętno (Stres/Wysiłek).";
}

String getHRVMessage(float h) {
  if (h < 30) return "Niskie HRV: Organizm pod stresem.";
  if (h < 70) return "HRV w normie: Dobra równowaga.";
  return "Wysokie HRV: Świetna regeneracja!";
}

void mousePressed() {
  float boxX = width/2 - 550;
  float boxY = 120;

  // HR / HRV
  if (mouseY > boxY - 60 && mouseY < boxY - 20) {
    if (mouseX > boxX && mouseX < boxX + 80) tab = "HR";
    if (mouseX > boxX + 100 && mouseX < boxX + 180) tab = "HRV";
  }

  // Plot buttons
  float plotX = width/2 - 550;
float plotY = 440 - 60;  // dokładnie y przycisków

if (mouseY > plotY && mouseY < plotY + 40) { // 40 = wysokość przycisku
    if (mouseX > plotX && mouseX < plotX + 80) plotTab = "WAVE";
    if (mouseX > plotX + 100 && mouseX < plotX + 180) plotTab = "IBI";
    if (mouseX > plotX + 200 && mouseX < plotX + 280) plotTab = "HRV";
}

  }
