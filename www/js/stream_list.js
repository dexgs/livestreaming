setInterval(getActiveStreams, 5000);
getActiveStreams();

function getActiveStreams() {
    fetch(stream_backend_url + "/api/streams/20")
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
        console.log("nada");
        streamListContainer.style.display = "none";
    } else {
        streamListContainer.style.display = "unset";
    }

    list.forEach(name => {
        let item = document.createElement("LI");
        let button = document.createElement("BUTTON");
        button.innerHTML = name;
        button.title = name;
        button.onclick = () => {
            streamNameInput.value = name;
            playStream();
        };
        item.appendChild(button);
        streamList.appendChild(item);
    });
}
