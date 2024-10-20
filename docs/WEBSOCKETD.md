# Websocket Daemon

The EMBER websocketd protocol is the work of the author.

References include:
* [The Websocket Handbook](https://pages.ably.com/hubfs/the-websocket-handbook.pdf)
* [RFC 6455 The Websocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)

Supported features include:
* Accept and respond to text or binary messages.
* Respond to ping messages.
* Push text or binary messages to connected clients; message pushes may be triggered by any FreeRTOS task.


See [here](./WEBSOCKETD_getting_started.md) to get started.

## Dependencies

| Library | Version |
| :-- | --: |
| EMBER | 0.0 |

## Message Size Constraints

The maximum size of a websocket message that may be transmitted or received is constrained by the amount of memory dedicated to the parent `TCPServer_t` instance's transmit `pcSndBuff` and receive `pcRcvBuff` buffers respectively, which are set by the macros `emberTCP_SND_BUFFER_SIZE` and `emberTCP_RCV_BUFFER_SIZE` respectively, both of which default to 2048 bytes. Note that the message size constraint includes the message header, which may be up 8 bytes.

The limited message size prevents any message from using the largest size headers (16 bytes) in practice, as there is no need to define a message length greater than 65535 bytes.

## Operation

### Upgrading

When an HTTP connection is upgraded to a Websocket connection, the existing `HTTPClient_t` instance is changed to a `WebsocketClient_t` instance at the same memory location. This is possible because:

* In a OO sense (in as much as C can be made to behave in an OO way), `HTTPClient_t` and `WebsocketClient_t` both inherit from `TCPClient_t` and the properties of `TCPClient_t` are at the same relative memory locations for both child classes.
* A `WebsocketClient_t` (192 bytes) is considerably smaller than a `HTTPClient_t` (468 bytes), so can overwrite it without overflowing its memory bounds.

During the upgrade process, the `TCPClient_t` common properties `xCreator`, `xWorker` and `xDelete` of the client object are altered to point to the corresponding websocketd functions. However, the `uxClientSz` common property is not, as the actual size of the client object has not changed.  Additionally, the route from which the websocket was derived is copied into the `pcRoute` property. This allows the websocket client instance to be identified as being associated with a particular server task during message transmission, as discussed [here](./WEBSOCKETD_getting_started.md#upgrading-routes) and [here](./WEBSOCKETD_getting_started.md#service-tasks).

### Message Reception

websocketd exposes the worker function `xWebsocketWork` that is periodically called by the EMBER task for each Websocket client connection. The worker function listens for and attempts to handle incoming Websocket messages.

Malformed messages or otherwise unhandled messages (see below) are discarded and the connection is closed.

Message types are handled as follows:

* `CONTINUE` messages are unsupported (due to the message size constraints discussed above) and will be ignored.

* If a message is either `TEXT` or `BINARY`, a corresponding handler function is executed. Handler functions have the signature `BaseType_t (*)(void*)`, where the void pointer argument is an anonymised `WebsocketClient_t` instance, and the return value is either an error (if negative) or the number of bytes transmitted in response to the request (if zero or positive). If no handler has been defined for a message type, the connection is closed with code `UNSUPPORTED DATA (1003)`.

* `PING` messages are responded to with `PONG` messages. `PONG` messages are discarded.

* `CLOSE` messages are responded to with `CLOSE` messages, and the connection is closed.

### Handling Received Messages

When a websocket connection is opened, handler functions for text and binary messages are assigned to the client object. When the client receives a message, the corresponding handler function is called.

Handler functions are simply public functions with a particular signature, as discussed [here](./WEBSOCKETD_getting_started.md#message-handlers).

### Responding to Received Messages

One of the arguments supplied during calls to message handler functions is the `WebsocketClient_t` instance that received the message. This client object can then be passed to a message transmission function in order to send a response message back to the client.

```
BaseType_t xSendWebsocketHeader(void *pxc, const eWebsocketOpcode eCode, const size_t uxPayloadSz);
BaseType_t xSendWebsocketPayload(void *pxc, const void *pvPayload, const size_t uxLen);
BaseType_t xSendWebsocketTextMessage(void *pxc, const char *pcMsg, const size_t uxLen);
BaseType_t xSendWebsocketBinaryMessage(void *pxc, const char *pcMsg, const size_t uxLen);
```

`xSendWebsocketHeader` and `xSendWebsocketPayload` can be used by a server task to construct and send any type of message, noting that the websocket header (the payload size parameter `uxPayloadSz`, in particular) and payload must match. `xSendWebsocketTextMessage` and `xSendWebsocketBinaryMessage` simplify message construction and transmission for the most common message types.

### Pushing Messages to Clients

A FreeRTOS task that interacts with users via websocket connections can [receive](#handling-received-messages) and [respond to](#responding-to-received-messages) websocket messages via a handler function, as discussed above. However, the principal value of websockets, compared to e.g. HTTP POST requests, is that they allow websocket server tasks to asynchronously transmit (or *push*) content to clients, i.e. without the client polling the server. In the context of FreeRTOS, this means that tasks other than the EMBER task must be able to asynchronously transmit messages to websocket clients.

A websocket server task can identify and transmit to connected clients using the function `void Ember_SelectClients(BaseType_t (*xSelect)(void*, void*), void *pxArg)` exposed by `ember.h`. This function takes two parameters:
  * `xSelect` is a function pointer that specifies what to do with `pxArg`; and 
  * `pxArg` is a pointer to the data that is to be passed to `xSelect`.

`xSelect` itself is executed by `Ember_SelectClients` once for each `TCPClient_t` instance that is currently connected to the EMBER server; the two `void*` arguments are the current `TCPClient_t` instance and the data supplied as `pxArg`. The list of current TCP clients is protected by a FreeRTOS mutex manage concurrency risks.

An example of how to configure `xSelect` and `pxArg` is shown [here](./WEBSOCKETD_getting_started.md#service-tasks).

Other uses may be concievably be found for `Ember_SelectClients`, but it was designed to allow websocket server tasks to broadcast messages to the subset of dependent Websocket clients.  The server task provides both `xSelect` and `pxArg`, allowing it to iterate over the set of current TCP clients, identify websocket clients that "belong" to it, and transmit messages to only those clients using the [websocket message transmission functions](#responding-to-received-messages).
