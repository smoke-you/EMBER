/*===============================================
 includes
 ===============================================*/

#include <FreeRTOS.h>
#include <FreeRTOS_IP.h>
#include <FreeRTOSIPConfig.h>
#include <fnmatch.h>
#include <strcasestr.h>
#include <sha1.h>
#include <base64.h>

#include "inc/httpd.h"
#include "inc/ember_private.h"
#include "inc/websocketd.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

struct xHTTP_RCV_HEADER_DESC {
	const char const *pcText;
};
typedef struct xHTTP_RCV_HEADER_DESC HTTPRcvdHeaderDescriptor_t;

typedef enum {
	eHTTPHeader_OK = 0,
	eHTTPHeader_Invalid = -eHTTP_BAD_REQUEST,
	eHTTPHeader_NotAllowed = -eHTTP_NOT_FOUND,
} eHTTPHeaderResult;

typedef enum {
	eHTTPBody_OK = 0,
	eHTTPBody_Invalid = -eHTTP_BAD_REQUEST,
} eHTTPBodyResult;

struct xHTTP_SND_HEADER_DESC {
	const char const *pcKey;
	const char const *pcValue;
};
typedef struct xHTTP_SND_HEADER_DESC HTTPSendHeaderDescriptor_t;

struct xTYPE_COUPLE {
	const char *pcExtension;
	const char *pcType;
};
typedef struct xTYPE_COUPLE TypeCouple_t;

/*===============================================
 private function prototypes
 ===============================================*/

static BaseType_t prvServiceRequest(HTTPClient_t *pxClient);
static BaseType_t prvDefaultErrorHandler(void *pxc, eHttpStatus xCode);
static void prvFindHTTPVerb(HTTPClient_t *pxClient);
static BaseType_t prvUrlDecode(char *pcUrl);
static void prvResolveUrlParts(HTTPClient_t *pxClient);
static BaseType_t prvFindMatchingHeader(const char *pcFind);
static BaseType_t prvResolveHeaders(HTTPClient_t *pxClient);
static BaseType_t prvResolveBody(HTTPClient_t *pxClient);
static BaseType_t prvMatchRoute(HTTPClient_t *pxClient);
static size_t prvConstructHeaders(
    char *pcDst,
    const size_t uxMaxSz,
    const BaseType_t xCode,
    const uint32_t xOpts,
    const char *pcContentType,
    const size_t uxContentLen,
    const char *pcExtra);
static BaseType_t prvSendWebsocketUpgradeHeaders(HTTPClient_t *pxc, char *pcKey);
static BaseType_t prvContinueSendFile(HTTPClient_t *pxClient);

/*===============================================
 private global variables
 ===============================================*/

static TypeCouple_t pxTypeCouples[] = {
    { "html", "text/html" },
    { "css", "text/css" },
    { "js", "text/javascript" },
    { "png", "image/png" },
    { "jpg", "image/jpeg" },
    { "gif", "image/gif" },
    { "json", "application/json" },
    { "txt", "text/plain" },
    { "mp3", "audio/mpeg3" },
    { "wav", "audio/wav" },
    { "flac", "audio/ogg" },
    { "pdf", "application/pdf" },
    { "ttf", "application/x-font-ttf" },
    { "ttc", "application/x-font-ttf" }
};

static const HTTPRcvdHeaderDescriptor_t pxRcvdHeaderDescs[] = {
    // http headers
    { "Accept" },
    { "Content-Length" },
    { "Content-Type" },
    // common headers
    { "Host" },
    { "Connection" },
    // websocket headers
    { "Transfer-Encoding" },
    { "Upgrade" },
    { "Sec-Websocket-Version" },
    { "Sec-Websocket-Key" },
};
static const size_t uxNumRcvdHeaderDescrs =
    sizeof(pxRcvdHeaderDescs)
        / sizeof(HTTPRcvdHeaderDescriptor_t);

static const char *const pcWebsocketUUID =
    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *const pcWebsocketRespHeaders =
    "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: ";

/*===============================================
 external objects
 ===============================================*/

extern const RouteConfig_t xRouteConfig __attribute__ ((weak, alias ("_xRouteConfig")));
/* the `xRouteConfig` object should be located elsewhere in source, e.g. in httpd_config.c
 * */
const RouteConfig_t _xRouteConfig = { "/\\", 0, 0, prvDefaultErrorHandler };

/*===============================================
 public functions
 ===============================================*/

BaseType_t xHttpCreate(void *pxc) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	pxClient->bits.ulFlags = 0;
	return 0;
}

BaseType_t xHttpWork(void *pxc) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	if (pxClient->bits.bFileInProgress) {
		return prvContinueSendFile(pxClient);
	}
	else {
		return prvServiceRequest(pxClient);
	}
}

BaseType_t xHttpDelete(void *pxc) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	if (pxClient->pxFileHandle != 0)
	  ff_fclose(pxClient->pxFileHandle);
	return 0;
}

const HttpStatusDescriptor_t* const pxGetHttpStatusMessage(
    const BaseType_t xStatus) {
	HttpStatusDescriptor_t *pxStatusDesc =
	    (HttpStatusDescriptor_t*) &xHttpStatuses[0];
	while (pxStatusDesc->uxLen != 0) {
		if (pxStatusDesc->pcStatus == xStatus)
		  return pxStatusDesc;
		pxStatusDesc++;
	}
	return pxDefaultHttpStatus;
}

size_t xPrintRoute(char *pcDst, const char **pcRouteParts, const size_t uxn) {
	size_t uxp, uxCnt = 0;
	char **pcr = (char**) pcRouteParts;
	while (*pcr != HTTPD_ROUTE_TERMINATOR) {
		uxp = snprintf(&pcDst[uxCnt], uxn - uxCnt, "/%s", *pcr++);
		uxCnt += uxp;
	}
	return uxCnt;
}

size_t xPrintParams(char *pcDst, const char **pcParamParts, const size_t uxn) {
	size_t uxp, uxCnt = 0;
	char **pcr = (char**) pcParamParts;
	if (*pcr == HTTPD_ROUTE_TERMINATOR) {
		*pcDst = 0;
		return 0;
	}
	else {
		*pcDst = '?';
		uxCnt = 1;
	}
	char pcDelim[2] = { 0, 0 };
	while (*pcr != HTTPD_ROUTE_TERMINATOR) {
		uxp = snprintf(&pcDst[uxCnt], uxn - uxCnt, "%s%s", pcDelim, *pcr++);
		pcDelim[0] = '&';
		uxCnt += uxp;
	}
	return uxCnt;
}

const char* pcGetContentsType(const char *pcFileExt) {
	const char *pcSlash = NULL;
	const char *pcDot = NULL;
	const char *pcPtr;
	const char *pcResult = "text/html";
	BaseType_t xi;
	for (pcPtr = pcFileExt; *pcPtr; pcPtr++) {
		if (*pcPtr == '.') pcDot = pcPtr;
		if (*pcPtr == '/') pcSlash = pcPtr;
	}
	if (pcDot > pcSlash) {
		pcDot++;
		for (xi = 0; xi < ARRAY_SIZE(pxTypeCouples); xi++) {
			if (strcasecmp(pcDot, pxTypeCouples[xi].pcExtension) == 0) {
				pcResult = pxTypeCouples[xi].pcType;
				break;
			}
		}
	}
	return pcResult;
}

BaseType_t xSendHttpResponseHeaders(
    void *pxc,
    const BaseType_t xCode,
    const uint32_t xOpts,
    const size_t uxLen,
    const char *pcContentType,
    const char *pcExtra
    ) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	char *pcSndBuff = pxClient->pxParent->pcSndBuff;
	size_t uxHeaderSz = 0;
	BaseType_t xRc;
	uxHeaderSz = prvConstructHeaders(pcSndBuff,
	    sizeof(pxClient->pxParent->pcSndBuff),
	    xCode,
	    xOpts,
	    pcContentType, uxLen, pcExtra);
	// send the data
	xRc = FreeRTOS_send(pxClient->xSock, (const void*) pcSndBuff, uxHeaderSz, 0);
	if (xRc < 0)
	  return xRc;
	// return the number of bytes sent
	return xRc;
}

BaseType_t xSendHttpResponseContent(void *pxc, char *pcContent, size_t uxLen) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	size_t uxSpace, uxBlock, uxSent = 0;
	BaseType_t xRc = 0;
	do {
		uxSpace = FreeRTOS_tx_space(pxClient->xSock);
		uxBlock = (uxLen - uxSent) > uxSpace ? uxSpace : uxLen - uxSent;
		if (uxBlock > 0) {
			xRc = FreeRTOS_send(pxClient->xSock, &pcContent[uxSent], uxBlock, 0);
			if (xRc < 0)
			  break;
			uxSent += (size_t) xRc;
		}
	} while (uxLen < uxSent);
	if (pxClient->uxBytesLeft == 0u) {
		/* Writing is ready, no need for further 'eSELECT_WRITE' events. */
		FreeRTOS_FD_CLR(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
	}
	else {
		/* Wake up the TCP task as soon as this socket may be written to. */
		FreeRTOS_FD_SET(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
	}
	return (BaseType_t) uxSent;
}

BaseType_t xSendHttpResponseChunk(void *pxc, char *pcContent, size_t uxLen) {
	char pcChunkMsg[16];
	size_t uxChunkSz, uxSent;
	BaseType_t xRc;
	if (pcContent == 0)
	  return xSendHttpResponseContent(pxc, "0\r\n\r\n", 5);
	if (uxLen == 0)
	  uxLen = strlen(pcContent);
	uxChunkSz = snprintf(pcChunkMsg, sizeof(pcChunkMsg), "%x\r\n", uxLen);
	xRc = xSendHttpResponseContent(pxc, pcChunkMsg, uxChunkSz);
	if (xRc < 0)
	  return xRc;
	uxSent += (size_t) xRc;
	xRc = xSendHttpResponseContent(pxc, pcContent, uxLen);
	if (xRc < 0)
	  return xRc;
	uxSent += (size_t) xRc;
	xRc = xSendHttpResponseContent(pxc, "\r\n", 2);
	if (xRc < 0)
	  return xRc;
	uxSent += (size_t) xRc;
	return (BaseType_t) uxSent;
}

BaseType_t xSendHttpResponseFile(void *pxc) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	size_t uxSpace, uxCount, uxSent;
	BaseType_t xRc = 0;
	if (pxClient->pxFileHandle == NULL)
	  return 0;
	pxClient->uxBytesLeft = (size_t) pxClient->pxFileHandle->ulFileSize;
	pxClient->bits.bFileInProgress = 1;
	uxSent = 0;
	do {
		uxSpace = FreeRTOS_tx_space(pxClient->xSock);
		uxCount = pxClient->uxBytesLeft < uxSpace ? pxClient->uxBytesLeft : uxSpace;
		if (uxCount > 0u) {
			if (uxCount > sizeof(pxClient->pxParent->pcSndBuff))
			  uxCount = sizeof(pxClient->pxParent->pcSndBuff);
			ff_fread(pxClient->pxParent->pcSndBuff, 1, uxCount,
			    pxClient->pxFileHandle);
			pxClient->uxBytesLeft -= uxCount;
			xRc = FreeRTOS_send(pxClient->xSock, pxClient->pxParent->pcSndBuff,
			    uxCount,
			    0);
			if (xRc <= 0)
			break;
			else // not required; included for clarity
			uxSent += xRc;
		}
	} while (pxClient->uxBytesLeft > 0u && uxSent < emberHTTP_FILE_CHUNK_SIZE);
	if (pxClient->uxBytesLeft == 0u) {
		/* Writing is ready, no need for further 'eSELECT_WRITE' events. */
		FreeRTOS_FD_CLR(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		// close the file, and clear the local pointer to the file handle
		ff_fclose(pxClient->pxFileHandle);
		pxClient->pxFileHandle = NULL;
		pxClient->bits.bFileInProgress = 0;
		return uxSent;
	}
	else if (xRc < 0) { // implied: error trying to write to socket; ditch
		FreeRTOS_FD_CLR(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		// close the file, and clear the local pointer to the file handle
		ff_fclose(pxClient->pxFileHandle);
		pxClient->pxFileHandle = NULL;
		pxClient->bits.bFileInProgress = 0;
		return xRc;
	}
	else { // implied: xRc == 0, i.e. could not write to socket, but bytes are left to write
		/* Wake up the TCP task as soon as this socket may be written to. */
		FreeRTOS_FD_SET(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		return uxSent;
	}
}

BaseType_t xGetHeaderValue(void *pxc, const char *pcText, char **pcValue) {
	BaseType_t xi;
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	BaseType_t xDescId = prvFindMatchingHeader(pcText);
	if (xDescId < 0) {
		*pcValue = 0;
		return xDescId;
	}
	for (xi = 0; xi < emberHTTP_HEADER_PARTS; xi++) {
		if (pxClient->pxHeaders[xi].xDescriptor < 0)
		  break;
		if (pxClient->pxHeaders[xi].xDescriptor == xDescId) {
			*pcValue = pxClient->pxHeaders[xi].pcValue;
			return xi;
		}
	}
	return -1;
}

BaseType_t xUpgradeToWebsocket(
    void *pxc,
    const WebsocketMessageHandler_t txtHandler,
    const WebsocketMessageHandler_t binHandler,
    const char *pcRoute) {
	HTTPClient_t *pxHttpClient = (HTTPClient_t*) pxc;
	WebsocketClient_t *pxWsClient = (WebsocketClient_t*) pxc;
	BaseType_t xRc;
	if (pxHttpClient->xHttpVerb != eHTTP_GET)
	  return xRouteConfig.pxErrorHandler(pxc, eHTTP_BAD_REQUEST);
	// find the mandatory websocket upgrade headers; if any were not received, throw a BAD_REQUEST
	char *pcHostname, *pcConnection, *pcUpgrade, *pcWsVersion, *pcWsKey;
	if ((xGetHeaderValue(pxc, "Host", &pcHostname) < 0)
	    || (xGetHeaderValue(pxc, "Connection", &pcConnection) < 0)
	    || (xGetHeaderValue(pxc, "Upgrade", &pcUpgrade) < 0)
	    || (xGetHeaderValue(pxc, "Sec-Websocket-Version", &pcWsVersion) < 0)
	    || (xGetHeaderValue(pxc, "Sec-Websocket-Key", &pcWsKey) < 0))
	  return xRouteConfig.pxErrorHandler(pxc, eHTTP_BAD_REQUEST);
	// check that the mandatory headers have the mandatory values; if any do not, throw a BAD_REQUEST
	if ((!strcasestr(pcConnection, "upgrade"))
	    || (!strcasestr(pcUpgrade, "websocket"))
	    || (!strcasestr(pcWsVersion, "13")))
	  return xRouteConfig.pxErrorHandler(pxc, eHTTP_BAD_REQUEST);
	xRc = prvSendWebsocketUpgradeHeaders(pxHttpClient, pcWsKey);
	if (xRc > 0) {
		pxWsClient->xCreator = WEBSOCKETD_CREATOR_METHOD;
		pxWsClient->xWork = WEBSOCKETD_WORKER_METHOD;
		pxWsClient->xDelete = WEBSOCKETD_DELETE_METHOD;
		pxWsClient->pxTxtHandler = txtHandler;
		pxWsClient->pxBinHandler = binHandler;
		strncpy(pxWsClient->pcRoute, pcRoute, sizeof(pxWsClient->pcRoute));
	}
	return xRc;
}

/*===============================================
 private functions
 ===============================================*/

static BaseType_t prvServiceRequest(HTTPClient_t *pxClient) {
	BaseType_t xRc;
	size_t uxCmdBuffSz;
	char *pcCmdBuff, *pcEndOfCmd, *pcEndOfUrl;
	pcCmdBuff = pxClient->pxParent->pcRcvBuff;
	uxCmdBuffSz = emberTCP_RCV_BUFFER_SIZE;
	// (try to) transfer a new request from the TCP receive buffer to the HTTP
	// server receive buffer
	xRc = FreeRTOS_recv(pxClient->xSock, (void*) pcCmdBuff, uxCmdBuffSz, 0);
	if (xRc <= 0) // -ve is an error; 0 is "no data received"; either way, return it
	  return xRc;
	// ensure that we know where the request ends
	if (xRc < uxCmdBuffSz)
	  pcCmdBuff[xRc] = 0;
	pcEndOfCmd = &pcCmdBuff[xRc];
	/* Parse the request:
	 *  - find the verb and URL
	 *  - resolve the URL parts
	 *  - resolve the headers (discarding any that we don't care about)
	 *  - resolve the body
	 *  - try to match the request to a route for which there is a handler
	 * */
	prvFindHTTPVerb(pxClient);
	if (pxClient->xHttpVerb < 0)
	  return xRouteConfig.pxErrorHandler(pxClient, eHTTP_BAD_REQUEST);
	pcEndOfUrl = strpbrk(pxClient->pcUrlData, " \t\n");
	if (pcEndOfUrl == 0 || pcEndOfUrl == pcEndOfCmd)
	  return xRouteConfig.pxErrorHandler(pxClient, eHTTP_BAD_REQUEST);
	*pcEndOfUrl = 0;
	prvResolveUrlParts(pxClient);
	xRc = prvResolveHeaders(pxClient);
	if (xRc < 0)
	  return xRouteConfig.pxErrorHandler(pxClient, eHTTP_BAD_REQUEST);
	xRc = prvResolveBody(pxClient);
	if (xRc < 0)
	  return xRouteConfig.pxErrorHandler(pxClient, eHTTP_BAD_REQUEST);
	return prvMatchRoute(pxClient);
}

static BaseType_t prvDefaultErrorHandler(void *pxc, eHttpStatus xCode) {
	HTTPClient_t *pxClient = (HTTPClient_t*) pxc;
	BaseType_t xRc;
	pxClient->bits.ulFlags = 0;
	const HttpStatusDescriptor_t *pxStatus = pxGetHttpStatusMessage(xCode);
	xRc = xSendHttpResponseHeaders(pxc, xCode, eResponseOption_ContentLength,
	    pxStatus->uxLen, "text/html", 0);
	xRc += xSendHttpResponseContent(pxc, (char*) pxStatus->pcText,
	    pxStatus->uxLen);
	return xRc;
}

static void prvFindHTTPVerb(HTTPClient_t *pxClient) {
	char *pcCmdBuff = pxClient->pxParent->pcRcvBuff;
	pxClient->xHttpVerb = -1;
	for (BaseType_t xi = 0; xHttpVerbs[xi].xLen > 0; xi++) {
		if (strncmp(pcCmdBuff, xHttpVerbs[xi].text, xHttpVerbs[xi].xLen) == 0) {
			pxClient->xHttpVerb = xHttpVerbs[xi].xVerb;
			pxClient->pcUrlData = &pcCmdBuff[xHttpVerbs[xi].xLen + 1];
			break;
		}
	}
}

static BaseType_t prvUrlDecode(char *pcUrl) {
	size_t uxUrlLen = strlen(pcUrl);
	BaseType_t xn = 0;
	while (xn < uxUrlLen) {
		if (pcUrl[xn] == '%') {
			if ((uxUrlLen - xn) < 3) {
				// there aren't enough chars to parse; eek!
				return uxUrlLen;
			}
			else {
				char pcNum[3];
				pcNum[0] = pcUrl[xn + 1];
				pcNum[1] = pcUrl[xn + 2];
				pcNum[2] = 0;
				pcUrl[xn] = strtol(pcNum, 0, 16);
				memcpy(&pcUrl[xn + 1], &pcUrl[xn + 3], uxUrlLen - xn - 2);
				uxUrlLen -= 2;
			}
		}
		else if (pcUrl[xn] == '+') {
			pcUrl[xn] = ' ';
		}
		xn++;
	}
	return xn;
}

static void prvResolveUrlParts(HTTPClient_t *pxClient) {
	BaseType_t xnRoutes = 0, xnParams = 0;
	char *pcWkgUrl = strncpy((char*) pxClient->pcWorkingUrl, pxClient->pcUrlData,
	    sizeof(pxClient->pcWorkingUrl));
	// discard any leading slashes from the uri
	if (*pcWkgUrl == '/') {
		*pcWkgUrl = 0;
		pcWkgUrl++;
	}
	// split the uri into the route and params sections
	char *pcParams = strchr(pcWkgUrl, '?');
	if (pcParams != 0)
	  *pcParams++ = 0;
	// split the route section into its parts
	pxClient->pcRouteParts[0] = pcWkgUrl;
	for (xnRoutes = 1; xnRoutes < (emberHTTP_ROUTE_PARTS - 1); xnRoutes++) {
		char *pcNext = strpbrk(pcWkgUrl, xRouteConfig.pcDelims);
		if (pcNext == 0)
		break;
		else {
			*pcNext = 0;
			pcWkgUrl = ++pcNext;
			pxClient->pcRouteParts[xnRoutes] = pcWkgUrl;
		}
	}
	pxClient->pcRouteParts[xnRoutes] = (char*) HTTPD_ROUTE_TERMINATOR;

	/* perform URL decoding on the route (or not, I don't think it should be decoded?) */
//	nRoutes = 0;
//	while (pxClient->routeParts[nRoutes] != HTTPD_ROUTE_TERMINATOR) {
//		prvUrlDecode((char*)pxClient->routeParts[xnRoutes++]);
//	}
	if (pcParams != 0) {
		pxClient->pcParamParts[0] = pcParams;
		for (xnParams = 1; xnParams < (emberHTTP_PARAM_PARTS - 1); xnParams++) {
			char *pcNext = strpbrk(pcParams, xRouteConfig.pcDelims);
			if (pcNext == 0)
			break;
			else {
				*pcNext = 0;
				pcParams = ++pcNext;
				pxClient->pcParamParts[xnParams] = pcParams;
			}
		}
	}
	pxClient->pcParamParts[xnParams] = (char*) HTTPD_ROUTE_TERMINATOR;
	/* perform URL decoding on the parameters */
	xnParams = 0;
	while (pxClient->pcParamParts[xnParams] != HTTPD_ROUTE_TERMINATOR) {
		prvUrlDecode((char*) pxClient->pcParamParts[xnParams]);
	}
}

static BaseType_t prvFindMatchingHeader(const char *pcFind) {
	for (size_t uxi = 0; uxi < uxNumRcvdHeaderDescrs; uxi++) {
		if (strcasecmp(pxRcvdHeaderDescs[uxi].pcText, pcFind) == 0)
		  return (BaseType_t) uxi;
	}
	return -1;
}

static BaseType_t prvResolveHeaders(HTTPClient_t *pxClient) {
	for (BaseType_t i = 0; i < emberHTTP_HEADER_PARTS; i++) {
		pxClient->pxHeaders[i].xDescriptor = -1;
		pxClient->pxHeaders[i].pcValue = 0;
	}
	BaseType_t xRemLen = emberTCP_SND_BUFFER_SIZE;
	char *pcStartHeader = memchr(pxClient->pxParent->pcRcvBuff, 0, xRemLen);
	if (pcStartHeader == 0)
	  return eHTTPHeader_Invalid;
	pcStartHeader++;
	xRemLen -= (pcStartHeader - pxClient->pxParent->pcRcvBuff);
	char *pcHttpHeader = strnstr(pcStartHeader, "HTTP/1.1\r\n", xRemLen);
	if (pcHttpHeader == 0)
	  return eHTTPHeader_Invalid;
	xRemLen -= ((pcHttpHeader - pcStartHeader) + 10);
	char *pcNextHeader = &pcHttpHeader[10];
	pcHttpHeader[8] = pcHttpHeader[9] = 0;
	BaseType_t xHdrId = 0;
	do {
		char *pcEndHeader = strnstr(pcNextHeader, "\r\n", xRemLen);
		if (pcEndHeader == 0) {
			// header must be terminated with a zero-length header, i.e. \r\n\r\n
			// if it just ends, ditch
			return eHTTPHeader_Invalid;
		}
		if (pcEndHeader == pcNextHeader) {
			// found a double-CRLF, i.e. a zero-length header, so the end of the header block
			pcEndHeader[0] = pcEndHeader[1] = 0;
			pxClient->pcBody = &pcEndHeader[2];
			break;
		}
		xRemLen -= ((pcEndHeader - pcNextHeader) + 2);
		char *pcEndName = strnstr(pcNextHeader, ":", xRemLen);
		if (pcEndName == 0 || pcEndName > pcEndHeader)
		  // end of header name should be before end of header - ditch
		  return eHTTPHeader_Invalid;
		char *pcStartName = pcNextHeader;
		while (*pcStartName == ' ' || *pcStartName == '\t')
			pcStartName++;
		if (pcStartName >= pcEndName)
		  // start of header name should be before end of header name - ditch
		  return eHTTPHeader_Invalid;
		pcEndName[0] = 0;
		char *pcStartValue = &pcEndName[1];
		while (*pcStartValue == ' ' || *pcStartValue == '\t')
			*pcStartValue++ = 0;
		char *pcEndValue = &pcEndHeader[0];
		pcEndValue[0] = pcEndValue[1] = 0;
		char *pcEndTrailer = &pcEndValue[-1];
		while (*pcEndTrailer == ' ' || *pcEndTrailer == '\t')
			*pcEndTrailer-- = 0;
		pcNextHeader = &pcEndValue[2];
		BaseType_t xMatchId = prvFindMatchingHeader(pcStartName);
		if (xMatchId >= 0 && xHdrId < (emberHTTP_HEADER_PARTS - 1)) {
			pxClient->pxHeaders[xHdrId].xDescriptor = xMatchId;
			pxClient->pxHeaders[xHdrId].pcValue = pcStartValue;
			xHdrId++;
		}
	} while (pcNextHeader);
	// TODO: Examine the headers and decide if whether any of them contain
	//       unacceptable values
	return xHdrId;
}

static BaseType_t prvResolveBody(HTTPClient_t *pxClient) {
	BaseType_t xContentLenId = -1, xChunkedId = -1;
	pxClient->uxBodySz = -1;
	for (BaseType_t xi = 0; xi < emberHTTP_HEADER_PARTS; xi++) {
		// A descriptor < 0 indicates no more potential headers
		if (pxClient->pxHeaders[xi].xDescriptor < 0)
		break;
		// Transfer-Encoding is descriptor 4, and has priority over Content-Length if it includes "chunked"
		else if (pxClient->pxHeaders[xi].xDescriptor == 4) {
			if (strstr(pxClient->pxHeaders[xi].pcValue, "chunked")) {
				xChunkedId = xi;
				xContentLenId = -1;
				break;
			}
		}
		// Content-Length is descriptor 1
		else if (pxClient->pxHeaders[xi].xDescriptor == 1)
		  xContentLenId = xi;
	}
	if (xChunkedId < 0 && xContentLenId < 0) {
		if (pxClient->pcBody == 0 || *pxClient->pcBody == 0)
		// if there is no body, and no headers indicating that there should be a body, all good
		pxClient->uxBodySz = 0;
		else
		// if there are headers indicating that there should be a body, but there isn't, ditch
		pxClient->uxBodySz = -1;
		return pxClient->uxBodySz;
	}
	size_t uxBodyLen = strnlen(
	    pxClient->pcBody,
	    emberTCP_RCV_BUFFER_SIZE
	        - (size_t) (pxClient->pcBody - pxClient->pxParent->pcRcvBuff)
	        );
	if (xChunkedId >= 0) {
		size_t uxRemBodyLen = uxBodyLen;
		size_t uxTotalSz = 0;
		char *pcChunkStart = pxClient->pcBody;
		char *pcChunkSzEnd, *chunkDataStart, *chunkDataEnd;
		do {
			if (uxRemBodyLen < 5)
			  return eHTTPBody_Invalid;
			if (strncmp(pcChunkStart, "0\r\n\r\n", 5) == 0) {
				pcChunkStart[0] = 0;
				pxClient->uxBodySz = uxTotalSz;
				return uxTotalSz;
			}
			pcChunkSzEnd = strnstr(pcChunkStart, "\r\n", uxBodyLen);
			if (pcChunkSzEnd == 0)
			  return eHTTPBody_Invalid;
			pcChunkSzEnd[0] = 0;
			size_t uxChunkSz = strtol(pcChunkStart, 0, 16);
			uxTotalSz += uxChunkSz;
			chunkDataStart = &pcChunkSzEnd[2];
			chunkDataEnd = &chunkDataStart[uxChunkSz + 2]; // +2 allows for (required) trailing CRLF
			memcpy(pcChunkStart, chunkDataStart, uxChunkSz);
			uxRemBodyLen -= (chunkDataEnd - pcChunkStart);
			memcpy(&pcChunkStart[uxChunkSz], chunkDataEnd, uxRemBodyLen);
			pcChunkStart[uxChunkSz + uxRemBodyLen] = 0;
			pcChunkStart = &pcChunkStart[uxChunkSz];
		} while (1);
	}
	else {
		BaseType_t xContentLen = 0;
		xContentLen = atoi(pxClient->pxHeaders[xContentLenId].pcValue);
		if (xContentLen < 0)
		  return eHTTPBody_Invalid;
		uxBodyLen = strnlen(
		    pxClient->pcBody,
		    emberTCP_RCV_BUFFER_SIZE
		        - (size_t) (pxClient->pcBody - pxClient->pxParent->pcRcvBuff)
		        );
		if (uxBodyLen != xContentLen)
		  return eHTTPBody_Invalid;
		pxClient->uxBodySz = uxBodyLen;
		return pxClient->uxBodySz;
	}
}

static BaseType_t prvMatchRoute(HTTPClient_t *pxClient) {
	xRouteHandler *pxHandler = 0;
	const RouteItem_t *pxRouteItem = 0;
	BaseType_t xi, xj;
	for (xi = 0; xi < xRouteConfig.uxNumRoutes; xi++) {
		pxRouteItem = &xRouteConfig.pxItems[xi];
		// note that tests for a zero-length string equate to testing for a '/' that was
		// stripped out during path and parameter resolution
		for (xj = 0; xj < emberHTTP_ROUTE_PARTS; xj++) {
			// if both the pointers are -1, we have reached the last comparison and have
			// a perfect match
			if (pxClient->pcRouteParts[xj] == HTTPD_ROUTE_TERMINATOR
			    && pxRouteItem->pcPath[xj] == HTTPD_ROUTE_TERMINATOR) {
				pxHandler = pxRouteItem->pxHandler;
				break;
			}
			// if the routeItem_t ignores (one) trailing slashes
			if (pxRouteItem->uxOptions.ignore_trailing_slash) {
				// if there is a trailing slash on the route but no trailing slash on the
				// routeItem_t's path or if there is a trailing slash on the routeItem_t's
				// path but no trailing slash on the route, we have a match
				if ((
				    pxClient->pcRouteParts[xj + 1] == HTTPD_ROUTE_TERMINATOR
				        && *pxClient->pcRouteParts[xj] == 0
				        && pxRouteItem->pcPath[xj] == HTTPD_ROUTE_TERMINATOR
				    ) || (
				    pxClient->pcRouteParts[xj] == HTTPD_ROUTE_TERMINATOR
				        && *pxRouteItem->pcPath[xj] == 0
				        && pxRouteItem->pcPath[xj + 1] == HTTPD_ROUTE_TERMINATOR
				    )) {
					pxHandler = pxRouteItem->pxHandler;
					break;
				}
			}
			// if either pointer is -1, then there is no perfect match - ditch
			if (pxClient->pcRouteParts[xj] == HTTPD_ROUTE_TERMINATOR
			    || pxRouteItem->pcPath[xj] == HTTPD_ROUTE_TERMINATOR)
			  break;
			// wildcard match against '*' for the path element, if it is enabled for the
			// path
			if (pxRouteItem->uxOptions.allow_wildcards
			    && fnmatch(pxRouteItem->pcPath[xj], pxClient->pcRouteParts[xj], 0)
			        == 0)
			  //			if (routeItem->options.allow_wildcards && routeItem->path[j][0] == '*')
			  continue;
			// wildcard match against '%' for the remainder of the path, if it is enabled for
			// the path
			if (pxRouteItem->uxOptions.allow_wildcards
			    && pxRouteItem->pcPath[xj][0] == '%') {
				pxHandler = pxRouteItem->pxHandler;
				break;
			}
			// if the aligned route parts are not case-insensitive equal, then there is
			// no match - ditch
			if (strcasecmp(pxClient->pcRouteParts[xj], pxRouteItem->pcPath[xj]) != 0)
			  break;
		}
		if (pxHandler != 0) {
			return pxHandler(pxClient);
		}
	}
	if (pxHandler == 0)
	  return xRouteConfig.pxErrorHandler(pxClient, eHTTP_NOT_FOUND);
	return -1;
}

static size_t prvConstructHeaders(
    char *pcDst,
    const size_t uxMaxSz,
    const BaseType_t xCode,
    const uint32_t xOpts,
    const char *pcContentType,
    const size_t uxContentLen,
    const char *pcExtra) {
	size_t uxp = 0;
	uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "HTTP/1.1 %d %s\r\n", xCode,
	    pxGetHttpStatusMessage(xCode)->pcText);
	uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "%s",
	    "Accept-Encoding: identity\r\nConnection: close\r\n");
	if (pcContentType)
	  uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "Content-Type: %s\r\n",
	      pcContentType);
	if (((ResponseOptions_t) xOpts).content_length)
	uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "Content-Length: %u\r\n",
	    uxContentLen);
	else if (((ResponseOptions_t) xOpts).chunked_body)
	  uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "%s",
	      "Transfer-Encoding: chunked\r\n");
	if (pcExtra) {
		uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "%s", pcExtra);
		// if someone didn't add a trailing \r\n, do so
		if (pcDst[uxp - 2] != 13 || pcDst[uxp - 1] != 10)
		  uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "%s", "\r\n");
	}
	// extra CRLF to terminate the header block
	uxp += snprintf(&pcDst[uxp], uxMaxSz - uxp, "%s", "\r\n");
	return uxp;
}

static BaseType_t prvSendWebsocketUpgradeHeaders(
    HTTPClient_t *pxClient,
    char *pcKey) {
	char *pcSndBuff = pxClient->pxParent->pcSndBuff;
	size_t uxSndBuffSz = sizeof(pxClient->pxParent->pcSndBuff);
	size_t uxHeaderSz = 0;
	BaseType_t xRc;
	uxHeaderSz = snprintf(pcSndBuff, uxSndBuffSz, pcWebsocketRespHeaders);
	char pcAcceptVal[64] = "";
	unsigned char pucAcceptHash[20];
	char pcAcceptEnc[32];
	strcat(pcAcceptVal, pcKey);
	strcat(pcAcceptVal, pcWebsocketUUID);
	sha1(pcAcceptVal, strlen(pcAcceptVal), pucAcceptHash);
	size_t uxAcceptLen = sizeof(pcAcceptEnc);
	if (base64_encode(pcAcceptEnc, &uxAcceptLen, pucAcceptHash,
	    sizeof(pucAcceptHash)) != 0)
	  return -pdFREERTOS_ERRNO_ENOBUFS;
	uxHeaderSz += snprintf(&pcSndBuff[uxHeaderSz], uxSndBuffSz, "%s\r\n\r\n",
	    pcAcceptEnc);
	xRc = FreeRTOS_send(pxClient->xSock, (const void*) pcSndBuff, uxHeaderSz, 0);
	if (xRc < 0)
	  return xRc;
	// return the number of bytes sent
	return xRc;
}

static BaseType_t prvContinueSendFile(HTTPClient_t *pxClient) {
	size_t uxSpace, uxCount, uxSent;
	BaseType_t xRc = 0;
	if (pxClient->pxFileHandle == NULL)
	  return 0;
	uxSent = 0;
	do {
		uxSpace = FreeRTOS_tx_space(pxClient->xSock);
		uxCount = pxClient->uxBytesLeft < uxSpace ? pxClient->uxBytesLeft : uxSpace;
		if (uxCount > 0u) {
			if (uxCount > sizeof(pxClient->pxParent->pcSndBuff))
			  uxCount = sizeof(pxClient->pxParent->pcSndBuff);
			ff_fread(pxClient->pxParent->pcSndBuff, 1, uxCount,
			    pxClient->pxFileHandle);
			pxClient->uxBytesLeft -= uxCount;
			xRc = FreeRTOS_send(pxClient->xSock, pxClient->pxParent->pcSndBuff,
			    uxCount,
			    0);
			if (xRc <= 0)
			break;
			else // not required; included for clarity
			uxSent += xRc;
		}
	} while (pxClient->uxBytesLeft > 0u && uxSent < emberHTTP_FILE_CHUNK_SIZE);
	if (pxClient->uxBytesLeft == 0u) {
		/* Writing is ready, no need for further 'eSELECT_WRITE' events. */
		FreeRTOS_FD_CLR(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		// close the file, and clear the local pointer to the file handle
		ff_fclose(pxClient->pxFileHandle);
		pxClient->pxFileHandle = NULL;
		pxClient->bits.bFileInProgress = 0;
		return uxSent;
	}
	else if (xRc < 0) { // implied: error trying to write to socket; ditch
		FreeRTOS_FD_CLR(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		// close the file, and clear the local pointer to the file handle
		ff_fclose(pxClient->pxFileHandle);
		pxClient->pxFileHandle = NULL;
		pxClient->bits.bFileInProgress = 0;
		return xRc;
	}
	else { // implied: xRc == 0, i.e. could not write to socket, but bytes are left to write
		/* Wake up the TCP task as soon as this socket may be written to. */
		FreeRTOS_FD_SET(pxClient->xSock, pxClient->pxParent->xSockSet,
		    eSELECT_WRITE);
		return uxSent;
	}

}

