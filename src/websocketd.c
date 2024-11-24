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

#include "inc/websocketd.h"
#include <string.h>

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

/*===============================================
 private function prototypes
 ===============================================*/

static BaseType_t prvParseFrame(WebsocketClient_t *pxClient);
static BaseType_t prvParseFrameX16(WebsocketClient_t *pxClient);
static BaseType_t prvParseFrameX64(WebsocketClient_t *pxClient);
static void prvMaskPayload(WebsocketClient_t *pxClient);
static BaseType_t prvSendClose(
	WebsocketClient_t *pxClient,
	const BaseType_t xCode);
static BaseType_t prvSendMessagePayload(
	WebsocketClient_t *pxClient,
	const void *pvPayload,
	const size_t uxLen);
static BaseType_t prvSendMessageHeader(
	WebsocketClient_t *pxClient,
	const eWebsocketOpcode eCode,
	const size_t uxPayloadSz);

/*===============================================
 private global variables
 ===============================================*/

/*===============================================
 public objects
 ===============================================*/

/*===============================================
 external objects
 ===============================================*/

/*===============================================
 public functions
 ===============================================*/

BaseType_t xWebsocketWork(void *pxc)
{
	BaseType_t xRc;
	WebsocketClient_t *pxClient = (WebsocketClient_t *)pxc;
	char *rcvBuff = pxClient->pxParent->pcRcvBuff;
	size_t rcvBuffSz = emberTCP_RCV_BUFFER_SIZE;
	xRc = FreeRTOS_recv(pxClient->xSock, (void *)rcvBuff, rcvBuffSz, 0);
	if (xRc <= 0) // -ve is an error; 0 is "no data received"; either way, return it
		return xRc;
	if (((WebsocketHeader_t *)rcvBuff)->xFlags.payLen < 126)
		xRc = prvParseFrame(pxClient);
	else if (((WebsocketHeader_t *)rcvBuff)->xFlags.payLen == 126)
		xRc = prvParseFrameX16(pxClient);
	else
		xRc = prvParseFrameX64(pxClient);
	if (xRc < 0)
		return -pdFREERTOS_ERRNO_EBADE;
	switch (pxClient->xFlags.opcode)
	{
	case eWSOp_Continue:
		return 0;
	case eWSOp_Text:
		if (pxClient->pxTxtHandler)
			return pxClient->pxTxtHandler(pxc);
		else
			prvSendClose(pxClient, eWS_UNSUPPORTED_DATA);
		return -1;
	case eWSOp_Binary:
		if (pxClient->pxBinHandler)
			return pxClient->pxBinHandler(pxc);
		else
			prvSendClose(pxClient, eWS_UNSUPPORTED_DATA);
		return -1;
	case eWSOp_Close:
		((WebsocketHeader_t *)rcvBuff)->xFlags.fin = 1;
		FreeRTOS_send(pxClient->xSock, (const void *)rcvBuff,
					  pxClient->xPayloadSz + sizeof(WebsocketHeader_t), 0);
		return -1;
	case eWSOp_Ping:
		((WebsocketHeader_t *)rcvBuff)->xFlags.opcode = eWSOp_Pong;
		((WebsocketHeader_t *)rcvBuff)->xFlags.fin = 1;
		return FreeRTOS_send(pxClient->xSock, (const void *)rcvBuff,
							 pxClient->xPayloadSz + sizeof(WebsocketHeader_t), 0);
	case eWSOp_Pong:
		return 0;
	default:
		prvSendClose(pxClient, eWS_PROTOCOL_ERROR);
		return -1;
	}
}

const WebsocketStatusDescriptor_t *const pxGetWebsocketStatusMessage(
	const BaseType_t status)
{
	WebsocketStatusDescriptor_t *sp =
		(WebsocketStatusDescriptor_t *)&WebsocketStatuses[0];
	while (sp->xLen != 0)
	{
		if (sp->xStatus == status)
			return sp;
		sp++;
	}
	return defaultWebsocketStatus;
}

BaseType_t xSendWebsocketHeader(
	void *pxc,
	const eWebsocketOpcode eCode,
	const size_t uxPayloadSz)
{
	return prvSendMessageHeader(pxc, eCode, uxPayloadSz);
}

BaseType_t xSendWebsocketPayload(
	void *pxc,
	const void *pvPayload,
	const size_t uxLen)
{
	return prvSendMessagePayload(pxc, pvPayload, uxLen);
}

BaseType_t xSendWebsocketTextMessage(
	void *pxc,
	const char *msg,
	const size_t len)
{
	WebsocketClient_t *pxClient = (WebsocketClient_t *)pxc;
	BaseType_t xRc;
	xRc = prvSendMessageHeader(pxClient, eWSOp_Text, len);
	if (xRc < 0)
		return xRc;
	return prvSendMessagePayload(pxClient, msg, len);
}

BaseType_t xSendWebsocketBinaryMessage(
	void *pxc,
	const char *msg,
	const size_t len)
{
	WebsocketClient_t *pxClient = (WebsocketClient_t *)pxc;
	BaseType_t xRc;
	xRc = prvSendMessageHeader(pxClient, eWSOp_Binary, len);
	if (xRc < 0)
		return xRc;
	return prvSendMessagePayload(pxClient, msg, len);
}

/*===============================================
 private functions
 ===============================================*/

static BaseType_t prvParseFrame(WebsocketClient_t *pxClient)
{
	char *pcRcvBuff = pxClient->pxParent->pcRcvBuff;
	WebsocketHeader_t *pxHeader = ((WebsocketHeader_t *)pcRcvBuff);
	pxClient->pcPayload = &pcRcvBuff[sizeof(WebsocketHeader_t)];
	pxClient->xPayloadSz = pxHeader->xFlags.payLen;
	pxClient->xFlags = pxHeader->xFlags;
	pxClient->usMaskKeyL = pxHeader->usMaskKeyL;
	pxClient->usMaskKeyU = pxHeader->usMaskKeyU;
	prvMaskPayload(pxClient);
	return 0;
}

static BaseType_t prvParseFrameX16(WebsocketClient_t *pxClient)
{
	char *pcRcvBuff = pxClient->pxParent->pcRcvBuff;
	WebsocketHeaderX16_t *pxHeader = ((WebsocketHeaderX16_t *)pcRcvBuff);
	// if we know that the frame doesn't fit in the receive buffer, start the connection close process
	if (pxHeader->xFlags.payLen > (sizeof(pxClient->pxParent->pcRcvBuff) - sizeof(WebsocketHeaderX16_t)))
	{
		prvSendClose(pxClient, eWS_MESSAGE_TOO_BIG);
		return -1;
	}
	pxClient->pcPayload = &pcRcvBuff[sizeof(WebsocketHeaderX16_t)];
	pxClient->xPayloadSz = pxHeader->usPayLenX16;
	pxClient->xFlags = pxHeader->xFlags;
	pxClient->usMaskKeyL = pxHeader->usMaskKeyL;
	pxClient->usMaskKeyU = pxHeader->usMaskKeyU;
	prvMaskPayload(pxClient);
	return 0;
}

static BaseType_t prvParseFrameX64(WebsocketClient_t *pxClient)
{
	// never accept large frames: respond with a "1009 message too big" closing frame
	prvSendClose(pxClient, eWS_MESSAGE_TOO_BIG);
	return -1;
}

static void prvMaskPayload(WebsocketClient_t *pxClient)
{
	uint16_t *pusMaskptr = (uint16_t *)pxClient->pcPayload;
	while (pusMaskptr < (uint16_t *)(&pxClient->pcPayload[pxClient->xPayloadSz]))
	{
		*pusMaskptr++ ^= pxClient->usMaskKeyL;
		*pusMaskptr++ ^= pxClient->usMaskKeyU;
	}
}

static BaseType_t prvSendClose(
	WebsocketClient_t *pxClient,
	const BaseType_t xCode)
{
	char *pcSndBuff = pxClient->pxParent->pcSndBuff;
	size_t uxSndBuffSz = sizeof(pxClient->pxParent->pcSndBuff);
	size_t uxMsglen = snprintf(pcSndBuff, uxSndBuffSz, "%d %s", xCode,
							   pxGetWebsocketStatusMessage(xCode)->pcText);
	return FreeRTOS_send(pxClient->xSock, (const void *)pcSndBuff, uxMsglen, 0);
}

static BaseType_t prvSendMessageHeader(
	WebsocketClient_t *pxClient,
	const eWebsocketOpcode eCode,
	const size_t uxPayloadSz)
{
	BaseType_t xRc;
	char buff[sizeof(WebsocketSendHeaderX16_t)];
	if (uxPayloadSz < 126)
	{
		WebsocketSendHeader_t *pxHeader = (WebsocketSendHeader_t *)buff;
		memset(pxHeader, 0, sizeof(WebsocketSendHeader_t));
		pxHeader->xFlags.fin = 1;
		pxHeader->xFlags.opcode = eCode;
		pxHeader->xFlags.payLen = uxPayloadSz;
		return FreeRTOS_send(pxClient->xSock, (const void *)buff,
							 sizeof(WebsocketSendHeader_t), 0);
	}
	else if (uxPayloadSz < 65536)
	{
		WebsocketSendHeaderX16_t *pxHeader = (WebsocketSendHeaderX16_t *)buff;
		memset(pxHeader, 0, sizeof(WebsocketSendHeaderX16_t));
		pxHeader->xFlags.fin = 1;
		pxHeader->xFlags.opcode = eCode;
		pxHeader->xFlags.payLen = 126;
		pxHeader->usPayLenX16 = FreeRTOS_htons(uxPayloadSz);
		return FreeRTOS_send(pxClient->xSock, (const void *)buff,
							 sizeof(WebsocketSendHeaderX16_t), 0);
	}
	else
	{
		prvSendClose(pxClient, eWS_INTERNAL_ERROR);
		return -1;
	}
}

static BaseType_t prvSendMessagePayload(
	WebsocketClient_t *pxClient,
	const void *pvPayload,
	const size_t uxLen)
{
	return FreeRTOS_send(pxClient->xSock, pvPayload, uxLen, 0);
}
