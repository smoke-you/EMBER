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
#include "inc/ember_private.h"
#include "inc/ember.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

typedef struct {
	BaseType_t xReady;
	const configSTACK_DEPTH_TYPE uxStackSz;
	const TickType_t uxStartupDelay;
	const TickType_t uxPeriod;
	TaskHandle_t xPid;
	const TCPServerConfig_t *const pxWebConfig;
	TCPServer_t *pxServer;
} EmberConfig_t;

/*===============================================
 private function prototypes
 ===============================================*/

/* Task for the Ember server */
static void prvEmber_Service(void *args);

static TCPServer_t* prvCreateTCPServer(
    const TCPServerConfig_t *const pxServerCfg);
static BaseType_t prvCreateProtocolServer(
    WebProtoServer_t *pxProto,
    const WebProtoConfig_t *pxProtoCfg);
static void prvTCPServerWork(void);
static void prvAcceptNewClient(WebProtoServer_t *pxProto, Socket_t xNewSock);
static TCPClient_t* prvRemoveClient(TCPClient_t *pxClient);
static TCPClient_t* prvDropClient(TCPClient_t *pxClient);

/*===============================================
 external objects
 ===============================================*/

extern const TCPServerConfig_t xWebProtoConfig __attribute__ ((weak, alias ("_xWebProtoConfig")));
/* the `xWebProtoConfig` object should be located elsewhere in source, e.g. in ember_config.c
 * */
static const TCPServerConfig_t _xWebProtoConfig = { 0, 0 };

/*===============================================
 private global variables
 ===============================================*/

static EmberConfig_t xEmber =
    { pdFALSE, 2048, 3000, 10, 0, &xWebProtoConfig, 0 };

/*===============================================
 public functions
 ===============================================*/

void Ember_Init() {
	if (xEmber.xReady)
	  return;
	xTaskCreate(prvEmber_Service, (const char*) "Ember", xEmber.uxStackSz, 0,
	    tskIDLE_PRIORITY, &xEmber.xPid);
	xEmber.xReady = pdTRUE;
}

void Ember_DeInit() {
	if (!xEmber.xReady)
	  return;
	vTaskDelete(xEmber.xPid);
	xEmber.xPid = 0;
	xEmber.xReady = pdFALSE;
}

void Ember_SelectClients(BaseType_t (*xActionFunc)(void*, void*), void *pxArg) {
	TCPClient_t *pxCurrClient, *pxNextClient;
	BaseType_t xRc;
	if (!xEmber.xReady || !xEmber.pxServer || !xEmber.pxServer->pxClients)
	  return;
	xSemaphoreTake(xEmber.pxServer->xClientMutex, xEmber.uxPeriod * 2);
	pxCurrClient = xEmber.pxServer->pxClients;
	while (pxCurrClient) {
		pxNextClient = pxCurrClient->pxNextClient;
		xRc = xActionFunc(pxCurrClient, pxArg);
		if (xRc < 0)
		  prvDropClient(pxCurrClient);
		pxCurrClient = pxNextClient;
	}
	xSemaphoreGive(xEmber.pxServer->xClientMutex);
}

/*===============================================
 private functions
 ===============================================*/

static void prvEmber_Service(void *args) {
	vTaskDelay(xEmber.uxStartupDelay);
	xEmber.pxServer = prvCreateTCPServer(xEmber.pxWebConfig);
	if (!xEmber.pxServer) {
		xEmber.xPid = 0;
		xEmber.xReady = pdFALSE;
		return;
	}
	while (1) {
		prvTCPServerWork();
	}
}

/* TODO: servers should be created on a specific network interface, not just assigned to
 * the first one (or maybe all of them)
 * */
static TCPServer_t* prvCreateTCPServer(
    const TCPServerConfig_t *const pxServerCfg) {
	TCPServer_t *pxNewServer;
	SocketSet_t xSockSet;
	Socket_t xSock;
	struct freertos_sockaddr xSockAddr;
	BaseType_t xNoTimeout = 0;

	if (!pxServerCfg || pxServerCfg->uxNumProtocols <= 0)
	  return 0;
	xSockSet = FreeRTOS_CreateSocketSet();
	if (!xSockSet)
	  return 0;
	size_t uxServerSz = sizeof(TCPServer_t)
	    + ((pxServerCfg->uxNumProtocols - 1) * sizeof(WebProtoServer_t));
	pxNewServer = pvPortMalloc(uxServerSz);
	if (!pxNewServer) {
		FreeRTOS_DeleteSocketSet(xSockSet);
		return 0;
	}
	memset(pxNewServer, 0, uxServerSz);
	pxNewServer->xSockSet = xSockSet;
	for (BaseType_t i = 0; i < pxServerCfg->uxNumProtocols; i++) {
		if (prvCreateProtocolServer(&pxNewServer->pxProtocols[i],
		    &pxServerCfg->pxProtocols[i]) == pdTRUE) {
			xSock = pxNewServer->pxProtocols[i].xSock;
			pxNewServer->pxProtocols[i].pxParent = pxNewServer;
			// TODO: need to specify the network interface, not just get the first one!
			xSockAddr.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();
			xSockAddr.sin_port = FreeRTOS_htons(pxServerCfg->pxProtocols[i].xPortNum);
			xSockAddr.sin_family = FREERTOS_AF_INET;
			FreeRTOS_bind(xSock, &xSockAddr, sizeof(xSockAddr));
			FreeRTOS_listen(xSock, pxServerCfg->pxProtocols[i].xBacklog);
			FreeRTOS_setsockopt(xSock, 0, FREERTOS_SO_RCVTIMEO, (void*) &xNoTimeout,
			    sizeof(xNoTimeout));
			FreeRTOS_setsockopt(xSock, 0, FREERTOS_SO_SNDTIMEO, (void*) &xNoTimeout,
			    sizeof(xNoTimeout));
			FreeRTOS_FD_SET(xSock, xSockSet, eSELECT_READ | eSELECT_EXCEPT);
		}
	}
	pxNewServer->uxNumProtocols = pxServerCfg->uxNumProtocols;
	pxNewServer->pcRcvBuff[0] = 0;
	pxNewServer->xClientMutex = xSemaphoreCreateMutex();
	return pxNewServer;
}

static BaseType_t prvCreateProtocolServer(
    WebProtoServer_t *pxProto,
    const WebProtoConfig_t *pxProtoCfg) {
	memset(pxProto, 0, sizeof(pxProto));
	Socket_t xSock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
	FREERTOS_IPPROTO_TCP);
	if (!xSock)
	  return pdFALSE;
	pxProto->pcRootDir = pxProtoCfg->pcRootDir;
	pxProto->xSock = xSock;
	pxProto->uxClientSz = pxProtoCfg->uxClientSz;
	pxProto->xCreator = pxProtoCfg->xCreator;
	pxProto->xWorker = pxProtoCfg->xWorker;
	pxProto->xDelete = pxProtoCfg->xDelete;
	return pdTRUE;
}

static void prvTCPServerWork(void) {
	TCPClient_t *currClient;
	BaseType_t xRc;
	if (!xEmber.xReady || !xEmber.pxServer)
	  return;
	// check for new connections/clients
	xRc = FreeRTOS_select(xEmber.pxServer->xSockSet, xEmber.uxPeriod);
	if (xRc != 0) {
		for (BaseType_t i = 0; i < xEmber.pxServer->uxNumProtocols; i++) {
			struct freertos_sockaddr sockAddr;
			Socket_t newSock;
			socklen_t newSockLen = sizeof(sockAddr);
			WebProtoServer_t *currProto = &xEmber.pxServer->pxProtocols[i];
			if (currProto->xSock == FREERTOS_NO_SOCKET)
			  continue;
			newSock = FreeRTOS_accept(currProto->xSock, &sockAddr, &newSockLen);
			if (newSock != FREERTOS_INVALID_SOCKET && newSock != FREERTOS_NO_SOCKET)
			  prvAcceptNewClient(currProto, newSock);
		}
	}
	// service existing connections/clients
	currClient = xEmber.pxServer->pxClients;
	while (currClient) {
		BaseType_t sockLive = FreeRTOS_issocketconnected(currClient->xSock);
		if (sockLive == pdTRUE) {
			xRc = currClient->xWork(currClient);
			if (xRc < 0)
			currClient = prvRemoveClient(currClient);
			else
			currClient = currClient->pxNextClient;
		}
		else {
			currClient = prvRemoveClient(currClient);
		}
	}
}

static void prvAcceptNewClient(WebProtoServer_t *pxProto, Socket_t xNewSock) {
	xSemaphoreTake(xEmber.pxServer->xClientMutex, portMAX_DELAY);
	TCPClient_t *newClient = 0;
	// (try to) malloc enough space for a suitable new TCP client struct
	newClient = (TCPClient_t*) pvPortMalloc(pxProto->uxClientSz);
	if (!newClient) {
		FreeRTOS_closesocket(xNewSock);
		xSemaphoreGive(xEmber.pxServer->xClientMutex);
		return;
	}
	memset(newClient, 0, pxProto->uxClientSz);
	newClient->pxParent = pxProto->pxParent;
	newClient->xSock = xNewSock;
	newClient->xCreator = pxProto->xCreator;
	newClient->xWork = pxProto->xWorker;
	newClient->xDelete = pxProto->xDelete;
	// try to create the new client; if this fails, delete it and ditch
	if (newClient->xCreator && newClient->xCreator(newClient) != 0) {
		FreeRTOS_closesocket(xNewSock);
		vPortFree(newClient);
		xSemaphoreGive(xEmber.pxServer->xClientMutex);
		return;
	}
	// the new client will be the head of the linked list
	newClient->pxPrevClient = 0;
	newClient->pxNextClient = xEmber.pxServer->pxClients;
	if (newClient->pxNextClient)
	  newClient->pxNextClient->pxPrevClient = newClient;
	xEmber.pxServer->pxClients = newClient;
	// add the client socket to the server's socketset
	FreeRTOS_FD_SET(xNewSock, xEmber.pxServer->xSockSet,
	    eSELECT_READ | eSELECT_EXCEPT);
	xSemaphoreGive(xEmber.pxServer->xClientMutex);
}

static TCPClient_t* prvRemoveClient(TCPClient_t *pxClient) {
	if (!pxClient)
	  return 0;
	xSemaphoreTake(xEmber.pxServer->xClientMutex, portMAX_DELAY);
	TCPClient_t *pxNextClient = pxClient->pxNextClient;
	prvDropClient(pxClient);
	xSemaphoreGive(xEmber.pxServer->xClientMutex);
	return pxNextClient;
}

static TCPClient_t* prvDropClient(TCPClient_t *pxClient) {
	TCPClient_t *pxPrevClient = pxClient->pxPrevClient;
	TCPClient_t *pxNextClient = pxClient->pxNextClient;
	// close any system resources used by the client (file handles, typically)
	if (pxClient->xDelete)
	  pxClient->xDelete(pxClient);
	// close the client's socket
	if (pxClient->xSock != FREERTOS_NO_SOCKET) {
		FreeRTOS_FD_CLR(pxClient->xSock, xEmber.pxServer->xSockSet, eSELECT_ALL);
		FreeRTOS_closesocket(pxClient->xSock);
		pxClient->xSock = FREERTOS_NO_SOCKET;
	}
	// unlink the target client from the list
	if (pxPrevClient)
	  pxPrevClient->pxNextClient = pxNextClient;
	if (pxNextClient)
	  pxNextClient->pxPrevClient = pxPrevClient;
	// if the target client was the head of the list, point the head at the next entry
	// (which might be NULL)
	if (pxClient == pxClient->pxParent->pxClients)
	  pxClient->pxParent->pxClients = pxNextClient;
}
