# Getting started with websocketd

## Configuration Macros

websocketd does not require any configuration `#define` macros.

## Configuration Objects

websocketd does not require any configuration objects.

## Upgrading Routes

In order to upgrade an HTTP connection to a Websocket connection, an [HTTP route handler](./HTTPD_getting_started.md#example-websocket-upgrade-request-handler-function) must explicitly perform the upgrade operation by calling `xUpgradeToWebsocket()` as shown in the example below.

```
static BaseType_t httpCountWebsocketHandler(void *pxc) {
  return xUpgradeToWebsocket(pxc, xSocketCounterMessageHandler, NULL, "/count");
}
```

Note that the last argument passed to `xUpgradeToWebsocket()`, the string `"/count"` in the example above, must correspond with the value assigned to the `pcPath` property of the `SocketCounterActionArgs_t` instance `xSelectArg` in the service task `prvSocketCounter_Service()` in the example at [Service Tasks](#service-tasks) below.

If the upgrade succeeds:

* The client object is changed from an `HTTPClient_t` to a `WebsocketClient_t`;
* The `pxTxtHandler` and `pxBinHandler` message handlers are set for the client object; and
* A string `pcRoute` that can be used to identify the client for the purpose of sending messages to it will be set for the client object.

## Message Handlers

Message handlers are functions with the signature `BaseType_t (*)(void *pxc)`. Separate handlers may be defined for text and binary messages.  Message handlers are passed to the upgrade handler, which stores them as properties of the websocket client object.

The following example demonstrates how a websocket message handler can be used to get or set the current value of a global unsigned integer `prvCount`. Note that the example has a dependency on [coreJSON](https://github.com/FreeRTOS/coreJSON/tree/main).

```
BaseType_t xSocketCounterMessageHandler(void *pxc) {
  WebsocketClient_t *pxClient = (WebsocketClient_t*) pxc;
  char *pcFieldVal;
  size_t xFieldSz;
  JSONTypes_t xFieldType;
  JSONStatus_t xStatus;
  xStatus = JSON_Validate(pxClient-> pcPayload, pxClient->xPayloadSz);
  if (xStatus != JSONSuccess)
    return -1;
  xStatus = JSON_SearchT(pxClient-> pcPayload, pxClient->xPayloadSz, "get", 3, &pcFieldVal, &xFieldSz, &xFieldType);
  if (xStatus == JSONSuccess) {
    char msg[32];
    return xSendWebsocketTextMessage(pxc, msg, snprintf(msg, sizeof(msg), "{\"count\":%d}", prvCount));
  }
  xStatus = JSON_SearchT(pxClient-> pcPayload, pxClient->xPayloadSz, "set", 3, &pcFieldVal, &xFieldSz, &xFieldType);
  if (xStatus == JSONSuccess && xFieldType == JSONNumber) {
    prvCount = atoi(pcFieldVal);
    return 0;
  }
  return 0;
}
```
## Service Tasks

A websocket service task is a FreeRTOS task that pushes content to any connected websocket clients.

Following is an example of defining and starting a simple service task that periodically increments the value of a global unsigned integer `prvCount` then pushes the value to connected websocket clients as JSON-encoded websocket text messages.

```
typedef struct {
  const char * pcPath;
  char * pcMsg;
  size_t xMsgLen;
} SocketCounterActionArgs_t;

static void prvSocketCounter_Service(void *pxArgs);
static BaseType_t prvUpdateWebSockets(void *pxc, void *pxArgs);

static UBaseType_t prvCount = 0;
static TaskHandle_t xPID;

...
// Start the socket counter service task
  xTaskCreate(prvSocketCounter_Service, (const char*)"socketCounter", 512, 0, tskIDLE_PRIORITY, &xPID);
...

static void prvSocketCounter_Service(void *pvArgs) {
  char pcMsg[32];
  SocketCounterActionArgs_t xSelectArg = { "/count", pcMsg, 0 };
  while (true) {
    xSelectArg.xMsgLen = snprintf(pcMsg, sizeof(pcMsg), "{\"count\":%d}", prvCount);
    Ember_SelectClients(prvUpdateWebSockets, &xSelectArg);
    vTaskDelay(1000);
    prvCount++;
  }
}

static BaseType_t prvUpdateWebSockets(void *pxc, void *pvArg) {
  WebsocketClient_t *pxClient = (WebsocketClient_t*)pxc;
  SocketCounterActionArgs_t *pxSockArgs = (SocketCounterActionArgs_t*)pxArgs;
  if ((pxClient->xWork == xWebsocketWork) && (strcasecmp(pxClient->pcRoute, pxSockArgs->pcPath) == 0)) {
    return xSendWebsocketTextMessage(pxc, pxSockArgs->pcMsg, pxSockArgs->xMsgLen);
  }
  return 0;
}
```
