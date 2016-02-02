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

  ext.setPixels = function(pixelRange, pixelColor) {
    var range = pixelRange.split(',');
    var colors = pixelColor.split(',');
    var startPixel = +range[0]
    var endPixel = +range[1];
    var red = +colors[0];
    var green = +colors[1];
    var blue = +colors[2];
    this.colorWipe(startPixel, endPixel, red, green, blue, 'nodelay');
  }
  
  ext.colorWipe = function(startPixel, endPixel, red, green, blue, wait) {
    var output = new Uint8Array(7);
    output[0] = 0x03;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;
    
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
  
  ext.colorFade = function(startPixel, endPixel, red, green, blue, wait) {
    var output = new Uint8Array(7);
    output[0] = 0x0E;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;

    if (wait === 'slow') {
      output[6] = 100;
    } else if (wait === 'medium') {
      output[6] = 50;
    } else {
      output[6] = 20;
    }
    device.send(output.buffer);
  };
  
  ext.sparkle = function(startPixel, endPixel, red, green, blue, duration) {
    var output = new Uint8Array(8);
    output[0] = 0x0D;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;
    output[6] = duration >> 8;
    output[7] = duration & 0xFF;
    device.send(output.buffer);
  };
  
    ext.shimmer = function(startPixel, endPixel, red, green, blue, duration) {
    var output = new Uint8Array(8);
    output[0] = 0x02;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;
    output[6] = duration >> 8;
    output[7] = duration & 0xFF;
    device.send(output.buffer);
  };
  
  ext.rainbow = function(startPixel, endPixel, wait, duration) {
    var output = new Uint8Array(6);
    output[0] = 0x04;
    output[1] = startPixel;
    output[2] = endPixel;
    if (wait === 'slow') {
      output[3] = 60;
    } else if (wait === 'medium') {
      output[3] = 30;
    } else {
      output[3] = 10;
    }
    output[4] = (duration & 0xFF00) >> 8;
    output[5] = duration & 0xFF;
    device.send(output.buffer);
  };
  
  ext.theatreChase = function(startPixel, endPixel, red, green, blue, wait, duration) {
    var output = new Uint8Array(9);
    output[0] = 0x06;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;
    if (wait === 'slow') {
      output[6] = 150;
    } else if (wait === 'medium') {
      output[6] = 100;
    } else {
      output[6] = 50;
    }
    output[7] = duration >> 8;
    output[8] = duration & 0xFF;
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
        return pixelsForInterval (0, 23);
      case 'inner ring':
        return pixelsForInterval (24, 35);
      case 'all pixels':
        return pixelsForInterval (0, 36);
      case 'center pixel':
        return pixelsForInterval (36, 36);
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
    }  
  }
  
  ext._getStatus = function() {
    if (!connected)
      return { status:1, msg:'Disconnected' };
    else
      return { status:2, msg:'Connected' };
  };

  ext._deviceRemoved = function(dev) {
    // Not currently implemented with serial devices
  };

  var poller = null;
  ext._deviceConnected = function(dev) {
    sendAttempts = 0;
    connected = true;
    if (device) return;
    
    device = dev;
    device.open({ stopBits: 0, bitRate: 38400, ctsFlowControl: 0 });
    device.set_receive_handler(function(data) {
      sendAttempts = 0;
      var inputData = new Uint8Array(data);
      processInput(inputData);
    }); 

    poller = setInterval(function() {

      /* TEMPORARY WORKAROUND
         Since _deviceRemoved is not
         called while using serial devices */
      if (sendAttempts >= 10) {
        connected = false;
        device.close();
        device = null;
        clearInterval(poller);
        return;
      }
      
      device.send(pingCmd.buffer); 
      sendAttempts++;

    }, 1000);

  };

  ext._shutdown = function() {
    ext.setPixelColor(0, 11, 0, 0, 0);
    if (device) device.close();
    if (poller) clearInterval(poller);
    device = null;
  };

  var descriptor = {
    blocks: [
      [' ', 'set %s to %s', 'setPixels', '0, 36', '0, 0, 0'],
      [' ', 'wipe pixels %n to %n to red %n, green %n, blue %n %m.speeds', 'colorWipe', 0, 11, 0, 0, 0, 'fast'],
      [' ', 'fade pixels %n to %n to red %n, green %n, blue %n %m.speeds', 'colorFade', 0, 11, 0, 0, 0, 'fast'],
      [' ', 'rainbow pixels %n to %n %m.speeds for %n ms', 'rainbow', 0, 11, 'fast', 5000],
      [' ', 'theatre chase pixels %n to %n to red %n, green %n, blue %n %m.speeds for %n ms', 'theatreChase', 0, 11, 255,0,0,'fast', 5000],
      [' ', 'shimmer pixels %n to %n to red %n, green %n, blue %n for %n ms', 'shimmer', 0, 11, 226, 121, 35, 5000],
      [' ', 'sparkle pixels %n to %n to red %n, green %n, blue %n for %n ms', 'sparkle', 0, 11, 255, 0, 0, 5000],
      [' ', 'set brightness to %n', 'setBrightness', 255],
      [' ', 'start recording', 'startRecording'],
      [' ', 'stop recording', 'stopRecording'],
      [' ', 'playback recording', 'playbackRecording'],
      ['r', '%m.rings pixels', 'pixelsForRing', 'outer ring'],
      ['r', 'pixels %n to %n', 'pixelsForInterval', 0, 36],
      ['r', 'color %m.colors', 'stringForColor', 'off'],
    ],
    menus: {
      speeds: ['slow', 'medium', 'fast'],
      rings: ['all', 'outer ring', 'inner ring', 'center'],
      colors: ['off', 'red', 'green', 'blue']
    },  
    url: 'http://camstevens.github.io/ScratchNeoPixels'
  };

  ScratchExtensions.register('ScratchNeoPixels', descriptor, ext, {type:'serial'});

})({});
