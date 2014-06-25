function createModule(url) {
  // Get the manifest to find the correct nmf to use.
  var manifest = chrome.runtime.getManifest();
  var nmf = manifest.nacl_modules[0].path;

  var embed = document.createElement('embed');
  embed.setAttribute('src', nmf);
  embed.setAttribute('type', 'application/x-nacl');
  embed.setAttribute('love_src', url);
  document.body.appendChild(embed);

  // Inject code the same way as if this were a mimetype handler, to share
  // code.  Idea copied from
  // http://stackoverflow.com/questions/9515704/building-a-chrome-extension-inject-code-in-a-page-using-a-content-script/9517879#9517879
  var script = document.createElement('script');
  script.src = chrome.extension.getURL('injected.js');
  script.addEventListener('load', function () {
    this.parentNode.removeChild(this);
  });
  document.head.appendChild(script);
}

function loadFileEntry(entry) {
  entry.file(function(file){
    chrome.fileSystem.getDisplayPath(file, createModule);
  });
}

loadFileEntry(launchData.items[0].entry);
