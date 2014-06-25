var global = this;
var enabledIcon = {'path': {'19': 'nyu19_on.png', '38': 'nyu38_on.png'}};
var disabledIcon = {'path': {'19': 'nyu19.png', '38': 'nyu38.png'}};
var connectedPorts = [];
var settings = {
  enabled: true,
  textColor: '#fff',
  backgroundColor: '#000',
};

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  chrome.app.window.create('hawkthorne.html', {innerBounds: {width: 1056, height: 700}});
});

chrome.runtime.onConnect.addListener(onPortConnected);

function onPortConnected(port) {
  console.log('onConnect called with port: ' + port.name);
  global.connectedPorts.push(port);

  var onDisconnect = function() {
    port.onMessage.removeListener(onMessage);
    port.onDisconnect.removeListener(onDisconnect);
    var index = global.connectedPorts.indexOf(port);
    if (index !== -1)
      global.connectedPorts.splice(index, 1);
  };

  var onMessage = function(message, sender) {
    console.log('onMessage called with message: ' + JSON.stringify(message));
    // Ignore outgoing messages.
    if (message.sender === 'background')
      return;

    var cmd = message.cmd;
    if (!cmd) {
      console.log('Message received with no command.');
      return;
    }

    var messageHandlerName = 'onMessage_' + cmd;
    var fn = global[messageHandlerName];
    if (typeof fn !== 'function') {
      console.log('Unknown message received: ' + cmd);
      return;
    }

    fn(message.data, sender);
  };

  port.onMessage.addListener(onMessage);
  port.onDisconnect.addListener(onDisconnect);
  postSettingsToPort(port, global.settings);
};

///
/// Message handlers
///
function onMessage_setSettings(data, sender) {
  setSettings(data);
}

function onMessage_toggleEnabled(data, sender) {
  toggleEnabled();
}

function onMessage_popupConnected(data, sender) {
  hideBadge();
}

///
/// Settings stuff
///
function loadSettings() {
  chrome.storage.sync.get('settings', function(data) {
    if (chrome.runtime.lastError) {
      console.log('Error loading settings: ' +
                  chrome.runtime.lastError.message);
      return;
    }

    if (!data.settings) {
      // If there is no data there yet, just use the defaults defined above.
      data.settings = global.settings;
    }

    // Force the settings to be updated.
    setSettings(data.settings, true, true);
  });
}

function saveSettings() {
  chrome.storage.sync.set({settings: global.settings}, function() {
    if (chrome.runtime.lastError) {
      console.log('Error saving settings: ' +
                  chrome.runtime.lastError.message);
      return;
    }
  });
}

function setSettings(newSettings, noSave, force) {
  var changed = false;
  var changedSettings = {};

  for (var key in newSettings) {
    var newValue = newSettings[key];
    var oldValue = global.settings[key];
    if (force || oldValue !== newValue) {
      changed = true;
      changedSettings[key] = newValue;
      global.settings[key] = newValue;
      var changedHandlerName = 'onSettingChanged_' + key;
      var fn = global[changedHandlerName];
      if (typeof fn === 'function')
        fn(newValue, oldValue);
    }
  }

  if (changed) {
    postSettingsToAllPorts(changedSettings);
    if (!noSave)
      saveSettings();
  }
}

function postSettingsToPort(port, changedSettings) {
  console.log('Sending settings to port: ' + port.name);
  port.postMessage({
    cmd: 'setSettings',
    data: changedSettings,
    sender: 'background'
  });
}

function postSettingsToAllPorts(changedSettings) {
  for (var i = 0; i < global.connectedPorts.length; ++i) {
    var port = global.connectedPorts[i];
    postSettingsToPort(port, changedSettings);
  }
}

function toggleEnabled() {
  setSettings({enabled: !global.settings.enabled});
  return global.settings.enabled;
}

