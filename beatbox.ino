
// The number of instruments to manage. 
// Changing this number also requires updates
// to the pin settings for buttons, lights, and bells.
const int INSTRUMENTS = 1;
// The maximum length of a song in ticks
const long MAX_TICKS = 1024;

// Used to pulse the beat,
const int TICKS_PER_BEAT = 8;

// How long in ms to ring the bell
const int BELL_TIME = 50;

const int RECORDING_LOOP_DELAY = 10;
const int MIN_TICK_DELAY = 100;
const int MAX_TICK_DELAY = 1000;

// Required duration of a button press for recording
const float STATE_CHANGE_DEBOUNCE = 00;

// Pin assigments
const int BUTTONS[INSTRUMENTS] = {2};
const int LIGHTS[INSTRUMENTS] = {6};
const int BELLS[INSTRUMENTS] = {10};

const int KNOCKER = A4;
const int KNOCKGROUND = A5;
const int SPEEDO = A1;

// Adjust this based on the sensitivity of the piezo sensor, Higher numbers decreases sensitivity
int knockThreshold = 300;

// The delay between ticks
long tickDelay = 200;
// This can change based on recording. Start
// with a one-beat song
long songLength = TICKS_PER_BEAT;

// When a button was first pressed.
long buttonPressStart = -1;
// The instrument index for a button press
int buttonPressed = -1;
// When a stateChange last began
long stateChangeTime = -1;

// The minimum time a playback or recording can occur before switching states
const int MIN_STATE_CHANGE_TIME = 5000;

// Stores whether an instrument is on or off at a particular tick
boolean song[INSTRUMENTS][MAX_TICKS]; 


void setup() {
  Serial.begin(9600);
  Serial.println("Starting beatbox");
  for (int i=0; i<INSTRUMENTS; i++) {
    pinMode(LIGHTS[i], OUTPUT);
    pinMode(BELLS[i], OUTPUT);
    pinMode(BUTTONS[i], INPUT);
    blink(LIGHTS[i], 3);
  }
  pinMode(KNOCKGROUND, OUTPUT);
  digitalWrite(KNOCKGROUND, LOW);
}

void loop() {
//  isKnocking();
  playback();
}


boolean isKnocking() {
  int samples = 100; // readings
  int high = 0;
  int low = 1023;
  int range = 0;
  
  for (int i=0; i<samples; i++) {
    int reading = analogRead(KNOCKER);
    if (reading > high) {
      high = reading;
    }
    if (reading < low) {
      low = reading;
    }
  }

  range = high - low;
  Serial.println(range);
  
  delay(1);
  return range > knockThreshold;
}

boolean stateChangeTriggered(int index) {
  if (stateChangeTime > 0 && millis() - stateChangeTime < MIN_STATE_CHANGE_TIME ) {
    return false;
  }
  
  if (buttonPressed >= 0 && buttonPressed != index) {
    // This isn't the button we're looking for
    return false; 
  }
  
  boolean down = digitalRead(BUTTONS[index]) == LOW;
  if (buttonPressed < 0 && down) {
    // The button has been pressed. Start tracking
    buttonPressed = index;
    buttonPressStart = millis();
    Serial.print("Tracking button ");
    Serial.println(index);
    return false;
  }
  
  // From this point on, we're looking at a button that's being tracked.
  if (buttonPressed == index) {
    long diff = millis() - buttonPressStart;
    if (diff > STATE_CHANGE_DEBOUNCE) {
      buttonPressed = -1;
      buttonPressStart = -1;
      stateChangeTime = millis();
      return true;
    }
    if (!down) {
      buttonPressed = -1;
      buttonPressStart = -1;
      return false;
    }
  }
  
  // Otherwise, keep tracking the current button.
  return false;  
}

void playback() {
  for (int tick=0; tick<songLength; tick++) {
    // read any adjustment to the speed of the playback.
    int dial = analogRead(SPEEDO);
    tickDelay = map(dial, 0, 1023, MIN_TICK_DELAY, MAX_TICK_DELAY);
    Serial.print("Tick: ");
    Serial.print(tick);
    Serial.print("; Dial: ");
    Serial.print(dial);
    Serial.print("; Delay: ");
    Serial.println(tickDelay);

    boolean rung = false;
    for (int index=0; index<INSTRUMENTS; index++) {
      if (stateChangeTriggered(index)) {
        record(index);
        return;
      }
      
      boolean ring = song[index][tick];
      if (ring) {
        rung = true;
      }
      
      digitalWrite(BELLS[index], ring);
      digitalWrite(LIGHTS[index], (tick % 2 == 0));
    }
    if (rung) {
      delay(BELL_TIME);
      for (int index=0; index<INSTRUMENTS; index++) {
        digitalWrite(BELLS[index], LOW);
      }
      // Account for the time it takes to ring the bell in the delay
      delay(tickDelay - BELL_TIME);
    }
    else {
      delay(tickDelay);
    }
  }
}


void blink(int pin, int times) {
   for (int i=0; i<times; i++) {
     digitalWrite(pin, LOW);
     delay(150);
     digitalWrite(pin, HIGH); 
     delay(250);
   }
}


void record(int index) {
  
  Serial.print("Starting recording for instrument ");
  Serial.println(index);
  long start = millis();
  long elapsed = 0;
  long maxTime = tickDelay * MAX_TICKS;
  int light = LIGHTS[index];
  int button = BUTTONS[index];
  boolean recording = true;
  boolean knocked = false;

  // reset the song
  for (int i=0; i<MAX_TICKS; i++) {
    song[index][i] = false; 
  }
  Serial.print("Listening until ");
  Serial.print(maxTime);
  Serial.println("ms elapse");
   
  // Blink to indicate recording
  blink(light, 5);
  
  // record the new song
  digitalWrite(light, HIGH);
  while (recording && elapsed < maxTime) {
    knocked = isKnocking();
    elapsed = millis() - start;
    
    if (knocked) {
      // map the time passed to the appropriate tick
      long k = map(elapsed, 0, maxTime - 1, 0, MAX_TICKS - 1);
      song[index][k] = true;
      if (k > songLength) {
        int adjustment = k % TICKS_PER_BEAT;
        songLength = k + adjustment;
        Serial.print("Song length bumped up to ");
        Serial.println(songLength);
      }
      digitalWrite(light, LOW);
      delay(RECORDING_LOOP_DELAY);
    } else {
      digitalWrite(light, HIGH);
    }
    
    if (stateChangeTriggered(index)) {
      Serial.println("Button triggered end of recording");
      recording = false; 
    }
  }
  
  Serial.println("Done recording");
  
  // Blink thrice to indicate finishing
  blink(light, 5);
  
  playback();
}


