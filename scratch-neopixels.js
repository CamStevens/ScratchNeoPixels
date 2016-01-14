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

  ext.setPixelColor = function(startPixel, endPixel, red, green, blue) {
    var output = new Uint8Array(6);
    output[0] = 0x02;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = red;
    output[4] = green;
    output[5] = blue;
    device.send(output.buffer);
  };
  
  ext.rainbow = function(startPixel, endPixel, wait) {
    var output = new Uint8Array(4);
    output[0] = 0x04;
    output[1] = startPixel;
    output[2] = endPixel;
    output[3] = wait;
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
      [' ', 'set pixels %n to %n to red:%n, green:%n, blue:%n', 'setPixelColor', 0, 11, 0, 0, 0],
      [' ', 'rainbox pixels %n to %n with wait %n ms', 'rainbow', 0, 11, 50],
      [' ', 'start recording', 'startRecording'],
      [' ', 'stop recording', 'stopRecording'],
      [' ', 'playback recording', 'playbackRecording'],
    ],
    url: 'http://camstevens.github.io/ScratchNeoPixels'
  };

  ScratchExtensions.register('ScratchNeoPixels', descriptor, ext, {type:'serial'});

})({});
