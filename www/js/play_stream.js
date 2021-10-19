window.onload = setup;

function setup() {
    let hash = window.location.hash;
    document.getElementById("stream-name").value = hash.substring(1, hash.length);
    let form = document.getElementById("stream-setup");
    form.onsubmit = playStream;
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
            enableStashBuffer: false,
        });
        player.attachMediaElement(videoElement);
        player.load();
        player.play();
    }

    return false;
}
