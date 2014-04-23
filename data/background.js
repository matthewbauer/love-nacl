chrome.app.runtime.onLaunched.addListener(function(launchData) {
  chrome.app.window.create('love.html', {}, function(window) {
    window.contentWindow.launchData = launchData;
  });
});
