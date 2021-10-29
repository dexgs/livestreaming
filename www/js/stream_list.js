setInterval(getActiveStreams, 5000);
getActiveStreams();

function getActiveStreams() {
    fetch(web_url + "/api/streams/20")
    .then(response => {
        if (response.ok) {
            response.json().then(list => showActiveStreams(list));
        } else {
            showActiveStreams([]);
        }
    });
}

const streamList = document.getElementById("stream-list");
const streamListContainer = document.getElementById("stream-list-container");
const streamListItemTemplate = document.getElementById("stream-list-item-template");

function showActiveStreams(list) {
    // Do not change the stream list while it is being hovered to avoid
    // changing the list right before it gets clicked
    if (streamListContainer.matches(':hover')) {
        return;
    }

    while (streamList.firstChild) {
        streamList.removeChild(streamList.lastChild);
    }

    if (list.length == 0) {
        streamListContainer.style.display = "none";
    } else {
        streamListContainer.style.display = "unset";
    }

    list.forEach(name => {
        let clone = streamListItemTemplate.content.cloneNode(true);

        let button = clone.querySelector("BUTTON");
        let srt = clone.querySelector(".srt-link");
        let http = clone.querySelector(".http-link");

        http.href = web_url + "/stream/" + name;
        http.title = http.href;

        srt.href = srt_url + "?streamid=" + name;
        srt.title = srt.href;

        button.innerHTML = name;
        button.title = "Watch " + name;
        button.onclick = () => {
            streamNameInput.value = name;
            playStream();
        };
        streamList.appendChild(clone);
    });
}
