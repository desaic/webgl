function loadFile(url, data, callback, errorCallback) {
    // Set up an asynchronous request
    var request = new XMLHttpRequest();
    //synchronous
    request.open('GET', url, false);

    // Hook the event that gets called as the request progresses
    request.onreadystatechange = function () {
        // If the request is "DONE" (completed or failed)
        if (request.readyState == 4) {
            // If we got HTTP status 200 (OK)
            if (request.status == 200) {
                callback(request.responseText, data)
            } else { // Failed
                errorCallback(url);
            }
        }
    };

    request.send(null);    
}

function loadFiles(urls, errorCallback) {
    var numUrls = urls.length;
    var numComplete = 0;
    var result = new Object();

    // Callback for a single file
    function partialCallback(text, urlIndex) {
        result[urlIndex] = text;
        numComplete++;
    }

    for (var i = 0; i < numUrls; i++) {
      loadFile(urls[i], i, partialCallback, errorCallback);
    }
    return result;
}
