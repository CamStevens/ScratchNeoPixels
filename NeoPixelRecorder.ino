/*
 *This program is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

/// PING command.  Used to verify connectivity between Scratch and Arduino
const uint8_t COMMAND_PING = 0x01;

/// The marker to indicate the beginning of the PING response
const int PING_RESPONSE = 0xF00F;

/// Perform SHIMMER effect
const uint8_t COMMAND_SHIMMER = 0x02;

/// Perform COLOR_WIPE effect
const uint8_t COMMAND_COLOR_WIPE = 0x03;

/// Perform RAINBOW effect
const uint8_t COMMAND_RAINBOW = 0x04;

/// Perform RAINBOW CYCLE effect
const uint8_t COMMAND_RAINBOW_CYCLE = 0x05;

/// Perform THEATRE CHASE effect
const uint8_t COMMAND_THEATRE_CHASE = 0x06;

/// Perform THEATRE CHASE RAINBOW effect
const uint8_t COMMAND_THEATRE_CHASE_RAINBOW = 0x07;

/// Start recording commands
const uint8_t COMMAND_START_RECORDING = 0x08;

/// Stop recording commands
const uint8_t COMMAND_STOP_RECORDING = 0x09;

/// Start playback of recorded commands
const uint8_t COMMAND_START_PLAYBACK = 0x0A;

/// Stop playback of recorded commands
const uint8_t COMMAND_STOP_PLAYBACK = 0x0B;

/// Set the brightness
const uint8_t COMMAND_SET_BRIGHTNESS = 0x0C;

/// Perform SPARKLE effect
const uint8_t COMMAND_SPARKLE = 0x0D;

/// Perform FADE effect
const uint8_t COMMAND_FADE = 0x0E;

/// Dump EEPROM memory
const uint8_t COMMAND_DUMP_EEPROM = 'd';

/// The amount of EEPROM space available for recording NeoPixel commands
const int COMMAND_BUFFER_SIZE = 1024;

/// The next address to write a command
int writeAddress;

/// The number of recorded commands
uint8_t commandCount = 0;

/// Indicates whether we are currently recording commands
bool recordingCommands = false;

/// The current time in milliseconds when recording or playback began
unsigned long startMillis;

/// The next address to read a command
int readAddress;

/// Indicates whether we are currently playing back commands
bool playingCommands = false;

/// Indicates whether to continuously repeat playback of recorded commands
bool repeatPlayback = false;

///  Indicates the index of the command currently being played back
uint8_t currentCommand = 0;

/// The offset in milliseconds when the next command should be played back
unsigned long nextCommandMillis;

/// The digital output pin connected to the NeoPixel
#define NEO_PIXEL_PIN 9

/// The number of pixels on the NeoPixel
#define PIXEL_COUNT 37

/// Instantiates the NeoPixel
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, NEO_PIXEL_PIN);

/// Indicates whether a command is currently being executed for the current pixel
uint8_t pixelInUse[PIXEL_COUNT];

/// Used to represent commands currently being executed.
typedef struct PixelCommand_t {
  uint8_t commandCode;        // The command code
  uint8_t wait;               // The time between each step
  unsigned long nextMillis;   // The time when the next step should be executed
  unsigned long duration;     // The duration for this command
  uint8_t red;                // The red color component for this command
  uint8_t green;              // The green color component for this command
  uint8_t blue;               // The blue color component for this command
  uint8_t startPixel;         // The starting pixel for this command 
  uint8_t endPixel;           // The ending pixels for this command 
  uint8_t currentPixel;       // The pixels for the command command step
  uint8_t currentStep;        // The current step being executed
} PixelCommand;

/// The number of commands that can be executing simultaneously.
#define COMMAND_BUFFER_ENTRIES 20 

/// The buffer of commands currently being executed
PixelCommand commandBuffer[COMMAND_BUFFER_ENTRIES];

/**
 * Performs one time setup functions
 */
void setup() {

  //
  // The ScratchX plugin will connect at 38,400
  //
  Serial.begin(38400);

  //
  // Setup NEO Pixel
  //
  strip.begin();
  strip.show();

  //
  // Wait 5 seconds, and then if the Serial port is not connected, go into continuous playback mode
  //
  delay(5000);
  if (!Serial) {
    repeatPlayback = true;
    processStartPlaybackCommand();
  }
}

/*
 * Retrieves a command from Serial or EEPROM if available.  Then steps all currently executing commands.
 */
void loop() {

  uint8_t commandCode = getNextCommandCode();
  switch (commandCode) {
    case COMMAND_PING:
      processPingCommand();
      break;

    case COMMAND_DUMP_EEPROM:
      processDumpEEPromCommand();
      break;

    case COMMAND_COLOR_WIPE:
      startColorWipeCommand();
      queueNextCommand();
      break;

    case COMMAND_SPARKLE:
      startSparkleCommand();
      queueNextCommand();
      break;

    case COMMAND_SHIMMER:
      startShimmerCommand();
      queueNextCommand();
      break;

    case COMMAND_FADE:
      startFadeCommand();
      queueNextCommand();
      break;

    case COMMAND_RAINBOW:
      startRainbowCommand();
      queueNextCommand();
      break;
      
    case COMMAND_THEATRE_CHASE:
      startTheatreChaseCommand();
      queueNextCommand();
      break;
      
    case COMMAND_SET_BRIGHTNESS:
      processSetBrightnessCommand();
      queueNextCommand();
      break;

    case COMMAND_START_RECORDING:
      processStartRecordingCommand();
      break;
      
    case COMMAND_STOP_RECORDING:
      processStopRecordingCommand();
      break;
      
    case COMMAND_START_PLAYBACK:
      processStartPlaybackCommand();
      break;
      
    default:
      break;
  }

  //
  // Step buffered commands
  //
  stepPixelCommands();
}

/**
 * 
 * Returns the next command code by either reading a single byte
 * from the Serial port or by replaying the next recorded command
 * 
 */
uint8_t getNextCommandCode() {

  //
  // Handle playback of recorded commands
  //
  if (playingCommands) {

    //
    // Check if it is time to replay the next command
    //
    if (millis() - startMillis < nextCommandMillis) {
      return 0;
    } else {

      //
      // Retrieve the next command code from EEPROM and return it
      //
      uint8_t commandCode;
      EEPROM.get(readAddress, commandCode);
      readAddress += sizeof(commandCode);
      currentCommand++;
      return commandCode;   
    }
  } else {

    //
    // Wait for a command to appear on the Serial port
    //
    if (Serial.available() > 0) {
      uint8_t commandCode = Serial.read();
      if (recordingCommands && shouldRecordCommand(commandCode)) {
        if (writeAddress == COMMAND_BUFFER_SIZE) {
          processStopRecordingCommand();
        } else {

          //
          // Store the delay time and the command code
          //          
          nextCommandMillis = millis() - startMillis;
          unsigned int intMillis = nextCommandMillis & 0x0000FFFF;
          EEPROM.put(writeAddress, intMillis);
          writeAddress += sizeof (intMillis);
          EEPROM.put(writeAddress, commandCode);
          writeAddress += sizeof (commandCode);
          commandCount++;
        }
      }
      return commandCode; 
    } else {
      return 0; 
    }
  } 
}

/**
 * 
 * Returns the next byte in a command message by either reading a single byte
 * from the Serial port or by replaying the next recorded byte
 * 
 */
uint8_t getNextCommandByte() {
  if (playingCommands) {
    uint8_t commandByte;
    EEPROM.get(readAddress, commandByte);
    readAddress += sizeof (commandByte);
    return commandByte;
  } else {
    uint8_t commandByte = Serial.read();
    if (recordingCommands) {
        EEPROM.put(writeAddress, commandByte);
        writeAddress += sizeof (commandByte);

        //
        // Check if we have filled the command buffer
        //
        if (writeAddress == COMMAND_BUFFER_SIZE) {
          commandCount--;
          processStopRecordingCommand();
        }
    }
    return commandByte;
  }
}

/**
 * 
 * Called after the previous command has been parsed when replaying commands from EEPROM.  The delay time for the next command is read
 * from EEPROM.
 * 
 */
void queueNextCommand() {

  if (!playingCommands) {
    return;
  }
  
  //
  // Check if we have replayed all the commands.  If we have, check if we need to repeat or terminate playback
  //
  if (currentCommand == commandCount) {
    if (repeatPlayback) {
      processStartPlaybackCommand();
    } else {
      processStopPlaybackCommand();
    }

    return;
  }

  //
  // Retrieve the next command delay from EEPROM
  //
  unsigned int intDelay;
  EEPROM.get(readAddress, intDelay);
  readAddress += sizeof(intDelay);
  nextCommandMillis = intDelay;
}

/**
 * 
 * Determines whether a specific command type should be recorded.
 * 
 */
boolean shouldRecordCommand(uint8_t commandCode) {
  switch (commandCode) {
    case COMMAND_PING:
    case COMMAND_START_RECORDING:
    case COMMAND_STOP_RECORDING:
    case COMMAND_START_PLAYBACK:
    case COMMAND_STOP_PLAYBACK:
    case COMMAND_DUMP_EEPROM:
      return false;
     default:
      return true;
  }
}

/**
 * 
 * Processes the COMMAND_PING by sendind a PING_RESPONSE
 * 
 */
void processPingCommand() {
  Serial.write(0xF0);
  Serial.write(0x0F);
}

/**
 * 
 * Process the COMMAND_DUMP_EEPROM command by writing out the EEPROM contents to Serial
 * 
 */
void processDumpEEPromCommand() {
  uint8_t val;
  
  for (int i = 0; i < 1024; i++) {
    EEPROM.get(i,val);
    Serial.print(val, HEX);
    Serial.print(F(" "));
    if (i % 16 == 0) {
      Serial.println(F(" "));
    }
  }
}

/**
 * 
 * Processes the COMMAND_START_PLAYBACK command
 * 
 */
void processStartPlaybackCommand() {
    playingCommands = true;
    recordingCommands = false;
    readAddress = 1;
    currentCommand = 0;
    EEPROM.get(0, commandCount);
    startMillis = millis();
    queueNextCommand();
}

/**
 * 
 * Processes the COMMAND_STOP_PLAYBACK command
 * 
 */
void processStopPlaybackCommand() {
  playingCommands = false;
  recordingCommands = false;
}

/**
 * 
 * Processes the COMMAND_START_RECORDING command
 * 
 */
void processStartRecordingCommand() {
  playingCommands = false;
  recordingCommands = true;
  writeAddress = 1;
  commandCount = 0;
  startMillis = millis(); 
}

/**
 * 
 * Processes the COMMAND_STOP_RECORDING command
 * 
 */
void processStopRecordingCommand() {
  playingCommands = false;
  recordingCommands = false;
  EEPROM.put(0, commandCount);
}

/**
 * 
 * Buffer a COMMAND_COLOR_WIPE command
 * 
 */
void startColorWipeCommand() {

  PixelCommand command;
  
  //
  // The message format is startPixel, endPixel, red, green, blue, wait
  //
  command.commandCode = COMMAND_COLOR_WIPE;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.currentPixel = command.startPixel;
  command.red = getNextCommandByte();
  command.green = getNextCommandByte();
  command.blue = getNextCommandByte();
  command.wait = getNextCommandByte();

  //
  // Wait for the pixels to become free, then queue this command
  //
  waitForPixels(command);
  command.nextMillis = millis();

  //
  // If there is no wait, render all the pixels right now.
  //
  if (!command.wait) {
    if (command.startPixel <= command.endPixel) {
      for (int i = command.startPixel; i <= command.endPixel; i++) {
        strip.setPixelColor(i, command.red, command.green, command.blue); 
      }
    } else {      
      for (int i = command.startPixel; i >= command.endPixel; i--) {
        strip.setPixelColor(i, command.red, command.green, command.blue); 
      }
    }
    strip.show();
  } else {
    bufferCommand(command);
  }
}

/**
 * 
 * Process the next step in a COMMAND_COLOR_WIPE command
 * 
 */
void processColorWipeCommand(int commandIndex) {

  PixelCommand command = commandBuffer[commandIndex];
  
  strip.setPixelColor(command.currentPixel, command.red, command.green, command.blue);
  strip.show();

  //
  // Check if we are done.  If so, delete the command.
  //
  if (command.currentPixel == command.endPixel) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
    return;
  }

  //
  // Update the current pixel index and next step time and save the command updates
  //
  if (command.startPixel <= command.endPixel) {
    command.currentPixel++;;
  } else {
    command.currentPixel--;
  }
  command.nextMillis = millis() + command.wait;
  commandBuffer[commandIndex] = command;
}

/**
 * 
 * Buffer a COMMAND_FADE command
 * 
 */
void startFadeCommand() {
  //
  // The message format is startPixel, endPixel, red, green, blue, wait
  //
  PixelCommand command;
  command.commandCode = COMMAND_FADE;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.red = getNextCommandByte();
  command.green = getNextCommandByte();
  command.blue = getNextCommandByte();
  command.wait = getNextCommandByte();
  command.currentStep = 0;

  //
  // Wait for the pixels to become free, then queue this command
  //
  waitForPixels(command);
  command.nextMillis = millis();
  bufferCommand(command);
}

/**
 * 
 * Processes the next step in a COMMAND_COLOR_FADE command
 * 
 */
void processFadeCommand(int commandIndex) {

  PixelCommand command = commandBuffer[commandIndex];

  for (int i = command.startPixel; i <= command.endPixel; i++) {
    uint32_t color = strip.getPixelColor(i);
    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >>  8);
    uint8_t b = (uint8_t)color;

    int rnew = (command.red - r) / (16 - command.currentStep) + r;
    if (rnew < 0) {
      rnew = 0;
    } else if (rnew > 255) {
      rnew = 255;
    }

    int gnew = (command.green - g) / (16 - command.currentStep) + g;
    if (gnew < 0) {
      gnew = 0;
    } else if (gnew > 255) {
      gnew = 255;
    }

    int bnew = (command.blue - b) / (16 - command.currentStep) + b;
    if (bnew < 0) {
      bnew = 0;
    } else if (bnew > 255) {
      rnew = bnew;
    }

    strip.setPixelColor(i, rnew, gnew, bnew);
  }
  strip.show();

  //
  // Check if we are done.  If so, delete the command.
  //
  if (command.currentStep == 16) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
    return;
  }
  
  //
  // Update the current step and next step time and save the command updates
  //
  command.currentStep++;
  command.nextMillis = millis() + command.wait;
  commandBuffer[commandIndex] = command;    
}

/**
 * 
 * Buffer a COMMAND_SHIMMER command
 * 
 */
void startShimmerCommand() {
  //
  // The message format is startPixel, endPixel, red, green, blue, duration
  //
  PixelCommand command;
  command.commandCode = COMMAND_SHIMMER;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.red = getNextCommandByte();
  command.green = getNextCommandByte();
  command.blue = getNextCommandByte();

  //
  // Wait for the pixels to become free, then queue this command
  //
  waitForPixels(command);
  command.nextMillis = millis();
  command.duration = command.nextMillis + (((unsigned long) getNextCommandByte()) << 8) + getNextCommandByte();
  bufferCommand(command);
}

/**
 * 
 * Processes the next step in the COMMAND_SHIMMER command
 * 
 */
void processShimmerCommand(int commandIndex) {
 
  PixelCommand command = commandBuffer[commandIndex];
  for(int pixel=command.startPixel; pixel<=command.endPixel; pixel++) {
      int shimmer = random(0,55);
      int r1 = command.red-shimmer;
      int g1 = command.green-shimmer;
      int b1 = command.blue-shimmer;
      if(g1<0) g1=0;
      if(r1<0) r1=0;
      if(b1<0) b1=0;
      strip.setPixelColor(pixel,r1,g1, b1);
  }
  strip.show();

  //
  // Update the next command firing time.  Check if we have reached the command duration
  //
  command.nextMillis = millis() + random(10,113);
    
  //
  // Check if we are done.  If so, delete the command.  Otherwise, save the updated command
  //
  if (command.nextMillis >= command.duration) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
  } else {
    commandBuffer[commandIndex] = command;
  }
}

/**
 * 
 * Buffer a COMMAND_SPARKLE command
 * 
 */
void startSparkleCommand() {
  //
  // The message format is startPixel, endPixel, red, green, blue, sparkles
  //
  PixelCommand command;
  command.commandCode = COMMAND_SPARKLE;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.red = getNextCommandByte();
  command.green = getNextCommandByte();
  command.blue = getNextCommandByte();
  command.currentStep = 0;

  //
  // Wait for the pixels to become free, set the initial pixel colors, then queue this command
  //
  waitForPixels(command);
  command.nextMillis = millis();
  command.duration = command.nextMillis + (((unsigned long) getNextCommandByte()) << 8) + getNextCommandByte();
  for(int pixel=command.startPixel; pixel<=command.endPixel; pixel++) {
      strip.setPixelColor(pixel, command.red, command.green, command.blue);
  }
  strip.show();
  bufferCommand(command);
}
 
/**
 * 
 * Processes the next step in the COMMAND_SPARKLE command
 * 
 */
void processSparkleCommand(int commandIndex) {

  PixelCommand command = commandBuffer[commandIndex];

  //
  // Is this a sparkle step, or clear sparkle step?
  //
  if (command.currentStep == 0) {
    int pixelCount = command.endPixel - command.startPixel + 1;
    int pixelsToSparkle = random(1,pixelCount / 4);
    for (int j = 0; j < pixelsToSparkle; j++) {
      int pixel = random(0,pixelCount) + command.startPixel;
      strip.setPixelColor(pixel,255,255,255);
    }
    strip.show();    
  } else {
    for(int pixel=command.startPixel; pixel <= command.endPixel; pixel++) {
        strip.setPixelColor(pixel, command.red, command.green, command.blue);
    }
    strip.show();    
  }

  //
  // Update the next command firing time.  Check if we have reached the command duration
  //
  command.nextMillis = millis() + random(10,113);
  if (command.currentStep == 0) {
    command.currentStep = 1;
  } else {
    command.currentStep = 0;
  }
    
  //
  // Check if we are done.  If so, delete the command.  Otherwise, save the updated command
  //
  if (command.nextMillis >= command.duration) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
  } else {
    commandBuffer[commandIndex] = command;
  }
}

/**
 * 
 * Processes the COMMAND_SET_BRIGHTNESS command
 * 
 */
void processSetBrightnessCommand() {

  uint8_t brightness = getNextCommandByte();
  strip.setBrightness(brightness);
  strip.show();
}

/**
 * 
 * Buffer a COMMAND_RAINBOW command
 * 
 */
void startRainbowCommand() {
  //
  // The message format is startPixel, endPixel, wait, duration
  //
  PixelCommand command;
  command.commandCode = COMMAND_RAINBOW;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.wait = getNextCommandByte();
  command.currentStep = 0;

  //
  // Wait for the pixels to become free, then queue this command
  //
  waitForPixels(command);

  command.nextMillis = millis();
  command.duration = command.nextMillis + (((unsigned long) getNextCommandByte()) << 8) + getNextCommandByte();
  bufferCommand(command);
}

/**
 * 
 * Processes the next step in the COMMAND_RAINBOW command 
 * 
 */
void processRainbowCommand(int commandIndex) {
  PixelCommand command = commandBuffer[commandIndex];

  for (int i = command.startPixel; i <= command.endPixel; i++) {
    strip.setPixelColor(i, Wheel((i+command.currentStep) & 255));
  }
  strip.show();

  //
  // Update the next command firing time.  Check if we have reached the command duration
  //
  command.nextMillis = millis() + command.wait;
  command.currentStep++;
  if (command.currentStep == 256) {
    command.currentStep = 0;
  }
    
  //
  // Check if we are done.  If so, delete the command.  Otherwise, save the updated command
  //
  if (command.nextMillis >= command.duration) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
  } else {
    commandBuffer[commandIndex] = command;
  }
}

/**
 * 
 * Buffer a COMMAND_THEATRE_CHASE command
 * 
 */
 void startTheatreChaseCommand() {
  //
  // The message format is startPixel, endPixel, red, green, blue, wait, duration
  //
  PixelCommand command;
  command.commandCode = COMMAND_THEATRE_CHASE;
  command.startPixel = getNextCommandByte();
  command.endPixel = getNextCommandByte();
  command.red = getNextCommandByte();
  command.green = getNextCommandByte();
  command.blue = getNextCommandByte();
  command.wait = getNextCommandByte();
  command.nextMillis = millis();
 
  //
  // Wait for the pixels to become free, then queue this command
  //
  waitForPixels(command);
  command.duration = command.nextMillis + (((unsigned long) getNextCommandByte()) << 8) + getNextCommandByte();
  command.currentStep = command.startPixel;
  bufferCommand(command);
}
 
/**
* 
* Processes the next step in the COMMAND_THEATRE_CHASE command 
* 
*/
void processTheatreChaseCommand(int commandIndex) {
  PixelCommand command = commandBuffer[commandIndex];

  int pixelsToLight = (command.endPixel - command.startPixel + 1) / 3;

  int onPixel = command.currentStep;
  int offPixel = command.currentStep - 1;
  if (offPixel < command.startPixel) {
    offPixel = command.endPixel;
  }
  for (int i = 0; i < pixelsToLight; i++) {
    strip.setPixelColor(offPixel, 0, 0, 0);    // turn the previously lit pixels off     
    strip.setPixelColor(onPixel, command.red, command.green, command.blue);    //turn every third pixel on
    onPixel += 3;
    if (onPixel > command.endPixel) {
      onPixel = onPixel - command.endPixel - 1 + command.startPixel;
    }
    offPixel += 3;
    if (offPixel > command.endPixel) {
      offPixel = offPixel - command.endPixel - 1 + command.startPixel;
    }
  }
  strip.show();

  command.currentStep++;
  if (command.currentStep > command.endPixel) {
    command.currentStep = command.startPixel;
  }
  command.nextMillis = millis() + command.wait;

  //
  // Check if we are done.  If so, delete the command.  Otherwise, save the updated command
  //
  if (command.nextMillis >= command.duration) {
    commandBuffer[commandIndex].commandCode = 0;
    setPixelsFree(command);
  } else {
    commandBuffer[commandIndex] = command;
  }
}

/**
 * 
 * Input a value 0 to 255 to get a color value.  
 * The colours are a transition r - g - b - back to r.
 * 
 */
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

/**
 * 
 * Marks the pixels associated with the specified command as busy.
 * 
 */
void setPixelsBusy(struct PixelCommand_t command) {
  if (command.startPixel <= command.endPixel) {
    for (int i = command.startPixel; i <= command.endPixel; i++) {
      pixelInUse[i] = 1;
    }
  } else {
    for (int i = command.endPixel; i <= command.startPixel; i++) {
      pixelInUse[i] = 1;
    }
  }
}

/**
 * 
 * Marks the pixels associated with the specified command as free.
 * 
 */
void setPixelsFree(struct PixelCommand_t command) {
  if (command.startPixel <= command.endPixel) {
    for (int i = command.startPixel; i <= command.endPixel; i++) {
      pixelInUse[i] = 0;
    }
  } else {
    for (int i = command.endPixel; i <= command.startPixel; i++) {
      pixelInUse[i] = 0;
    }
  }
}

/**
 * 
 * Checks if the pixels associated with the specified command are ALL free.
 * 
 */
bool arePixelsFree(struct PixelCommand_t command) {
  if (command.startPixel <= command.endPixel) {
    for (int i = command.startPixel; i <= command.endPixel; i++) {
      if (pixelInUse[i]) {
        return false;
      }
    }
  } else {
    for (int i = command.endPixel; i <= command.startPixel; i++) {
      if (pixelInUse[i]) {
        return false;
      }
    }
  }

  return true;
}

/**
 * 
 * Waits until all the pixels associated with the specified command are free.  Steps currently executing commands while waiting.
 * 
 */
void waitForPixels(struct PixelCommand_t command) {
  while (!arePixelsFree(command)) {
    stepPixelCommands();
  }
}

/**
 * 
 * Performs the next step of any pixel animation commands that are currently executing
 * 
 */
void stepPixelCommands() {
  unsigned long now = millis();
  for (int i = 0; i < COMMAND_BUFFER_ENTRIES; i++) {
    if (commandBuffer[i].commandCode && now >= commandBuffer[i].nextMillis) {
      switch (commandBuffer[i].commandCode) {
        case COMMAND_COLOR_WIPE:
          processColorWipeCommand(i);
          break;

        case COMMAND_FADE:
          processFadeCommand(i);
          break;

        case COMMAND_SHIMMER:
          processShimmerCommand(i);
          break;

        case COMMAND_SPARKLE:
          processSparkleCommand(i);
          break;

        case COMMAND_RAINBOW:
          processRainbowCommand(i);
          break;

        case COMMAND_THEATRE_CHASE:
          processTheatreChaseCommand(i);
          break;

         default:
          break;
      }
    }
  }

  delay(1);
}

/**
 * 
 * Adds the specified command to the buffer of pixel animation commands that are currently executing.
 * 
 */
void bufferCommand(struct PixelCommand_t command) {
  for (int i = 0; i < COMMAND_BUFFER_ENTRIES; i++) {
    if (!commandBuffer[i].commandCode) {
      commandBuffer[i] = command;
      setPixelsBusy(command);
      return;
    }
  }
}
