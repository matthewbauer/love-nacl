chrome.app.runtime.onLaunched.addListener(function(launchData) {
  chrome.app.window.create('hawkthorne.html', {});
});
