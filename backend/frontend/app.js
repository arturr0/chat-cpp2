const protocol = location.protocol === "https:" ? "wss:" : "ws:";
const ws = new WebSocket(`${protocol}//${location.host}`);

const chat = document.getElementById("chat");

ws.onmessage = (e) => {
    const div = document.createElement("div");
    div.textContent = e.data;
    chat.appendChild(div);
};

function send() {
    const input = document.getElementById("msg");
    if (!input.value) return;

    ws.send(input.value);
    input.value = "";
}
