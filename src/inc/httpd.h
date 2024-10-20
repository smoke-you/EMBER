/*
 * Copyright (C) 2024 Mark R. Turner.  All Rights Reserved.
 *
 * The Ember ("EMBedded c webservER") server code is based on the FreeRTOS Labs
 * TCP protocols example at
 * https://github.com/FreeRTOS/FreeRTOS/blob/main/FreeRTOS-Plus/Demo/Common/Demo_IP_Protocols/Common/FreeRTOS_TCP_server.c
 * (and associated directories).
 *
 * For that reason, the FreeRTOS licence is reproduced below.  However, the
 * reader should be aware that the author has undertaken considerable additional
 * work to extend both the core TCP server and the protocol implementations.
 *
 * In any case, the additional work is released under the same MIT licence as the
 * FreeRTOS Labs demonstration code.
 *
 * ===============================================================================
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 * ===============================================================================
 *
 * MIT Licence
 * ============
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef EMBER_V0_0_INC_HTTPD_H_
#define EMBER_V0_0_INC_HTTPD_H_

/*===============================================
 includes
 ===============================================*/

#include "./ember_private.h"
#include "./websocketd.h"

/*===============================================
 public constants
 ===============================================*/

#define HTTPD_ROUTE_TERMINATOR    ((const char const *)0xffffffff)
#define HTTPD_CLIENT_SZ           (sizeof(HTTPClient_t))
#define HTTPD_CREATOR_METHOD      (NULL)
#define HTTPD_WORKER_METHOD       (xHttpWork)
#define HTTPD_DELETE_METHOD       (xHttpDelete)

/*===============================================
 public data prototypes
 ===============================================*/

/**
 * @struct xHTTP_HEADER_DESC
 * @brief Mapping between a received HTTP header and a "header of interest"
 *   defined in a private array `pxRcvdHeaderDescs` in `httpd.c`
 */
struct xHTTP_HEADER_DESC {
	BaseType_t xDescriptor;
	char *pcValue;
};

/**
 * @struct xHTTP_CLIENT
 * @brief HTTP client data record. Inherits from `TCPClient_t` via the
 *   `TCP_CLIENT_PROPERTIES` macro.
 */
struct xHTTP_CLIENT {
	TCP_CLIENT_PROPERTIES
		;
	/* --- Keep at the top  --- */
	char *pcUrlData;
	size_t uxBytesLeft;
	FF_FILE *pxFileHandle;
	union {
		struct {
			unsigned bReplySent :1;
		};
		uint32_t ulFlags;
	} bits;
	BaseType_t xHttpVerb;
	const char pcCurrentFilename[ffconfigMAX_FILENAME];
	const char pcWorkingUrl[ffconfigMAX_FILENAME];
	const char *pcRouteParts[emberHTTP_ROUTE_PARTS];
	const char *pcParamParts[emberHTTP_PARAM_PARTS];
	struct xHTTP_HEADER_DESC pxHeaders[emberHTTP_HEADER_PARTS];
	char *pcBody;
	BaseType_t uxBodySz;
};
typedef struct xHTTP_CLIENT HTTPClient_t;

/**
 * @enum eHttpStatus
 * @brief Enumeration of HTTP response status codes.
 */
typedef enum {
	eHTTP_SWITCHING_PROTOCOLS = 101,  /**< eHTTP_SWITCHING_PROTOCOLS */
	eHTTP_REPLY_OK = 200,             /**< eHTTP_REPLY_OK */
	eHTTP_NO_CONTENT = 204,           /**< eHTTP_NO_CONTENT */
	eHTTP_BAD_REQUEST = 400,          /**< eHTTP_BAD_REQUEST */
	eHTTP_UNAUTHORIZED = 401,         /**< eHTTP_UNAUTHORIZED */
	eHTTP_NOT_FOUND = 404,            /**< eHTTP_NOT_FOUND */
	eHTTP_NOT_ALLOWED = 405,          /**< eHTTP_NOT_ALLOWED */
	eHTTP_GONE = 410,                 /**< eHTTP_GONE */
	eHTTP_PRECONDITION_FAILED = 412,  /**< eHTTP_PRECONDITION_FAILED */
	eHTTP_PAYLOAD_TOO_LARGE = 413,    /**< eHTTP_PAYLOAD_TOO_LARGE */
	eHTTP_HEADER_TOO_LARGE = 431,     /**< eHTTP_HEADER_TOO_LARGE */
	eHTTP_INTERNAL_SERVER_ERROR = 500,/**< eHTTP_INTERNAL_SERVER_ERROR */
} eHttpStatus;

/**
 * @struct xHTTP_STATUS_DESC
 * @brief Mapping of HTTP status code enumerated at `eHttpStatus` to
 *   corresponding text description.
 */
struct xHTTP_STATUS_DESC {
	BaseType_t uxLen;
	const char *const pcText;
	BaseType_t pcStatus;
};
typedef struct xHTTP_STATUS_DESC HttpStatusDescriptor_t;
static const HttpStatusDescriptor_t xHttpStatuses[] = {
    { 19, "switching protocols", eHTTP_SWITCHING_PROTOCOLS },
    { 2, "OK", eHTTP_REPLY_OK },
    { 10, "no content", eHTTP_NO_CONTENT },
    { 11, "bad request", eHTTP_BAD_REQUEST },
    { 14, "not authorized", eHTTP_UNAUTHORIZED },
    { 9, "not found", eHTTP_NOT_FOUND },
    { 11, "not allowed", eHTTP_NOT_ALLOWED },
    { 5, "gone!", eHTTP_GONE },
    { 19, "precondition failed", eHTTP_PRECONDITION_FAILED },
    { 17, "payload too large", eHTTP_PAYLOAD_TOO_LARGE },
    { 17, "headers too large", eHTTP_HEADER_TOO_LARGE },
    { 21, "internal server error", eHTTP_INTERNAL_SERVER_ERROR },
    { 0, "", -1 },
};
static const size_t xNumHttpStatusDescs = sizeof(xHttpStatuses)
    / sizeof(HttpStatusDescriptor_t);
static const HttpStatusDescriptor_t *pxDefaultHttpStatus =
    &xHttpStatuses[xNumHttpStatusDescs - 1];

/**
 * @enum eHttpVerb
 * @brief Enumeration of HTTP verbs.
 */
typedef enum {
	eHTTP_GET = 0,/**< eHTTP_GET */
	eHTTP_HEAD,   /**< eHTTP_HEAD */
	eHTTP_POST,   /**< eHTTP_POST */
	eHTTP_PUT,    /**< eHTTP_PUT */
	eHTTP_DELETE, /**< eHTTP_DELETE */
	eHTTP_TRACE,  /**< eHTTP_TRACE */
	eHTTP_OPTIONS,/**< eHTTP_OPTIONS */
	eHTTP_CONNECT,/**< eHTTP_CONNECT */
	eHTTP_PATCH,  /**< eHTTP_PATCH */
	eHTTP_UNKNOWN,/**< eHTTP_UNKNOWN */
} eHttpVerb;

/**
 * @struct xHTTP_VERB_DESC
 * @brief Mapping of HTTP verbs enumerated at `eHttpVerb` to corresponding text
 *   description.
 */
struct xHTTP_VERB_DESC {
	size_t xLen;
	const char *const text;
	eHttpVerb xVerb;
};
typedef struct xHTTP_VERB_DESC HttpVerbDescriptor_t;
static const HttpVerbDescriptor_t xHttpVerbs[] = {
    { 3, "GET", eHTTP_GET },
    { 4, "HEAD", eHTTP_HEAD },
    { 4, "POST", eHTTP_POST },
    { 3, "PUT", eHTTP_PUT },
    { 6, "DELETE", eHTTP_DELETE },
    { 5, "TRACE", eHTTP_TRACE },
    { 7, "OPTIONS", eHTTP_OPTIONS },
    { 7, "CONNECT", eHTTP_CONNECT },
    { 5, "PATCH", eHTTP_PATCH },
    { 7, "UNKNOWN", eHTTP_UNKNOWN },
    { 0, "", -1 },
};
static const HttpVerbDescriptor_t *pxDefaultVerb =
    &xHttpVerbs[(sizeof(xHttpVerbs) / sizeof(HttpVerbDescriptor_t)) - 2];

/**
 * @enum eRouteOptions
 * @brief Enumeration of bit flag options for treatment of HTTP routes.
 */
typedef enum {
	eRouteOption_None = 0,                 /**< eRouteOption_None */
	eRouteOption_IgnoreTrailingSlash = 0x1,/**< eRouteOption_IgnoreTrailingSlash */
	eRouteOption_AllowWildcards = 0x2,     /**< eRouteOption_AllowWildcards */
} eRouteOptions;

/**
 * @enum eResponseOptions
 * @brief Enumeration of bit flag options for construction of HTTP headers for
 *   transmission.
 */
typedef enum {
	eResponseOption_None = 0,         /**< eResponseOption_None */
	eResponseOption_ContentLength = 1,/**< eResponseOption_ContentLength */
	eResponseOption_ChunkedBody = 2,  /**< eResponseOption_ChunkedBody */
} eResponseOptions;

/**
 * @union xRESPONSE_OPTIONS
 * @brief Mapping of `eResponseOptions` bit flags to struct bitfields.
 */
union xRESPONSE_OPTIONS {
	UBaseType_t ulValue;
	struct {
		unsigned content_length :1;
		unsigned chunked_body :1;
	};
};
typedef union xRESPONSE_OPTIONS ResponseOptions_t;

/**
 * @fn BaseType_t (*xRouteHandler)(void*)
 * @brief Signature for HTTP route handler functions.
 */
typedef BaseType_t (xRouteHandler)(void*);
/**
 * @fn BaseType_t (*xErrorHandler)(void*, eHttpStatus)
 * @brief Signature for HTTP error handler functions
 */
typedef BaseType_t (xErrorHandler)(void*, eHttpStatus);

/**
 * @struct xROUTE_ITEM
 * @brief Description of an individual HTTP route.
 */
struct xROUTE_ITEM {
	union {
		UBaseType_t uxValue;
		struct {
			unsigned ignore_trailing_slash :1;
			unsigned allow_wildcards :1;
			unsigned :30;
		};
	} uxOptions;
	xRouteHandler *pxHandler;
	const char const *const*pcPath;
};
typedef struct xROUTE_ITEM RouteItem_t;

/**
 * @struct xROUTE_CONFIG
 * @brief Collection of HTTP routes and related information.
 */
struct xROUTE_CONFIG {
	const char const *pcDelims;
	const size_t uxNumRoutes;
	const RouteItem_t const *pxItems;
	xErrorHandler *pxErrorHandler;
};
typedef struct xROUTE_CONFIG RouteConfig_t;

/*===============================================
 public function prototypes
 ===============================================*/

/**
 * @fn BaseType_t xHttpWork(void*)
 * @brief HTTP client work function. Called periodically by EMBER for each
 *   `HTTPClient_t` instance in its list of current `TCPClient_t` instances.
 *
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xHttpWork(void *pxc);

/**
 * @fn BaseType_t xHttpDelete(void*)
 * @brief HTTP client deletion function. Called by EMBER to close any files
 *   opened by an `HTTPClient_t` instance.
 *
 * @post The `HTTPClient_t` instance's files are closed.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @return 0
 */
BaseType_t xHttpDelete(void *pxc);

/**
 * @fn const HttpStatusDescriptor_t* const pxGetHttpStatusMessage (const BaseType_t)
 * @brief Find a status descriptor (text) corresponding to an HTTP status code.
 *
 * @param xStatus The status code to search for
 * @return A pointer to an `HttpStatusDescriptor_t` that includes the status text.
 */
const HttpStatusDescriptor_t* const pxGetHttpStatusMessage(
    const BaseType_t xStatus);

/**
 * @fn size_t xPrintRoute(char*, const char**, const size_t)
 * @brief Print a formatted HTTP route.
 *
 * @param pcDest The location to print the route.
 * @param pxRouteParts An array of strings making up the parts of the route,
 *   terminated by a `HTTPD_ROUTE_TERMINATOR`. The terminator is not printed.
 * @param uxn The maximum number of characters to print.
 * @return The actual number of characters printed.
 */
size_t xPrintRoute(char *pcDest, const char **pxRouteParts, const size_t uxn);

/**
 * @fn size_t xPrintParams(char*, const char**, const size_t)
 * @brief Print formatted HTTP route parameters.
 *
 * @param pcDest The location to print the parameters.
 * @param pxParamParts An array of strings making up the parameters, terminated
 *   by a `HTTPD_ROUTE_TERMINATOR`. The terminator is not printed.
 * @param uxn The maximum number of characters to print.
 * @return The actual number of characters printed.
 */
size_t xPrintParams(char *pcDest, const char **pxParamParts, const size_t uxn);

/**
 * @fn const char* pcGetContentsType(const char*)
 * @brief Given a file extension, select a content type for transmission in an
 *   HTTP header.
 *
 * @param pcFileExt The file extension.
 * @return A char* to an HTTP content type string.
 */
const char* pcGetContentsType(const char *pcFileExt);

/**
 * @fn BaseType_t xSendHttpResponseHeaders(void*, const BaseType_t,
 *   const UBaseType_t, const size_t, const char*, const char*)
 * @brief Construct and transmit HTTP headers.
 *
 * @post The `HTTPClient_t` instance is ready to transmit an HTTP body.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @param xCode The HTTP response code.
 * @param uxOpts Header construction options.
 * @param uxLen The size (in bytes) of the expected HTTP body to follow.
 *   Relevant only if `uxOpts.content_length` is `1`. Set `uxLen` = 0 for
 *   "chunked" transmissions.
 * @param pcContentType The "Content-Type" header text.
 * @param pcExtra Extra headers, as text.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendHttpResponseHeaders(
    void *pxc,
    const BaseType_t xCode,
    const UBaseType_t uxOpts,
    const size_t uxLen,
    const char *pcContentType,
    const char *pcExtra);

/**
 * @fn BaseType_t xSendHttpResponseContent(void*, char*, size_t)
 * @brief Transmit a block of the HTTP body. Note that the total size of all
 *   transmitted blocks should match the `uxLen` parameter for the preceding
 *   `xSendHttpResponseHeaders` call.
 *
 * @pre `xSendHttpResponseHeaders` should have been called immediately prior with
 *   `uxOpts.content_length`=`1`.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @param pcContent A char* to the content to be sent.
 * @param uxLen The number of bytes of content to be sent.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendHttpResponseContent(void *pxc, char *pcContent, size_t uxLen);

/**
 * @fn BaseType_t xSendHttpResponseChunk(void*, char*, size_t)
 * @brief Transmit a chunk of the HTTP body, in "chunked" transmission mode.
 *
 * @pre `xSendHttpResponseHeaders` should have been called immediately prior with
 *   `uxOpts.chunked_body`=`1`.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @param pcContent A char* to the content to be sent.
 * @param uxLen The number of bytes of content to be sent.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendHttpResponseChunk(void *pxc, char *pcContent, size_t uxLen);

/**
 * @fn BaseType_t xSendHttpResponseFile(void*)
 * @brief Transmit a file from the filesystem. Note that the file reference is
 *   set separately, in the `HTTPClient_t` instance.
 *
 * @pre
 *   * The `HTTPClient_t` field `pxFileHandle` should have been set (and the
 *     file opened).
 *   * `xSendHttpResponseHeaders` should have been sent immediately prior with
 *   `uxOpts.content_length` equal to the size of the file.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendHttpResponseFile(void *pxc);

/**
 * @fn BaseType_t xGetHeaderValue(void*, const char*, char**)
 * @brief Given a header name, return its value if it exists.
 *
 * @pre An HTTP request has been received by the `HTTPClient_t` instance.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @param pcText The header name to be searched/checked.
 * @param pcValue The location to store a pointer to the header value.
 * @return The index of the corresponding header part in the TCPClient_t
 *   instance, or -1 if it was not found.
 */
BaseType_t xGetHeaderValue(void *pxc, const char *pcText, char **pcValue);

/**
 * @fn BaseType_t xUpgradeToWebsocket(void*, const WebsocketMessageHandler_t,
 *   const WebsocketMessageHandler_t, const char*)
 * @brief Upgrade an `HTTPClient_t` instance to a `WebsocketClient_t` and set
 *   several the fields of the new object.
 *
 * @post
 *   * The de-anonymized `pxc` argument will be changed from a `HTTPClient_t`
 *     to a `WebsocketClient_t`.
 *   * The `xCreator`, `xWork` and `xDelete` properties of the new object will
 *     be set to the corresponding websocketd functions.
 *   * The `pxTxtHandler`, `pxBinHandler` and `pcRoute` properties of the new
 *     object will be set to the corresponding parameters supplied to this
 *     function.
 * @param pxc An anonymized `HTTPClient_t` instance.
 * @param txtHandler The handler for text messages received by the newly
 *   upgraded websocket connection.
 * @param binHandler The handler for binary messages received by the newly
 *   upgraded websocket connection.
 * @param pcRoute A string that can be used by websocket service threads to
 *   identify websocket connections that have been opened to them.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xUpgradeToWebsocket(
    void *pxc,
    const WebsocketMessageHandler_t txtHandler,
    const WebsocketMessageHandler_t binHandler,
    const char *pcRoute);

#endif /* EMBER_V0_0_INC_HTTPD_H_ */
