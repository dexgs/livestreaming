window.onload = setup;

function setup() {
    let hash = window.location.hash;
    document.getElementById("stream-name").value = hash.substring(1, hash.length);
    let form = document.getElementById("stream-setup");
    form.onsubmit = playStream;
    let bufferSizeInput = document.getElementById("buffer-size");
    bufferSizeInput.nextElementSibling.value = bufferSizeInput.value + "kB";
}

var playerStarted = false;
function startPlayer() {
    if (!playerStarted) {
        playStream();
        playerStarted = true;
    }
}

function playStream() {
    if (mpegts.getFeatureList().mseLivePlayback) {
        let streamName = document.getElementById("stream-name").value;
        window.location.hash = streamName;
        var videoElement = document.getElementById("stream-player");
        var player = mpegts.createPlayer({
            type: "m2ts",
            isLive: true,
            url: stream_backend_url + "/stream/" + streamName,
            lazyLoad: false,
            enableStashBuffer: document.getElementById("enable-buffer").checked,
            stashInitialSize: document.getElementById("buffer-size").value * 1000
        });
        player.attachMediaElement(videoElement);
        player.load();
        player.play();
    }

    return false;
}
