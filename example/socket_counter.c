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
#include <ember.h>
#include <websocketd.h>
#include <core_json.h>
#include <socket_counter.h>

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

typedef struct
{
	bool bReady;
	TaskHandle_t xPID;
	const configSTACK_DEPTH_TYPE xStackSz;
} SocketCounterConfig_t;

typedef struct
{
	const char *pcPath;
	char *pcMsg;
	size_t xMsgLen;
} SocketCounterActionArgs_t;

/*===============================================
 private function prototypes
 ===============================================*/

static void prvSocketCounter_Service(void *pvArgs);
static BaseType_t prvUpdateWebSockets(void *pxc, void *pvArg);

/*===============================================
 private global variables
 ===============================================*/

static SocketCounterConfig_t prvCfg = {false, 0, 512};
static UBaseType_t prvCount = 0;

/*===============================================
 public objects
 ===============================================*/

/*===============================================
 external objects
 ===============================================*/

/*===============================================
 public functions
 ===============================================*/

void vSocketCounter_Init(void)
{
	if (prvCfg.bReady)
		return;
	xTaskCreate(prvSocketCounter_Service, (const char *)"socketCounter", prvCfg.xStackSz, 0, tskIDLE_PRIORITY, &prvCfg.xPID);
	prvCfg.bReady = true;
}

void vSocketCounter_DeInit(void)
{
	if (!prvCfg.bReady)
		return;
	vTaskDelete(prvCfg.xPID);
	prvCfg.xPID = 0;
	prvCfg.bReady = false;
}

BaseType_t xSocketCounterMessageHandler(void *pxc)
{
	WebsocketClient_t *pxClient = (WebsocketClient_t *)pxc;
	char *pcFieldVal;
	size_t xFieldSz;
	JSONTypes_t xFieldType;
	JSONStatus_t xStatus;
	xStatus = JSON_Validate(pxClient->pcPayload, pxClient->xPayloadSz);
	if (xStatus != JSONSuccess)
		return -1;
	xStatus = JSON_SearchT(pxClient->pcPayload, pxClient->xPayloadSz, "get", 3, &pcFieldVal, &xFieldSz, &xFieldType);
	if (xStatus == JSONSuccess)
	{
		char msg[32];
		return xSendWebsocketTextMessage(pxc, msg, snprintf(msg, sizeof(msg), "{\"count\":%d}", prvCount));
	}
	xStatus = JSON_SearchT(pxClient->pcPayload, pxClient->xPayloadSz, "set", 3, &pcFieldVal, &xFieldSz, &xFieldType);
	if (xStatus == JSONSuccess && xFieldType == JSONNumber)
	{
		prvCount = atoi(pcFieldVal);
		return 0;
	}
	return 0;
}

/*===============================================
 private functions
 ===============================================*/

static void prvSocketCounter_Service(void *pvArgs)
{
	char pcMsg[32];
	SocketCounterActionArgs_t xSelectArg = {"/count", pcMsg, 0};
	while (true)
	{
		xSelectArg.xMsgLen = snprintf(pcMsg, sizeof(pcMsg), "{\"count\":%d}", prvCount);
		Ember_SelectClients(prvUpdateWebSockets, &xSelectArg);
		vTaskDelay(1000);
		prvCount++;
	}
}

static BaseType_t prvUpdateWebSockets(void *pxc, void *pvArg)
{
	WebsocketClient_t *pxClient = (WebsocketClient_t *)pxc;
	SocketCounterActionArgs_t *pxSockArgs = (SocketCounterActionArgs_t *)pvArg;
	if ((pxClient->xWork == xWebsocketWork) && (strcasecmp(pxClient->pcRoute, pxSockArgs->pcPath) == 0))
	{
		return xSendWebsocketTextMessage(pxc, pxSockArgs->pcMsg, pxSockArgs->xMsgLen);
	}
	return 0;
}
