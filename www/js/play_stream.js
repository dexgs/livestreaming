window.onload = setup;

var player;

const streamNameInput = document.getElementById("stream-name");
const streamPlayer = document.getElementById("stream-player");
const enableBufferInput = document.getElementById("enable-buffer");
const bufferSizeInput = document.getElementById("buffer-size");

function setup() {
    let hash = window.location.hash;
    streamNameInput.value = decodeURI(hash.substring(1, hash.length));
    let form = document.getElementById("stream-setup");
    form.onsubmit = playStream;
    let bufferSizeInput = document.getElementById("buffer-size");
    bufferSizeInput.nextElementSibling.value = bufferSizeInput.value + "kB";
}

var lastStreamURL = "";
function startPlayer() {
    if (streamNameInput.value != lastStreamURL) {
        playStream();
    }
}

function playStream() {
    let streamName = streamNameInput.value;

    lastStreamURL = streamName;
    fetch(stream_backend_url + "/api/stream/" + streamName)
    .then(response => {
        if (response.ok) {
            if (player) {
                player.unload();
            }
            if (mpegts.getFeatureList().mseLivePlayback) {
                window.location.hash = streamName;
                var videoElement = streamPlayer;
                player = mpegts.createPlayer({
                    type: "m2ts",
                    isLive: true,
                    url: stream_backend_url + "/stream/" + streamName,
                    lazyLoad: false,
                    enableStashBuffer: enableBufferInput.checked,
                    stashInitialSize: bufferSizeInput.value * 1000
                });
                player.attachMediaElement(videoElement);
                player.load();
                player.play();
            }
        } else {
        }
    });

    return false;
}
