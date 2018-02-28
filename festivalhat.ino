#include <FastLED.h>

// define your LED hardware setup here
#define CHIPSET     WS2801
//#define LED_PIN     23
#define DATA_PIN 10
#define CLOCK_PIN 11
#define COLOR_ORDER GRB
// just in case you have dead pixels at the beginning
// of your strip. If not it is just 0 (lucky you!).

// set master brightness 0-255 here to adjust power consumption
// and light intensity
#define BRIGHTNESS  20
// matrix size
const uint8_t WIDTH  = 17;
const uint8_t HEIGHT = 20;
#define NUM_LEDS (WIDTH * HEIGHT)
CRGB leds[NUM_LEDS];

// MSGEQ7 wiring based on spectrum analyser shield
#define MSGEQ7_STROBE_PIN 4
#define MSGEQ7_RESET_PIN  5
#define AUDIO_LEFT_PIN    A0

// storage for the MSGEQ7 band levels
byte left[7]; 

// noise related 
static uint16_t x;
static uint16_t y;
static uint16_t z;
static uint16_t scale;
// this is the array that we keep our computed noise values in
uint8_t noise[WIDTH][HEIGHT];

void setup() {
FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setDither(0);
  Serial.begin(38400);
      Serial.print("setup start");
        Serial.println(" ");
  InitMSGEQ7();
  //some point in the noise space to start with
  x = random16();
  y = random16();
  z = random16();
        Serial.print("setup end");
        Serial.println(" ");
}

// translates from x, y into an index into the LED array and
// finds the right index for a serpentine matrix
int XY(int x, int y) { 
  if(y > HEIGHT) { 
    y = HEIGHT; 
  }
  if(y < 0) { 
    y = 0; 
  }
  if(x > WIDTH) { 
    x = WIDTH;
  } 
  if(x < 0) { 
    x = 0; 
  }

    return (x * (WIDTH) + y); 
  
}

// Bresenham line algorythm 
void Line(int x0, int y0, int x1, int y1, byte color) {
  int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2; 
  for(;;) {  
    leds[XY(x0, y0)] = CHSV(color, 255, 255);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 > dy) { 
      err += dy; 
      x0 += sx; 
    } 
    if (e2 < dx) { 
      err += dx; 
      y0 += sy; 
    } 
  }
}

void ShowFrame() {
  FastLED.show();
  LEDS.countFPS();
  // check your serial monitor to see the fps...
}

// wake up the MSGEQ7
void InitMSGEQ7() {
  pinMode(MSGEQ7_RESET_PIN, OUTPUT);      
  pinMode(MSGEQ7_STROBE_PIN, OUTPUT);   
  digitalWrite(MSGEQ7_RESET_PIN, LOW);     
  digitalWrite(MSGEQ7_STROBE_PIN, HIGH); 
}

// get the data from the MSGEQ7
void ReadAudioMono() {
  digitalWrite(MSGEQ7_RESET_PIN, HIGH);
  digitalWrite(MSGEQ7_RESET_PIN, LOW);
  for(byte band = 0; band < 7; band++) {
    digitalWrite(MSGEQ7_STROBE_PIN, LOW); 
    // depends on your setup how much time it needs to have
    // a settled output level, 
    // most people wait 30 Âµs
    delayMicroseconds(20);
    //read the 10 bit band values and map them down to a byte 
    left[band] = analogRead(AUDIO_LEFT_PIN)/4; 
    digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
  }
}

// have an eye on the 7 frequency bands
void DrawAnalyzer() {
  // dim area
  for(int i = 0; i < 7; i++) {
    for(int j = 0; j < 4; j++) {
      leds[XY(i + 9, 15 - j)].nscale8(127);
      ;
    }
  }
  // draw lines according to the band levels 
  for(int i = 0; i < 7; i++) {
    Line(i + 9, 15, i + 9, 15 - left[i] / 64, (-i) * 15);
  }
  ShowFrame();
}

// calculate noise matrix
// x and y define the lower left point
void FillNoise() {
  for(int i = 0; i < WIDTH; i++) {
    int ioffset = scale * i;
    for(int j = 0; j < HEIGHT; j++) {
      int joffset = scale * j;
      noise[i][j] = inoise8(x + ioffset, y + joffset, z);
    }
  }
}

// calculate noise matrix
// x and y define the center
void FillNoiseCentral() {
  for(int i = 0; i < WIDTH; i++) {
    int ioffset = scale * (i - 8);
    for(int j = 0; j < HEIGHT; j++) {
      int joffset = scale * (j - 8);
      noise[i][j] = inoise8(x + ioffset, y + joffset, z);
    }
  }
}


void FunkyNoiseBeat() {
  ReadAudioMono();
  // zoom controlled by band 5 (snare)
  scale = left[5] / 4;
  FillNoise();
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // color controlled by lowest basedrum + noisevalue  
      left[0] + noise[i][j],
      255,
      // brightness controlled by kickdrum
      left[1]);
    }
  }
  DrawAnalyzer();
  // one white pixel to help you recognize the pattern
  leds[XY(9,15)] = 0xffffff;
  ShowFrame();
}

void FunkyNoisePump() {
  ReadAudioMono();
  // zoom factor set by inversed band 0 (63-0)
  // 3 added to avoid maximal zoom = plain color
  scale = 3 + 63 - left[0] / 4;
  // move one step in the noise space when basedrum is present
  // = slowly change the pattern while the beat goes
  if (left[0] > 128) z++;
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + noise[i][j] + left[0] / 4,
      // no changes here 
      255,
      // brightness controlled by the average of band 0 and 1
      // 20 to ensure a minimum brigness
      // = limiting contrast
      20 + (left[0] + left[1]) / 2);
    }
  }
  // just to visualize all MSGEQ7 data
  DrawAnalyzer();
  leds[XY(10,15)] = 0xffffff;
  // show the result and count fps
  ShowFrame();
}

void FunkyNoiseCirc() {
  ReadAudioMono();
  // fix zoom factor
  scale = 20;
  // move one step in the noise space when basedrum is present
  // = slowly change the pattern while the beat goes
  if (left[0] > 128) z=z+10;
  // x any y is defining the position in the noise space
  x = left[3] / 2;
  y = left[0] / 2;
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + noise[i][j] + left[0] / 4,
      // no changes here 
      255,
      // brightness controlled by noise
      noise[i][j]);
    }
  }
  // just to visualize all MSGEQ7 data
  DrawAnalyzer();
  leds[XY(11,15)] = 0xffffff;
  // show the result and count fps
  ShowFrame();
}

void FunkyNoiseFire() {
  ReadAudioMono();
  // fix zoom factor
  scale = 20;
  // position in the noise space depending on band 5
  // = change of the pattern
  z=left[5] / 2;
  // x scrolling through
  // = horizontal movement
  x = x + 20;
  // y controlled by lowest band
  // = jumping of the pattern
  y = left[0];
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + noise[i][j] + left[0] / 4,
      // no changes here 
      255,
      // brightness controlled by noise
      noise[i][j]);
    }
  }
  // just to visualize all MSGEQ7 data
  DrawAnalyzer();
  leds[XY(12,15)] = 0xffffff;
  // show the result and count fps
  ShowFrame();
}

void FunkyNoiseFill() {
  ReadAudioMono();
  // dynamic zoom: average of band 0 and 1
  scale = 128-(left[0]+left[1])/2;
  // position in the noise space depending on x, y and z
  // z slowly scrolling
  z++;
  // x static
  // y scrolling 
  y = y + 5;
  // calculate the noise array
  // x any y are this time defining THE CENTER 
  FillNoiseCentral();
  // map the noise
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      120 + noise[i][j] * 2,
      // no changes here 
      255,
      // brightness controlled by noise
      noise[i][j]);
    }
  }
  // just to visualize all MSGEQ7 data
  DrawAnalyzer();
  leds[XY(13,15)] = 0xffffff;
  // show the result and count fps
  ShowFrame();
}


void loop() {
// for(int i = 0; i < 250; i++) FunkyNoiseFill();   //Not so GOOOD
 for(int i = 0; i < 250; i++) FunkyNoiseBeat();    //PUMPING GOOOOOOOOOD
// for(int i = 0; i < 250; i++) FunkyNoiseCirc();     // NOT SO FINE
 for(int i = 0; i < 250; i++) FunkyNoisePump();     //PUMPING GOOOOOOOOOD
// for(int i = 0; i < 250; i++) FunkyNoiseFire();     //FIRE OK

        Serial.print("programm end");
        Serial.println(" ");

}


