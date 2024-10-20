/* websocket instance variable */
let cntSock = null;

/* Every 3s, attempt to open the websocket */
setInterval(() => { openCount(); }, 3000 );

/* 
If the websocket is closed, open it and attach an onmessage event that updates 
the counter display value.
*/
function openCount() {
	if (!cntSock) {
		cntSock = new WebSocket("/count");
		cntSock.onmessage = (event) => {
			if (event.data != null) {
				const data = JSON.parse(event.data);
				if (data.count != null) {
					document.getElementById("cntValue").innerText = data.count;
					console.log(`count: ${data.count}`);
				}
			}
		};
		cntSock.onclose = (event) => {
			cntSock = null;
		}
	}
}

/* Send a websocket message requesting that the counter be reset. */
function zeroCount() {
	cntSock.send(JSON.stringify({set:0}));
}

/* Close the websocket. */
function closeCount() {
	if (!cntSock) {
		console.log("cntSock already closed");
	}
	else {
		cntSock.close();
	}
}
