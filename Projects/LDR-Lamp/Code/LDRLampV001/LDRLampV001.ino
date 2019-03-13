
#include <CapacitiveSensor.h>
#include <SimpleKalmanFilter.h>
#include <Adafruit_NeoPixel.h>
#include <ColorConverter.h>

//////////
// PINS //
//////////

#define NUMPIX 24
#define PIXPIN 6
#define TASKLIGHTPIN 5

///////////
// DEBUG //
///////////

// Serial output refresh time
const long SERIAL_REFRESH_TIME = 10;
long refresh_time;



////////////////
// TASK LIGHT //
////////////////



int taskLightCurrentValue = 0;
int taskLightMaxValue = 255;

int state = 0;    //0 = off, 1 = turning on, 2 = on, 3 = turning off
int fadeIncrement = 1;  //increment to fade by
int fadeInterval = 15; //ms between increment
long fadeCheckpoint = 0;


///////////////
// NEOPIXELS //
///////////////

int pixIntensity = 0;
int pixCountMax = 5;  //cycles each pix is on for
int pixCounter = 0;     //keep track of time per pix
int pixIndex = 0;       //keep track of which pix is on
bool pixFlag = false;   //whether to begin pix

//Store some colour values for pixels to chill at (IN HSV!)
int pixelsDefaultHSI[NUMPIX][3];

//Store the current displayable values of pixel colour
int pixelsCurrentHSI[NUMPIX][3];

int startingHueMin = 5;
int startingHueMax = 30;

//////////////////
// INTERACTIONS //
//////////////////

bool  isTouching = false;
float touchSinewaveOffset = 0;    //Keep track of where in the curve you are. Start at 0 since sin(0) = 0, no offset
float touchSinewaveOffsetIncrement = 0.00000002;       // What a tiny number! Why this order of magnitude tho?
float touchSinewaveAmplitudeMultiplier = 255;          // Amount of Intensity to vary with the wave

/////////////
// UTILITY //
/////////////

float deltaTime = 0;  //Keep track of the change in time from loop to loop 
                      // so we can standardise speeds

// When changing an increment, the longer the delay in the previous loop, the greater the
// next increment should be. Fortunately it's a simple proportional relationship.

int randomMessageFrequency = 1000; //ms interval to check for new message
int randomMessageProbabilty = 2; // Chance out of 100 of 'new message' being sent
int randomMessageCheckpoint = 0;

///////////////
// LIBRARIES //
///////////////

CapacitiveSensor   cs_4_2 = CapacitiveSensor(4, 2);       // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIX, PIXPIN, NEO_GRB + NEO_KHZ800);
ColorConverter converter;
SimpleKalmanFilter simpleKalmanFilter(2, 2, 0.05);

void setup()
{
  cs_4_2.set_CS_AutocaL_Millis(0xFFFFFFFF);     // turn off autocalibrate on channel 1 - just as an example

  Serial.begin(9600);

  //Set up the neopixel strip
  pixSetupDefaults();

  // Take a breath before turning on.
  delay(1000);
}

void loop()
{
  long loopStartTime = millis();
  switch (state) {
    case 0:
      offState();
      break;
    case 1:
      turningOnState(loopStartTime);
      break;
    case 2:
      onState(loopStartTime);
      break;
    case 3:
      // TURN OFF
      break;
    default:
      // Errors?
      break;
  }

  deltaTime = (loopStartTime - millis())/1000;
}

void offState() {
  //Wait for 2 secs
  if (millis() - 2000 > 0) {
    state = 1;
  }
}

void turningOnState(long loopStartTime) {


  
  if (taskLightCurrentValue >= 255) {
    cs_4_2.set_CS_AutocaL_Millis(0xFFFFFFFF);
    state = 2;
    return;
  }

  if (loopStartTime - fadeCheckpoint > fadeInterval) {
    
    taskLightCurrentValue += fadeIncrement;
    taskLightCurrentValue = constrain(taskLightCurrentValue, 0, 255);
    fadeCheckpoint = loopStartTime;

    float fadePI = (float)taskLightCurrentValue/255 * PI - PI/2;
    float easetaskLightCurrentValue = (sin(fadePI)+1.0) * 128.0;  //Nice ease out/ease in

    easetaskLightCurrentValue = constrain(easetaskLightCurrentValue, 0, 255);
    analogWrite(TASKLIGHTPIN, easetaskLightCurrentValue);
    //SET NEOPIX FADE IN
    handleNeoPixTurningOnState(easetaskLightCurrentValue);

  }

  

}

void onState(long loopStartTime) {
  // Read and use capacitive touch sensor values
  senseCapacitiveTouch(loopStartTime);    
  capacitiveTouchFeedback();              
  
  handleFakeInput();  // Simulate another connected lamp sending a message.
  handlepixOnState(); // Master function to deal with neopix stuff.

  analogWrite(TASKLIGHTPIN, taskLightCurrentValue);

}

void senseCapacitiveTouch(long loopStartTime) {
  
  long total1 =  cs_4_2.capacitiveSensor(5);  // 5 samples is relatively inaccurate but we are going for speed not accuracy.

  total1 = constrain(total1, 0, 100000);      // In my tests anything abut 100K is an erratic outlier/bug, so cap at this value

  float mapped_val = total1 / 1000.0;         // Narmalise to 0-100 for use with Kalman filter

  // calculate the estimated value with Kalman Filter
  float estimated_value = simpleKalmanFilter.updateEstimate(mapped_val);

  detectThreshhold(estimated_value);          // Determine whether reading is above 'touch' threshold
  
}

void capacitiveTouchFeedback(){
  
  if(isTouching){
    //Setup sinewave (that remembers where it is) for adjusting brightness.
    
    float sineOffset = touchSinewaveOffset;
    float amplitude = (sin(sineOffset)+1)/2 * touchSinewaveAmplitudeMultiplier;

    // Default brightness is full. As sinewave increases, output brightness decreases.
    taskLightCurrentValue = taskLightMaxValue - amplitude;

    touchSinewaveOffset += (touchSinewaveOffsetIncrement * deltaTime);    // Use deltaTime here for framerate-independent wave.

    if (millis() > refresh_time) {
      Serial.println(taskLightCurrentValue);
      refresh_time = millis() + SERIAL_REFRESH_TIME;
    }
  }

  
}

void detectThreshhold(float val) {
  //Arbitrary based on tests. Todo: figure out better way to determine threshold?

  //Threshold = 20;
  
  if(val > 20){
   digitalWrite(13, HIGH); //Debug on arduino LED pin.
   isTouching = true;
  }
  else{
    isTouching = false;
  }


}

void handleFakeInput() {
  //Random input for demonstration purposes
  
  if(millis()- randomMessageCheckpoint > randomMessageFrequency){
    if(random(100) < randomMessageProbabilty){
      pixRandomise();
    }
    
  randomMessageCheckpoint = millis();
  }
}



/////////////////////
// NEO PIX SCRIPTS //
// --------------- //
// Function syntax //
// pixFunctionName //
/////////////////////

void pixRandomise(){
  for(int i = 0; i < NUMPIX; i++){
//    pixelsCurrentHSI[i][0];
//    pixelsCurrentHSI[i][1] = sat;
    pixelsCurrentHSI[i][2] = random(0,100);
  }
}

void pixReturnToDefault(){
  
  for (int i = 0; i < NUMPIX; i++) {
    int hue= pixelsDefaultHSI[i][0] ;
    int sat = pixelsDefaultHSI[i][1];
    
    if(pixelsDefaultHSI[i][2] > pixelsCurrentHSI[i][0]){
      //decrememnt
      pixelsDefaultHSI[i][2]++;
    }else if(pixelsDefaultHSI[i][2] < pixelsCurrentHSI[i][0]){
      //increment
      pixelsDefaultHSI[i][2]--;
    }else{
      //do nothing
    }
    
    
    
  }
}

void handleNeoPixTurningOnState(int fadingValue) {
  //Handle neopix while lamp is turning on.
  pixFadeIn(fadingValue);
}


void handlepixOnState() {
  //Handle neopix when lamp is in ON state

  // Detect if in change cycle, 
  //  if so, continue in change cycle
  // fade to default
  // Show Pix

  pixReturnToDefault();
  pixShowCurrent();
  
  
}

void pixFadeIn(int fadingValue) {

  // Normalise fadingValue to float (0-1)
  // Set pixelCurrentHSV to pixelDefaultHSV * normalisedtaskLightCurrentValue

  float fadingValue_normalised = fadingValue/255.0*1.0;
  
  for (int i = 0; i < NUMPIX; i++) {
    int hue= pixelsDefaultHSI[i][0] ;
    int sat = pixelsDefaultHSI[i][1];
    int inty = pixelsDefaultHSI[i][2] * fadingValue_normalised;
    
    pixelsCurrentHSI[i][0] = hue;
    pixelsCurrentHSI[i][1] = sat;
    pixelsCurrentHSI[i][2] = inty;
    
  }
  
  pixShowCurrent();
}

void pixChangeState(){
  if (pixFlag) {
    pixCounter++;
    if (pixCounter >= pixCountMax) {
      pixels.setPixelColor(pixIndex, pixels.Color(0, 0, 0));
      pixCounter = 0;
      pixIndex++;
    }

    if (pixIndex > 5) { //numpix-1, end of array
      pixCounter = 0;
      pixIndex = 0;
      pixFlag = false;
      pixels.clear();
      pixels.show(); // This sends the updated pixel color to the hardware.
      return;
    }
    
    pixels.setPixelColor(pixIndex, pixels.Color(150, 50, 0));
    pixels.show();
  }

}



void pixSetupDefaults(){
  pixels.begin();
  pixels.clear();
  pixels.show();
  
  for(int i = 0; i < NUMPIX; i++){
    //Let's make this a pleasant oragne for now
    pixelsDefaultHSI[i][0] = random(startingHueMin,startingHueMax);
    pixelsDefaultHSI[i][1] = 100;
    pixelsDefaultHSI[i][2] = 50;
  }
}

void pixShowCurrent(){
  for (int i = 0; i < NUMPIX; i++) {
    int hue= pixelsCurrentHSI[i][0];
    int sat = pixelsCurrentHSI[i][1];
    int inty = pixelsCurrentHSI[i][2];
    
    RGBColor color = converter.HSItoRGB(hue, sat, inty);
    pixels.setPixelColor(i, color.red, color.green, color.blue); 
    pixels.show();
  }
}

