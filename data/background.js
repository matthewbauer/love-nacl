chrome.app.runtime.onLaunched.addListener(function(launchData) {
  if (launchData && launchData.items && launchData.items[0]) {
    chrome.app.window.create('love.html', {}, function(window) {
      window.contentWindow.launchData = launchData;
    });
  } else {
    chrome.app.window.create('test.html', {innerBounds: {width: 400, height: 640}});
  }
});
