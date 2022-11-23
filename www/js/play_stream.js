window.onload = setup;

var player;

const streamNameInput = document.getElementById("stream-name");
const streamPlayer = document.getElementById("stream-player");
const enableWorkerInput = document.getElementById("enable-worker");
const enableBufferInput = document.getElementById("enable-buffer");
const bufferSizeInput = document.getElementById("buffer-size");

function setup() {
    let hash = window.location.hash;
    streamNameInput.value = decodeURI(hash.substring(1, hash.length));
    let form = document.getElementById("stream-setup");
    form.onsubmit = startPlayer;
    let bufferSizeInput = document.getElementById("buffer-size");
    bufferSizeInput.nextElementSibling.value = bufferSizeInput.value + "kB";
}

function startPlayer(e) {
    if (typeof e != typeof undefined) {
        e.preventDefault();
    }
    playStream(streamNameInput.value, true);
}

function playStream(streamName) {
    cleanupPlayer();

    fetch(web_url + "/api/stream/" + streamName)
    .then(response => {
        if (response.ok) {
            if (mpegts.getFeatureList().mseLivePlayback) {
                window.location.hash = streamName;
                var videoElement = streamPlayer;
                player = mpegts.createPlayer({
                    type: "m2ts",
                    isLive: true,
                    url: web_url + "/stream/" + streamName,
                    lazyLoad: false,
                    liveBufferLatencyChasing: true,
                    liveBufferLatencyMinRemain: 0.0,
                    enableWorker: enableWorkerInput.checked,
                    enableStashBuffer: enableBufferInput.checked,
                    stashInitialSize: bufferSizeInput.value * 1000
                });

                player.on(mpegts.Events.ERROR, e => {
                    if (e == mpegts.ErrorTypes.NETWORK_ERROR) {
                        playStream(streamName);
                    }
                });

                player.attachMediaElement(videoElement);
                player.load();
                player.play();
            }
        }
    });
}

function cleanupPlayer() {
    if (player) {
        player.pause();
        player.detachMediaElement();
        player.unload();
        player.destroy();
        player = null;
    }
}
