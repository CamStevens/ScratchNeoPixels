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
 (function(ext) {

  var connected = false;
  var device = null;
  var poller = null;

  /* TEMPORARY WORKAROUND
     this is needed since the _deviceRemoved method
     is not called when serial devices are unplugged*/
  var sendAttempts = 0;

  var pingCmd = new Uint8Array(1);
  pingCmd[0] = 1;

  function processInput(data) {
  }

  function getRed(pixelColor) {
    var colors = pixelColor.split(',');
    return colors[0];
  }
  
  function getGreen(pixelColor) {
    var colors = pixelColor.split(',');
    return colors[1];
  }
  
  function getBlue(pixelColor) {
    var colors = pixelColor.split(',');
    return colors[2];
  }
  
  function getStartPixel(pixelRange) {
        var range = pixelRange.split(',');
        return range[0];
  }
  
  function getEndPixel(pixelRange) {
        var range = pixelRange.split(',');
        return range[1];
  }
  
  ext.turnOffAllPixels = function() {
    this.setPixels('0,46', '0,0,0');
  }
  
  ext.setPixels = function(pixelRange, pixelColor) {
    this.colorWipe(pixelRange, pixelColor, 'nodelay');
  }
  
  ext.colorWipe = function(pixelRange, pixelColor, wait) {
    var output = new Uint8Array(7);
    output[0] = 0x03;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    output[3] = getRed(pixelColor);
    output[4] = getGreen(pixelColor);
    output[5] = getBlue(pixelColor);
    
    if (wait === 'nodelay') {
      output[6] = 0;
    } else if (wait === 'slow') {
      output[6] = 60;
    } else if (wait == 'medium') {
      output[6] = 30;
    } else {
      output[6] = 10;
    }
    device.send(output.buffer);
  };
  
  ext.colorFade = function(pixelRange, pixelColor, wait) {
    var output = new Uint8Array(7);
    output[0] = 0x0E;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    output[3] = getRed(pixelColor);
    output[4] = getGreen(pixelColor);
    output[5] = getBlue(pixelColor);

    if (wait === 'slow') {
      output[6] = 100;
    } else if (wait === 'medium') {
      output[6] = 50;
    } else {
      output[6] = 20;
    }
    device.send(output.buffer);
  };
  
  ext.sparkle = function(pixelRange, pixelColor, duration) {
    var output = new Uint8Array(8);
    var millis = Math.floor(duration * 1000);
    output[0] = 0x0D;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    output[3] = getRed(pixelColor);
    output[4] = getGreen(pixelColor);
    output[5] = getBlue(pixelColor);
    output[6] = millis >> 8;
    output[7] = millis & 0xFF;
    device.send(output.buffer);
  };
  
    ext.shimmer = function(pixelRange, pixelColor, duration) {
    var output = new Uint8Array(8);
    var millis = Math.floor(duration * 1000);
    output[0] = 0x02;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    output[3] = getRed(pixelColor);
    output[4] = getGreen(pixelColor);
    output[5] = getBlue(pixelColor);
    output[6] = millis >> 8;
    output[7] = millis & 0xFF;
    device.send(output.buffer);
  };
  
  ext.rainbow = function(pixelRange, wait, duration) {
    var output = new Uint8Array(6);
    var millis = Math.floor(duration * 1000);
    output[0] = 0x04;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    if (wait === 'slow') {
      output[3] = 60;
    } else if (wait === 'medium') {
      output[3] = 30;
    } else {
      output[3] = 10;
    }
    output[4] = (millis & 0xFF00) >> 8;
    output[5] = millis & 0xFF;
    device.send(output.buffer);
  };
  
  ext.theatreChase = function(pixelRange, pixelColor, wait, duration) {
    var output = new Uint8Array(9);
    var millis = Math.floor(duration * 1000);
    output[0] = 0x06;
    output[1] = getStartPixel(pixelRange);
    output[2] = getEndPixel(pixelRange);
    output[3] = getRed(pixelColor);
    output[4] = getGreen(pixelColor);
    output[5] = getBlue(pixelColor);
    if (wait === 'slow') {
      output[6] = 150;
    } else if (wait === 'medium') {
      output[6] = 100;
    } else {
      output[6] = 50;
    }
    output[7] = millis >> 8;
    output[8] = millis & 0xFF;
    device.send(output.buffer);
  };
  
  ext.setBrightness = function(brightness) {
    var output = new Uint8Array(2);
    output[0] = 0x0C;
    output[1] = brightness;
    device.send(output.buffer);
  };
  
  ext.startRecording = function() {
    var output = new Int8Array(1);
    output[0] = 0x08;
    device.send(output.buffer);
  };
  
  ext.stopRecording = function() {
    var output = new Int8Array(1);
    output[0] = 0x09;
    device.send(output.buffer);
  };
  
  ext.playbackRecording = function() {
    var output = new Int8Array(1);
    output[0] = 0x0A;
    device.send(output.buffer);
  };
  
  ext.pixelsForRing = function(ringName) {
    switch (ringName) {
      case 'outer ring':
        return '0,23';
      case 'middle ring':
        return '24,39';
      case 'inner ring':
        return '41,46';
      case 'all pixels':
        return '0,46';
      case 'center pixel':
        return '40,40';
    }
  };
  
  ext.pixelsForInterval = function(start, end) {
    return '' + start + ',' + end;
  }
  
  ext.stringForColor = function(colorName) {
    switch (colorName) {
      case 'off':
        return '0,0,0';
      case 'red':
        return '255,0,0';
      case 'green':
        return '0,255,0';
      case 'blue':
        return '0,0,255';
      case 'white':
       return '255,255,255';
      case 'yellow':
       return '255,255,0';
      case 'cyan':
       return '0,255,255';
      case 'magenta':
       return '255,0,255';
      case 'orange':
       return '255,165,0';
      case 'pink':
       return '255,192,203';
    }  
  }
  
  ext.stringForRGB = function(red, green, blue) {
   return '' + red + ',' + green + ',' + blue; 
  }
  
  ext._getStatus = function() {
    if (!connected)
      return { status:1, msg:'NeoPixel Disconnected' };
    else
      return { status:2, msg:'NeoPixel Connected' };
  };

  ext._deviceRemoved = function(dev) {
    // Not currently implemented with serial devices
  };

  var poller = null;
  
  var potentialDevices = [];
  ext._deviceConnected = function(dev) {
   console.log('Device ' + dev.id + ' discovered.');
   if (dev.id == 'COM1') {
     console.log('Skipping device: COM1');
    return;
   }
   potentialDevices.push(dev);

   if (!device) {
    tryNextDevice();
   }
  };

  function tryNextDevice() {
   // If potentialDevices is empty, device will be undefined.
   // That will get us back here next time a device is connected.
   device = potentialDevices.shift();
   if (!device) return;
   
    console.log('Attempting to connect to device: ' + device.id);
    sendAttempts = 0;
    device.open({ stopBits: 0, bitRate: 38400, ctsFlowControl: 0 });
    device.set_receive_handler(function(data) {
      console.log('Received response from device: ' + device.id);
      sendAttempts = 0;
      connected = true;
    }); 

    poller = setInterval(function() {

      /* TEMPORARY WORKAROUND
         Since _deviceRemoved is not
         called while using serial devices */
      if (sendAttempts >= 2) {
        console.log('Maximum ping attempts exceeded for device: ' + device.id);
        connected = false;
        device.close();
        device = null;
        clearInterval(poller);
        return;
      }
      
      sendAttempts++;
      console.log('Sending ping attempt ' + sendAttempts + ' to device: ' + device.id);
      device.send(pingCmd.buffer); 

    }, 1000);

  };

  ext._shutdown = function() {
    ext.setPixels('0,36', '0,0,0');
    if (device) device.close();
    if (poller) clearInterval(poller);
    device = null;
  };

  var descriptor = {
    blocks: [
      ['r', ' %m.rings ', 'pixelsForRing', 'all pixels'],
      ['r', 'pixels %n to %n', 'pixelsForInterval', 0, 46],
      ['r', ' %m.colors ', 'stringForColor', 'red'],
      ['r', 'red %n green %n blue %n', 'stringForRGB', 255, 0, 0],
      [' ', 'turn off all pixels', 'turnOffAllPixels'],
      [' ', 'fill %s using color %s', 'setPixels', '', ''],
      [' ', 'wipe %s using color %s %m.speeds', 'colorWipe', '', '', 'fast'],
      [' ', 'fade %s to color %s %m.speeds', 'colorFade', '','', 'fast'],
      [' ', 'theatre chase %s using color %s %m.speeds for %n seconds', 'theatreChase', '','','fast', 5.0],
      [' ', 'shimmer %s using color %s for %n seconds', 'shimmer', '', '', 5.0],
      [' ', 'sparkle %s using color %s for %n seconds', 'sparkle', '', '', 5.0],
      [' ', 'rainbow %s %m.speeds for %n seconds', 'rainbow', '', 'fast', 5.0],
      [' ', 'set brightness to %n', 'setBrightness', 255],
      [' ', 'start recording', 'startRecording'],
      [' ', 'stop recording', 'stopRecording'],
      [' ', 'playback recording', 'playbackRecording'],
    ],
    menus: {
      speeds: ['slow', 'medium', 'fast'],
      rings: ['all pixels', 'outer ring', 'middle ring', 'inner ring', 'center pixel'],
      colors: ['off', 'red', 'green', 'blue', 'white', 'yellow', 'cyan', 'magenta', 'orange', 'pink']
    },  
    url: 'http://camstevens.github.io/ScratchNeoPixels'
  };

  ScratchExtensions.register('ScratchNeoPixels', descriptor, ext, {type:'serial'});

})({});
