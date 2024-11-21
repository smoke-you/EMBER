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

#ifndef EMBER_V0_0_INC_FTPD_H_
#define EMBER_V0_0_INC_FTPD_H_

/*===============================================
 includes
 ===============================================*/

#include "./ember_private.h"

/*===============================================
 public constants
 ===============================================*/

#define FTPD_CLIENT_SZ (sizeof(FTPClient_t))
#define FTPD_CREATOR_METHOD (NULL)
#define FTPD_WORKER_METHOD (xFtpWork)
#define FTPD_DELETE_METHOD (xFtpDelete)

/*===============================================
 public data prototypes
 ===============================================*/

/**
 * @struct xFTP_CLIENT
 * @brief FTP client data record. Inherits from `TCPClient_t` via the
 *   `TCP_CLIENT_PROPERTIES` macro.
 */
struct xFTP_CLIENT
{
	/* This define contains fields which must come first within each of the client structs */
	TCP_CLIENT_PROPERTIES;
	/* --- Keep at the top  --- */
	const char *pcRootDir;
	uint32_t ulRestartOffset;
	uint32_t ulRecvBytes;
	size_t uxBytesLeft; /* Bytes left to send */
	uint32_t ulClientIP;
	TickType_t xStartTime;
	uint16_t usClientPort;
	Socket_t xTransferSocket;
	BaseType_t xTransType;
	BaseType_t xDirCount;
	FF_FindData_t xFindData;
	FF_FILE *pxReadHandle;
	FF_FILE *pxWriteHandle;
	char pcCurrentDir[ffconfigMAX_FILENAME];
	char pcFileName[ffconfigMAX_FILENAME];
	char pcConnectionAck[ffconfigMAX_FILENAME];
	char pcClientAck[ffconfigMAX_FILENAME];
	union
	{
		struct
		{
			uint32_t
				bHelloSent : 1,
				bLoggedIn : 1,
				bStatusUser : 1,
				bInRename : 1,
				bReadOnly : 1;
		};
		uint32_t ulFTPFlags;
	} bits;
	union
	{
		struct
		{
			uint32_t
				bIsListen : 1,		  /* pdTRUE for passive data connections (using list()). */
				bDirHasEntry : 1,	  /* pdTRUE if ff_findfirst() was successful. */
				bClientConnected : 1, /* pdTRUE after connect() or accept() has succeeded. */
				bEmptyFile : 1,		  /* pdTRUE if a connection-without-data was received. */
				bHadError : 1;		  /* pdTRUE if a transfer got aborted because of an error. */
		};
		uint32_t ulConnFlags;
	} bits1;
};
typedef struct xFTP_CLIENT FTPClient_t;

/*===============================================
 public function prototypes
 ===============================================*/

/**
 * @fn BaseType_t xFtpWork(void*)
 * @brief FTP client work function. Called periodically by EMBER for each
 *   `FTPClient_t` instance in its list of current `TCPClient_t` instances.
 *
 * @param pxTCPClient An anonymized `FTPClient_t` instance.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xFtpWork(void *pxTCPClient);

/**
 * @fn BaseType_t xFtpDelete(void*)
 * @brief FTP client deletion function. Called by EMBER to close any directories
 *   and/or files opened by an `FTPClient_t` instance.
 *
 * @post The `FTPClient_t` instance's files and directories are closed.
 * @param pxc An anonymized `FTPClient_t` instance.
 * @return 0
 */
BaseType_t xFtpDelete(void *pxc);

#endif /* EMBER_V0_0_INC_FTPD_H_ */
