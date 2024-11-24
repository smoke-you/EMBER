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

/*===============================================
 includes
 ===============================================*/

#include <FreeRTOS.h>
#include <FreeRTOS_IP.h>
#include <ftpd.h>
#include <httpd.h>
#include <websocketd.h>
#include "./inc/socket_counter.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

/*===============================================
 private function prototypes
 ===============================================*/

static BaseType_t httpErrorHandler(void *pxc, eHttpStatus code);
static BaseType_t httpRootHandler(void *pxc);
static BaseType_t httpStaticHandler(void *pxc);
static BaseType_t httpCountWebsocketHandler(void *pxc);

/*===============================================
 private objects
 ===============================================*/

static const RouteItem_t pxRouteItems[] = {
	{
		eRouteOption_IgnoreTrailingSlash + eRouteOption_AllowWildcards,
		httpStaticHandler,
		(const char const *[]){"static", "%", HTTPD_ROUTE_TERMINATOR},
	},
	{
		eRouteOption_None,
		httpRootHandler,
		(const char const *[]){"", HTTPD_ROUTE_TERMINATOR},
	},
	{
		eRouteOption_IgnoreTrailingSlash,
		httpCountWebsocketHandler,
		(const char const *[]){"count", HTTPD_ROUTE_TERMINATOR},
	},
};

/*===============================================
 public objects
 ===============================================*/

const RouteConfig_t xRouteConfig = {
	"/\\",
	sizeof(pxRouteItems) / sizeof(RouteItem_t),
	pxRouteItems,
	httpErrorHandler,
};

const WebProtoConfig_t pxWebProtocols[] = {
	{80, 12, "/", HTTPD_CLIENT_SZ, HTTPD_CREATOR_METHOD, HTTPD_WORKER_METHOD, HTTPD_DELETE_METHOD},
	{21, 4, "/", FTPD_CLIENT_SZ, FTPD_CREATOR_METHOD, FTPD_WORKER_METHOD, FTPD_WORKER_METHOD},
};

const TCPServerConfig_t xWebProtoConfig = {
	sizeof(pxWebProtocols) / sizeof(struct xWEBPROTO_CONFIG),
	pxWebProtocols};

/*===============================================
 external objects
 ===============================================*/

/*===============================================
 public functions
 ===============================================*/

/*===============================================
 private functions
 ===============================================*/

static BaseType_t httpErrorHandler(void *pxc, eHttpStatus code)
{
	HTTPClient_t *pxClient = (HTTPClient_t *)pxc;
	BaseType_t xRc;
	pxClient->bits.ulFlags = 0;
	snprintf((char *)pxClient->pcCurrentFilename,
			 sizeof(pxClient->pcCurrentFilename), "/spidisk/web/error/%d.htm", code);
	pxClient->pxFileHandle = ff_fopen(pxClient->pcCurrentFilename, "r");
	if (pxClient->pxFileHandle == 0)
	{
		const HttpStatusDescriptor_t *sp = pxGetHttpStatusMessage(code);
		xRc = xSendHttpResponseHeaders(pxc, code, eResponseOption_ContentLength,
									   sp->uxLen,
									   "text/html", 0);
		xRc += xSendHttpResponseContent(pxc, (char *)sp->pcText, sp->uxLen);
	}
	else
	{
		xRc = xSendHttpResponseHeaders(pxClient, code,
									   eResponseOption_ContentLength,
									   pxClient->pxFileHandle->ulFileSize, "text/html", 0);
		xRc += xSendHttpResponseFile(pxClient);
	}
	return xRc;
}

static BaseType_t httpRootHandler(void *pxc)
{
	HTTPClient_t *pxClient = (HTTPClient_t *)pxc;
	size_t uxSent;
	BaseType_t xRc;
	BaseType_t xLen = 0;
	if (pxClient->xHttpVerb == eHTTP_GET)
	{
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
	else
	{
		return httpErrorHandler(pxc, eHTTP_NOT_ALLOWED);
	}
}

static BaseType_t httpStaticHandler(void *pxc)
{
	HTTPClient_t *pxClient = (HTTPClient_t *)pxc;
	BaseType_t xp;
	BaseType_t xRc;
	BaseType_t xLen = 0;
	if (pxClient->xHttpVerb == eHTTP_GET)
	{
		xp = snprintf((char *)pxClient->pcCurrentFilename,
					  sizeof(pxClient->pcCurrentFilename), "/spidisk/web/static");
		xp += xPrintRoute((char *)&pxClient->pcCurrentFilename[xp],
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
	else
	{
		return httpErrorHandler(pxc, eHTTP_NOT_ALLOWED);
	}
}

static BaseType_t httpCountWebsocketHandler(void *pxc)
{
	return xUpgradeToWebsocket(pxc, xSocketCounterMessageHandler, NULL, "/count");
}
