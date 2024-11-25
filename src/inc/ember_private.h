/*
 * Copyright (C) Mark R. Turner.  All Rights Reserved.
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

#ifndef EMBER_V0_0_INC_EMBER_PRIVATE_H_
#define EMBER_V0_0_INC_EMBER_PRIVATE_H_

/*===============================================
 includes
 ===============================================*/

#include <FreeRTOS.h>
#include <FreeRTOS_IP.h>
#include <ff_stdio.h>
#include "./ember.h"

/*===============================================
 public constants
 ===============================================*/

#define	FREERTOS_NO_SOCKET					(0)

#define TCP_CLIENT_PROPERTIES        \
	struct xTCP_SERVER *pxParent;      \
	Socket_t xSock;                   \
	xTCPClientCreate xCreator;         \
	xTCPClientWorker xWork;            \
	xTCPClientDelete xDelete;          \
	struct xTCP_CLIENT *pxPrevClient;  \
	struct xTCP_CLIENT *pxNextClient

/*===============================================
 public data prototypes
 ===============================================*/

/* --------------------------------------------
 * GENERIC TCP CLIENT DEFINITIONS
 * -----------------------------------------**/

typedef BaseType_t (*xTCPClientCreate)(void*);
typedef BaseType_t (*xTCPClientWorker)(void*);
typedef BaseType_t (*xTCPClientDelete)(void*);

struct xTCP_CLIENT {
	TCP_CLIENT_PROPERTIES
		;
};
typedef struct xTCP_CLIENT TCPClient_t;

/* --------------------------------------------
 * GENERIC TCP SERVER DEFINITIONS
 * -----------------------------------------**/

struct xWEBPROTO_SERVER {
	struct xTCP_SERVER *pxParent;
	const char *pcRootDir;
	size_t uxClientSz;
	xTCPClientCreate xCreator;
	xTCPClientWorker xWorker;
	xTCPClientDelete xDelete;
	Socket_t xSock;
};
typedef struct xWEBPROTO_SERVER WebProtoServer_t;

struct xTCP_SERVER {
	SocketSet_t xSockSet;
	char pcRcvBuff[emberTCP_RCV_BUFFER_SIZE];
	char pcSndBuff[emberTCP_SND_BUFFER_SIZE];
	char pcNewDir[ffconfigMAX_FILENAME];
	char pcContentsType[40];
	SemaphoreHandle_t xClientMutex;
	TCPClient_t *pxClients;
	size_t uxNumProtocols;
	/* The `protocols` field _must_ be the last field for this struct, as the array
	 * may be increased in size.*/
	WebProtoServer_t pxProtocols[1];
};
typedef struct xTCP_SERVER TCPServer_t;

struct xWEBPROTO_CONFIG {
	BaseType_t xPortNum;
	BaseType_t xBacklog;
	const char *pcRootDir;
	size_t uxClientSz;
	xTCPClientCreate xCreator;
	xTCPClientWorker xWorker;
	xTCPClientDelete xDelete;
};
typedef struct xWEBPROTO_CONFIG WebProtoConfig_t;

struct xTCP_SERVER_CONFIG {
	const size_t uxNumProtocols;
	const WebProtoConfig_t *const pxProtocols;
};
typedef struct xTCP_SERVER_CONFIG TCPServerConfig_t;

/*===============================================
 public function prototypes
 ===============================================*/

#endif /* EMBER_V0_0_INC_EMBER_PRIVATE_H_ */
