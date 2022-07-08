// This example is minimally adapted from one in PJRC's OctoWS2811 Library.
// Changes primarily focus on data format: this version always writes in
// RGB order with no gamma correction, rows always left-to-right; these
// operations now happen on the microcontroller side with the corresponding
// Arduino sketch (VideoMSC); the colorWiring() function and gamma[] table
// are removed. To work with current version of Processing, new Movie() had
// to move into setup(), and end-of-video detection changed.
// Mentions of "SD card" throughout are left intact here, though the
// corresponding playback code actually uses onboard flash storage, not SD.
// THIS IS CODE FOR PROCESSING (processing.org), NOT ARDUINO.
// Processing is oddly persnickety and you might need to run this twice
// to convert a full video rather than just the first frame.

/*  OctoWS2811 movie2sdcard.pde - Convert video for SD card playing, with
      Teensy 3.1 running OctoWS2811 VideoSDcard.ino

    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2014 Paul Stoffregen, PJRC.COM, LLC

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

// To configure this program, edit the following:
//
//  1: Change myMovie to open a video file of your choice    ;-)
//     Also change the output file name.
//
//  2: Edit ledWidth, ledHeight for your LEDs
//
//  3: Edit framerate.  This configures the speed VideoSDcard
//     will play your video data.  It's also critical for merging
//     audio to be played by Teensy 3.1's digital to analog output.
//

import processing.video.*;
import processing.serial.*;
import java.io.*;

Movie myMovie;

int ledWidth =  30;          // size of LED panel
int ledHeight = 16;

double framerate = 23.98;    // You MUST set this to the movie's frame rate
                             // Processing does not seem to have a way to detect it.

FileOutputStream myFile;     // edit output filename below...

PImage ledImage;
long elapsed_picoseconds=0L;
long elapsed_microseconds=0L;
long picoseconds_per_frame = (long)(1e12 / framerate + 0.5);
boolean fileopen=true;
PImage tmpImage;

void setup() {
  myMovie = new Movie(this, "/Users/me/Desktop/mymovie.mp4");

  try {
    myFile = new FileOutputStream("/Users/me/Desktop/mymovie.bin");
  } catch (Exception e) {
    exit();
  }
  ledImage = createImage(ledWidth, ledHeight, RGB);
  size(720, 560);  // create the window

  myMovie.play();  // start the movie :-)
}

int lastEventTime = 0;

// movieEvent runs for each new frame of movie data
void movieEvent(Movie m) {
  // read the movie's next frame
  m.read();
  lastEventTime = millis();

  elapsed_picoseconds += picoseconds_per_frame;
  int usec = (int)((elapsed_picoseconds / 1000000L) - elapsed_microseconds);
  elapsed_microseconds += (long)usec;
  println("usec = " + usec);

  // copy the movie's image to the LED image
  // Processing is acting strange and won't antialias a copy-with-scale
  // from a Movie, so make an interim full-size copy, resize that, then
  // copy the result to ledImage (referenced here and in draw()).
  tmpImage = createImage(m.width, m.height, RGB);
  tmpImage.copy(m, 0, 0, m.width, m.height, 0, 0, m.width, m.height);
  tmpImage.resize(ledWidth, ledHeight);
  ledImage.copy(tmpImage, 0, 0, ledWidth, ledHeight, 0, 0, ledWidth, ledHeight);

  // convert the LED image to raw data
  byte[] ledData =  new byte[(ledWidth * ledHeight * 3) + 5];
  image2data(ledImage, ledData);
  ledData[0] = '*';  // first Teensy is the frame sync master
  
  ledData[1] = (byte)(ledWidth * ledHeight);
  ledData[2] = (byte)((ledWidth * ledHeight) >> 8);
  ledData[3] = (byte)(usec);   // request the frame sync pulse
  ledData[4] = (byte)(usec >> 8); // at 75% of the frame time
  // send the raw data to the LEDs  :-)
  //ledSerial[i].write(ledData); 
  try {
    myFile.write(ledData);
  } catch (Exception e) {    
    exit();
  }
}

// image2data converts an image to OctoWS2811's raw data format.
// The number of vertical pixels in the image must be a multiple
// of 8.  The data array must be the proper size for the image.
void image2data(PImage image, byte[] data) {
  int offset = 5;
  int i;

  for (i = 0; i < image.width * image.height; i++) {
    int pixel = image.pixels[i];
    data[offset++] = byte(pixel >> 16);
    data[offset++] = byte(pixel >> 8);
    data[offset++] = byte(pixel);
  }
}

// draw runs every time the screen is redrawn - show the movie...
void draw() {
  // If nothing read from movie in last second, assume end, close output
  if ((millis() - lastEventTime) > 1000) {
    if (fileopen) {
      println("movie stop, closing output file");
      try {
        myFile.close();
      } catch (Exception e) {
      }
      exit();
    }
  } else {
    image(myMovie, 0, 80);
    image(ledImage, 240 - ledWidth / 2, 10);
  }
}

// respond to mouse clicks as pause/play
boolean isPlaying = true;
void mousePressed() {
  if (isPlaying) {
    myMovie.pause();
    isPlaying = false;
  } else {
    myMovie.play();
    isPlaying = true;
  }
}
