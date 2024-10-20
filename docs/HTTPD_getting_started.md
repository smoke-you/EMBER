# Getting started with httpd

## Configuration Macros

The following macros are used by httpd. User-defined values may be set in the configuration header file `ember_config.h`. If no values are defined for these macros in `ember_config.h` then the default values shown below will be used.

| #define | Default | Explanation |
| :-- | --: | :-- |
| emberHTTP_ROUTE_PARTS | 9 | The (maximum + 1) number of request URL parts that can make up a route |
| emberHTTP_PARAM_PARTS | 9 | The (maximum + 1) number of parameters in the request URL |
| emberHTTP_HEADER_PARTS | 10 | The (maximum + 1) number of headers (of interest) in the HTTP request |

## Configuration Objects

A configuration object, `xRouteConfig`, is used to describe HTTP routes and their handlers. If a public object of this name is not defined by the user, e.g. in `ember_config.c`, then a very limited default object (that simply returns a 404 error to all connection attempts) will be used instead.

`xRouteConfig` is an instance of `RouteConfig_t`:

| Field Name | Description |
| --- | --- |
| `pcDelims` | A string containing each of the characters that will be recognized as a route delimiter, typically `"/\\"` |
| `uxNumRoutes` | The number of recognized routes, i.e. entries in `pxItems` |
| `pxItems` | An array of `RouteItem_t` |
| `pxErrorHandler` | The handler that will be called if an HTTP request fails. The function signature is `BaseType_t (*)(void*, eHttpStatus)`, where the first parameter is a pointer to an `HttpClient_t` instance and the second is the HTTP return code. |

`RouteItem_t` instances are made up of:

| Field Name | Description |
| --- | --- |
| `uxOptions` | Options flags (see below). |
| `pxHandler` | The handler that will be called if the route's path matches an incoming HTTP request's path. The function signature is `BaseType_t (*)(void*)`, where the parameter is a pointer to an `HttpClient_t` instance. |
| `pcPath` | A pointer to an array of null-terminated strings. The array is terminated by a char pointer with a value of `HTTPD_ROUTE_TERMINATOR`, or `0xffffffff`. The array represents the ordered parts of the route. |

Two options are recognized:
| Option Name | Value | Description |
| --- | --: | --- |
| `ignore_trailing_slash` | `0x00000001` | Strip any trailing slash from the target path in incoming HTTP requests when matching against this route. |
| `allow_wildcards` | `0x00000002` | Allow wildcards when attempting to match incoming HTTP requests against this route. Uses `fnmatch` wildcard syntax. |

### Example Route Configuration

A simple `xRouteConfig` might look like:

```
static BaseType_t httpRootHandler(void *pxc);
static BaseType_t httpStaticHandler(void *pxc);
static BaseType_t httpRNGHandler(void *pxc);
static BaseType_t httpRTCWebsocketHandler(void *pxc);
static BaseType_t httpErrorHandler(void *pxc, eHttpStatus code);

const RouteConfig_t xRouteConfig = {
    "/\\",
    4,
    {
        {
            eRouteOption_None,
            httpRootHandler,
            (const char const*[] ) { "", HTTPD_ROUTE_TERMINATOR } ,
        },
        {
            eRouteOption_IgnoreTrailingSlash + eRouteOption_AllowWildcards,
            httpStaticHandler,
            (const char const*[] ) { "static", "%", HTTPD_ROUTE_TERMINATOR } ,
        },
        {
            eRouteOption_IgnoreTrailingSlash,
            httpRNGHandler,
            (const char const*[] ) { "rng", HTTPD_ROUTE_TERMINATOR } ,
        },
        {
            eRouteOption_IgnoreTrailingSlash,
            httpCountWebsocketHandler,
            (const char const*[] ) { "count", HTTPD_ROUTE_TERMINATOR } ,
        },
    },
    httpErrorHandler,
};
```

* Route 1 matches the root path `/`. The prototypes for the various handler functions are shown above. Simple handler examples are shown below.

* Route 2 matches any path beginning with `/static/` and could be used to serve static files, such as CSS, Javascript and images. It can include filesystem path components, e.g. `/static/js/index.js` might map to the filesystem at `/spidisk/web/static/js/index.js`.

* Route 3 matches the path `/rng` (or `/rng/`). "rng" refers to "random number generator", i.e. `GET /rng` returns dynamic content, in this case a value read from the host's random number generator.

* Route 4 matches the path `/count` (or `/count/`). `GET /count` is a request to open a websocket connection to the `/count` path.

### Example Root Path Handler Function

Following is an example of mapping a route to any HTML (or other) file in the host's filesystem.

```
static BaseType_t httpRootHandler(void *pxc) {
  HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
  size_t uxSent;
  BaseType_t xRc;
  BaseType_t xLen = 0;
  if (pxClient->xHttpVerb == eHTTP_GET) {
    pxClient->bits.ulFlags = 0;
    strncpy(pxClient->pxParent->pcContentsType, "text/html",
        sizeof(pxClient->pxParent->pcContentsType));
    pxClient->pxFileHandle = ff_fopen("/spidisk/web/static/index.htm", "r");
    if (pxClient->pxFileHandle == 0)
      return httpErrorHandler(pxc, eHTTP_NOT_FOUND);
    xRc = xSendHttpResponseHeaders(pxClient, eHTTP_REPLY_OK,
        eResponseOption_ContentLength, pxClient->pxFileHandle->ulFileSize,
        "text/html", 0);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    xRc = xSendHttpResponseFile(pxClient);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    return xLen;
  }
  else {
    return httpErrorHandler(pxc, eHTTP_NOT_ALLOWED);
  }
}
```

* If the HTTP request verb is not GET, respond with HTTP_NOT_ALLOWED (405).

* If the static HTML file that maps to the root path does not exist, respond with HTTP_NOT_FOUND (404).

* Otherwise, attempt to send the HTML file.  If any internal error occurs, i.e. if any of the HTTP function calls return a negative value, return that error code. Otherwise, return the total number of bytes transmitted.

### Example Static Files Handler Function

Following is an example of mapping a route to a segment of the host's filesystem. In this case, the HTTP path `/static` is mapped to the filesystem path `/spidisk/web/static`.

```
static BaseType_t httpStaticHandler(void *pxc) {
  HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
  BaseType_t xp;
  BaseType_t xRc;
  BaseType_t xLen = 0;
  if (pxClient->xHttpVerb == eHTTP_GET) {
    xp = snprintf((char*) pxClient->pcCurrentFilename,
        sizeof(pxClient->pcCurrentFilename), "/spidisk/web/static");
    xp += xPrintRoute((char*) &pxClient->pcCurrentFilename[xp],
        &pxClient->pcRouteParts[1],
        sizeof(pxClient->pcCurrentFilename) - xp);
    pxClient->bits.ulFlags = 0;
    pxClient->pxFileHandle = ff_fopen(pxClient->pcCurrentFilename, "r");
    if (pxClient->pxFileHandle == 0)
      return httpErrorHandler(pxc, eHTTP_NOT_FOUND);
    xRc = xSendHttpResponseHeaders(pxc, eHTTP_REPLY_OK,
        eResponseOption_ContentLength,
        pxClient->pxFileHandle->ulFileSize,
        pcGetContentsType(pxClient->pcCurrentFilename), 0);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    xRc = xSendHttpResponseFile(pxc);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    return xLen;
  }
  else {
    return httpErrorHandler(pxc, eHTTP_NOT_ALLOWED);
  }
}
```
* If the HTTP request verb is not GET, respond with HTTP_NOT_ALLOWED (405).

* If the static file requested does not exist, respond with HTTP_NOT_FOUND (404).

* Otherwise, attempt to send the static file.  If any internal error occurs, i.e. if any of the HTTP function calls return a negative value, return that error code. Otherwise, return the total number of bytes transmitted.

### Example Dynamic Response Handler Function

Following is an example of responding to an HTTP request with dynamically generated content. Chunked HTTP transmission is used because the size of the response is not known when the HTTP headers are transmitted.

Note that the function `RNG_Read()` is not documented as it is not part of EMBER. It returns a 32-bit unsigned integer.

```
static BaseType_t httpRNGHandler(void *pxc) {
  HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
  BaseType_t xRc;
  BaseType_t xLen = 0;
  if (pxClient->xHttpVerb == eHTTP_GET) {
    pxClient->bits.ulFlags = 0;
    xRc = xSendHttpResponseHeaders(pxClient, eHTTP_REPLY_OK,
        eResponseOption_ChunkedBody, 0, "application/json", 0);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    // misuse the "pcCurrentFilename" field of pxClient as a string buffer for the JSON content
    xRc = xSendHttpResponseChunk(pxClient, (char*) pxClient->pcCurrentFilename,
        snprintf((char*) pxClient->pcCurrentFilename,
            sizeof(pxClient->pcCurrentFilename), "{\"rng\":\"%08x\"}",
            RNG_Read()));
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    xRc = xSendHttpResponseChunk(pxClient, 0, 0);
    if (xRc < 0)
      return xRc;
    xLen += xRc;
    return xLen;
  }
  else {
    return httpErrorHandler(pxc, eHTTP_NOT_ALLOWED);
  }
}
```

* If the HTTP request verb is not GET, respond with HTTP_NOT_ALLOWED (405).

* Otherwise, read the random number generator, format the result as an 8-digit hex value, and embed it in a JSON response.  If any internal error occurs, i.e. if any of the HTTP function calls return a negative value, return that error code. Otherwise, return the total number of bytes transmitted.

### Example Websocket Upgrade Request Handler Function

Following is an example of upgrading an incoming request to a Websocket connection.  Refer to [Getting started with Websockets](./WEBSOCKETD_getting_started.md) for additional information.

The message handler function `SocketCountMessageHandler()` is discussed further [elsewhere](./WEBSOCKETD_getting_started.md#defining-a-message-handler).

```
static BaseType_t httpCountWebsocketHandler(void *pxc) {
  return xUpgradeToWebsocket(pxc, SocketCounterMessageHandler, NULL, "/count");
}
```

* Pass the HTTP request; the websocket message handlers; and a string that can be used later to identify the websocket clients; to `xUpgradeToWebsocket()`. Return the response.

