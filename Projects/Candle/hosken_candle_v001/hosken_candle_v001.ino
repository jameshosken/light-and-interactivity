/*
  NeoPixel Candle using ColorConverter

  Original Sketch:
  created 28 Jun 2018
  by Tom Igoe

  Modified one 10 Febv 2019
  by James  Hosken
*/


// State machine:
// 0: Off, listening for light
// 1: Lighting up
// 2: On, listening for wind,
// 3: Dying (back to off)
int state = 0;

#include <Adafruit_NeoPixel.h>
#include <ColorConverter.h>

const int neoPixelPin = 6;
const int pixelCount = 7;    // number of pixels



// set up strip:
Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, neoPixelPin, NEO_GRBW + NEO_KHZ800);
ColorConverter converter;

int hsi_values[7][3]={
 {15,100,0},
 {11,100,},
 {13,100,0},
 {5,100,0},
 {14,100,0},
 {13,100,0},
 {7,100,0}
};
//225-245 blue

int hue_increments[7] = {
  1,1,1,1,1,1,1
};

int hue_ranges[7][2] = {
  {5,15},
  {2,15},
  {3,15},
  {4,15},
  {2,15},
  {3,15},
  {4,15}
};

// Set increment frequency per pixel
int frequencies[7] = {
  10,3,4,5,3,4,5
};

//TURNING ON
int turnOnTime = 1000; //how many millseconds to turn on)
int turnOnTimeCheckpoint = 0;
int onTimeDelay = 20; //Delay per increment turning on
int intensity_increments[7] = {    //Increments to add to each pix while turning on
  5,3,2,1,2,1,3
};

//Intensity Adjustments during on state
int max_intensity_offset = 0;     // Keeps track of maximum wind value received
float intensity_offset = 0;       // Subtract this from overall intensity


// Adjust overall frequencies (based on noise/wind
int frequency_multiplier = 10; 

//Keep track of the time of each pixel's previous statechange
long checkpoints[7] = {
  0,0,0,0,0,0,0
};

void setup() {

  pinMode(A0, INPUT);
  strip.begin();    // initialize pixel strip
  strip.clear();    // turn all LEDs off
  strip.show();     // update strip

  Serial.begin(9600);
}

//Resets board after end of dying state. It's better this way.
void(* resetFunc) (void) = 0;//declare reset function at address 0

void loop() {

  if(state == 0){ //Off, listening for light
    handleOffState();
  }else if(state == 1){ // Turning on
     handleTurningOnState();
  }else if(state == 2){ // On, listening for wind
     handleOnState();
  }else{ // Dying
    handleDyingState();
  }
}


void handleOffState(){
  //Make sure light is off
  strip.clear();
  strip.show();

  if(analogRead(A1) < 40){ //TODO: Circular buffer, take a while to 'light'
    Serial.println("********* STATE = 1 **************");
    turnOnTimeCheckpoint = millis(); //Flag checkpoint for turn on state
    state = 1;
  }
  delay(1);
}

void handleTurningOnState(){
  if(millis() - turnOnTimeCheckpoint > turnOnTime){
    Serial.println("######## STATE = 2 #############");
    state = 2;
  }

  
  for(int i =0; i < 7; i++){
    incrementIntensity(i);
    incrementHue(i);
  }
  
  strip.show();
  delay(onTimeDelay);
}

void handleOnState(){

  //Increment hue at different times for each pixel
  long timestamp = millis();
  for(int i = 7-1; i >= 0; i--){
    if(timestamp - checkpoints[i] > frequencies[i] * frequency_multiplier ){
      incrementHue(i);
      checkpoints[i] = timestamp;
    }
  }
  
  intensity_map();
  intensity_return();

  //Change state to dying if wind above certain threshhold
  if(intensity_offset > 80){ //TODO remove magic number, create threshold variable
    Serial.println("+++++++++++++STATE = 3 +++++++++++++++++");
    turnOnTimeCheckpoint = millis();
    for(int i = 7-1; i >= 0; i--){
      checkpoints[i] = millis();
    }
    state = 3;
  }
  strip.show();   // update the strip
  delay(1);
}

void handleDyingState(){
  
  if(millis() - turnOnTime*5 > turnOnTimeCheckpoint){
    Serial.println("================STATE = RESET ===========");
    resetFunc();
  }
  
  //Repurpose timestamp/checkpoints code but with increment values as frequencies
  //Basically means middle pixel  (on jewel) goes down slowest.
  
  long timestamp = millis();
  for(int i = 7-1; i >= 0; i--){
    if(timestamp - checkpoints[i] > sq(intensity_increments[i])* 30 ){
      decrementIntensity(i);
      checkpoints[i] = timestamp;
    }
  }

  strip.show();

}

void intensity_return(){
  if(intensity_offset > 0){
    intensity_offset = intensity_offset - ( (float(max_intensity_offset)+0.1) - (intensity_offset) ) / 50 ; //wtf is this maths james. Why 50.
  }
}


void intensity_map(){
  int loudness = mapped_wind();
  
  if( loudness > intensity_offset){       //When a louder sound than the current offset is recorded,
    max_intensity_offset = loudness;      //Track current maximum for smooth curve
    intensity_offset = loudness;          //Value to map to a curve
  }
  
  //cap at 100
  intensity_offset = min(intensity_offset, max_intensity_offset);
}

int mapped_wind(){
  int output = map(analogRead(A0), 0, 500, 0,100); //I found these values work for the electret mic I have used
  return output;  
}


void incrementIntensity(int n){
 
  hsi_values[n][2] = min(hsi_values[n][2] + intensity_increments[n], 100);  //Cap at 100
  RGBColor color = converter.HSItoRGB(hsi_values[n][0], hsi_values[n][1], hsi_values[n][2]);
  strip.setPixelColor(n, color.red, color.green, color.blue); 
}

void decrementIntensity(int n){
  RGBColor color = converter.HSItoRGB(hsi_values[n][0], hsi_values[n][1], hsi_values[n][2] - intensity_offset);
  hsi_values[n][2] = max(hsi_values[n][2] - intensity_increments[n], 0);  //Cap at 0
  strip.setPixelColor(n, color.red, color.green, color.blue); 

}

void incrementHue(int n){
  RGBColor color = converter.HSItoRGB(hsi_values[n][0], hsi_values[n][1], hsi_values[n][2] - intensity_offset);
  hsi_values[n][0] = hsi_values[n][0] + hue_increments[n];
  
  if (hsi_values[n][0] < hue_ranges[n][0] || hsi_values[n][0] > hue_ranges[n][1]) {
    hue_increments[n] = -hue_increments[n];
  }
  strip.setPixelColor(n, color.red, color.green, color.blue);   
}



