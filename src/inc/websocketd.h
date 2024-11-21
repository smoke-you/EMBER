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

#ifndef EMBER_V0_0_INC_WEBSOCKETD_H_
#define EMBER_V0_0_INC_WEBSOCKETD_H_

/*===============================================
 includes
 ===============================================*/

#include "./ember_private.h"

/*===============================================
 public constants
 ===============================================*/

#define WEBSOCKETD_CREATOR_METHOD (NULL)
#define WEBSOCKETD_WORKER_METHOD (xWebsocketWork)
#define WEBSOCKETD_DELETE_METHOD (NULL)

/*===============================================
 public data prototypes
 ===============================================*/

/**
 * @fn BaseType_t (*WebsocketMessageHandler)(void*)
 * @brief Signature for websocket message handler functions.
 */
typedef BaseType_t (*WebsocketMessageHandler_t)(void *);

/**
 * @struct xWEBSOCKET_FLAGS
 * @brief The first two bytes of all websocket messages. Common to all types of
 *   websocket message header.
 *
 */
struct __attribute__((packed)) xWEBSOCKET_FLAGS
{
	// byte[0]
	unsigned opcode : 4;
	unsigned rsv : 3;
	unsigned fin : 1;
	// byte[1]
	unsigned payLen : 7;
	unsigned mask : 1;
};
typedef struct xWEBSOCKET_FLAGS WebsocketFlags_t;

/**
 * @struct xWEBSOCKET_CLIENT
 * @brief Websocket client record. Inherits from `TCPClient_t` via the
 *   `TCP_CLIENT_PROPERTIES` macro.
 *
 * The websocket client struct overwrites what was originally an HTTP
 * client struct. The TCP_CLIENT_PROPERTIES sections are placed identically,
 * but the HTTP struct has a considerable amount of unused space compared to
 * the websocket struct. At time of writing, the HTTP struct had 444(?) bytes
 * over and above the TCP_CLIENT_PROPERTIES, compared to 156 for the
 * websocket struct.
 */
struct xWEBSOCKET_CLIENT
{
	TCP_CLIENT_PROPERTIES;
	/* --- Keep at the top  --- */
	struct xWEBSOCKET_FLAGS xFlags;
	uint16_t usMaskKeyL;
	uint16_t usMaskKeyU;
	int64_t xPayloadSz;
	char *pcPayload;
	char pcRoute[ffconfigMAX_FILENAME];
	WebsocketMessageHandler_t pxTxtHandler;
	WebsocketMessageHandler_t pxBinHandler;
};
typedef struct xWEBSOCKET_CLIENT WebsocketClient_t;

/**
 * @enum eWebsocketOpcode
 * @brief Enumeration of websocket message opcodes.
 */
typedef enum
{
	eWSOp_Continue = 0, /**< eWSOp_Continue */
	eWSOp_Text = 1,		/**< eWSOp_Text */
	eWSOp_Binary = 2,	/**< eWSOp_Binary */
	eWSOp_Close = 8,	/**< eWSOp_Close */
	eWSOp_Ping = 9,		/**< eWSOp_Ping */
	eWSOp_Pong = 10,	/**< eWSOp_Pong */
} eWebsocketOpcode;

/**
 * @union xWEBSOCKET_SND_HEADER
 * @brief Basic websocket send (transmit) header. Used for messages with
 *   size <= 125 bytes.
 */
union xWEBSOCKET_SND_HEADER
{
	uint8_t pucBytes[2];
	WebsocketFlags_t xFlags;
};
typedef union xWEBSOCKET_SND_HEADER WebsocketSendHeader_t;

/**
 * @union xWEBSOCKET_SND_HEADER_X16
 * @brief 16b extended websocket send (transmit) header. Used for messages with
 *   65535 <= size < 125 bytes.
 */
union xWEBSOCKET_SND_HEADER_X16
{
	uint8_t bytes[4];
	struct __attribute__((packed))
	{
		WebsocketFlags_t xFlags;
		uint16_t usPayLenX16;
	};
};
typedef union xWEBSOCKET_SND_HEADER_X16 WebsocketSendHeaderX16_t;

/**
 * @union xWEBSOCKET_HEADER
 * @brief Basic websocket receive header. Used for messages with size <= 125
 *   bytes.
 */
union xWEBSOCKET_HEADER
{
	uint8_t pucBytes[6];
	struct __attribute__((packed))
	{
		WebsocketFlags_t xFlags;
		uint16_t usMaskKeyL;
		uint16_t usMaskKeyU;
	};
};
typedef union xWEBSOCKET_HEADER WebsocketHeader_t;

/**
 * @union xWEBSOCKET_HEADER_X16
 * @brief 16b extended websocket receive header. Used for messages with
 *   65535 <= size < 125 bytes.
 */
union xWEBSOCKET_HEADER_X16
{
	uint8_t pucBytes[8];
	struct __attribute__((packed))
	{
		WebsocketFlags_t xFlags;
		uint16_t usPayLenX16;
		uint16_t usMaskKeyL;
		uint16_t usMaskKeyU;
	};
};
typedef union xWEBSOCKET_HEADER_X16 WebsocketHeaderX16_t;

/**
 * @union xWEBSOCKET_HEADER_X64
 * @brief 64b extended websocket receive header. Used for messages with
 *   2^64 <= size < 65535 bytes.
 */
union xWEBSOCKET_HEADER_X64
{
	uint8_t pucBytes[16];
	struct __attribute__((packed))
	{
		WebsocketFlags_t xFlags;
		uint16_t usPayLenX16;
		uint32_t usPayLenX48;
		uint16_t usMaskKeyL;
		uint16_t usPayLenX64;
		uint16_t usMaskKeyU;
		unsigned : 16;
	};
};
typedef union xWEBSOCKET_HEADER_X64 WebsocketHeaderX64_t;

/**
 * @enum eWebsocketStatus
 * @brief Enumeration of possible websocket close status codes.
 */
typedef enum
{
	eWS_NORMAL_CLOSURE = 1000,		 /**< eWS_NORMAL_CLOSURE */
	eWS_GOING_AWAY = 1001,			 /**< eWS_GOING_AWAY */
	eWS_PROTOCOL_ERROR = 1002,		 /**< eWS_PROTOCOL_ERROR */
	eWS_UNSUPPORTED_DATA = 1003,	 /**< eWS_UNSUPPORTED_DATA */
	eWS_NO_STATUS_RCDV = 1005,		 /**< eWS_NO_STATUS_RCDV */
	eWS_ABNORMAL_CLOSURE = 1006,	 /**< eWS_ABNORMAL_CLOSURE */
	eWS_INVALID_PAYLOAD_DATA = 1007, /**< eWS_INVALID_PAYLOAD_DATA */
	eWS_POLICY_VIOLATION = 1008,	 /**< eWS_POLICY_VIOLATION */
	eWS_MESSAGE_TOO_BIG = 1009,		 /**< eWS_MESSAGE_TOO_BIG */
	eWS_MANDATORY_EXTENSION = 1010,	 /**< eWS_MANDATORY_EXTENSION */
	eWS_INTERNAL_ERROR = 1011,		 /**< eWS_INTERNAL_ERROR */
	eWS_SERVICE_RESTART = 1012,		 /**< eWS_SERVICE_RESTART */
	eWS_TRY_AGAIN_LATER = 1013,		 /**< eWS_TRY_AGAIN_LATER */
	eWS_BAD_GATEWAY = 1014,			 /**< eWS_BAD_GATEWAY */
} eWebsocketStatus;

/**
 * @struct xWEBSOCKET_STATUS_DESC
 * @brief Mapping of websocket close status code enumerated at
 *   `eWebsocketStatus` to corresponding text description.
 */
struct xWEBSOCKET_STATUS_DESC
{
	BaseType_t xLen;
	const char *const pcText;
	BaseType_t xStatus;
};
typedef struct xWEBSOCKET_STATUS_DESC WebsocketStatusDescriptor_t;
static const WebsocketStatusDescriptor_t WebsocketStatuses[] = {
	{14, "Normal closure", eWS_NORMAL_CLOSURE},
	{10, "Going away", eWS_GOING_AWAY},
	{14, "Protocol error", eWS_PROTOCOL_ERROR},
	{16, "Unsupported data", eWS_UNSUPPORTED_DATA},
	{18, "No status received", eWS_NO_STATUS_RCDV},
	{16, "Abnormal closure", eWS_ABNORMAL_CLOSURE},
	{20, "Invalid payload data", eWS_INVALID_PAYLOAD_DATA},
	{16, "Policy violation", eWS_POLICY_VIOLATION},
	{15, "Message too big", eWS_MESSAGE_TOO_BIG},
	{19, "Mandatory extension", eWS_MANDATORY_EXTENSION},
	{14, "Internal error", eWS_INTERNAL_ERROR},
	{15, "Service restart", eWS_SERVICE_RESTART},
	{15, "Try again later", eWS_TRY_AGAIN_LATER},
	{11, "Bad gateway", eWS_BAD_GATEWAY},
	{0, "", -1},
};
static const BaseType_t numWebsocketStatusDescs = sizeof(WebsocketStatuses) / sizeof(WebsocketStatusDescriptor_t);
static const WebsocketStatusDescriptor_t *defaultWebsocketStatus =
	&WebsocketStatuses[1];

/*===============================================
 public function prototypes
 ===============================================*/

/**
 * @fn BaseType_t xWebsocketWork(void*)
 * @brief Websocket client work function. Called periodically by EMBER for each
 *   `WebsocketClient_t` instance in its list of current `TCPClient_t` instances.
 *
 * @param pxc An anonymized `WebsocketClient_t` instance.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xWebsocketWork(void *pxc);

/**
 * @fn const WebsocketStatusDescriptor_t* const
 *   pxPrintWebsocketStatusMessage(const BaseType_t)
 * @brief Find a status descriptor (text) corresponding to a Websocket status
 *   code.
 *
 * @param xStatus The status code to search for
 * @return A pointer to a `WebsocketStatusDescriptor_t` that includes the status
 *   text.
 */
const WebsocketStatusDescriptor_t *const pxGetWebsocketStatusMessage(
	const BaseType_t xStatus);

/**
 * @fn BaseType_t xSendWebsocketHeader(void*, const eWebsocketOpcode, const size_t)
 * @brief Construct and transmit a websocket message header.
 *
 * @post The `WebsocketClient_t` is ready to transmit a message body.
 * @param pxc An anonymized `WebsocketClient_t` instance.
 * @param eCode The websocket message opcode.
 * @param uxPayloadSz The size of the following message body.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendWebsocketHeader(
	void *pxc,
	const eWebsocketOpcode eCode,
	const size_t uxPayloadSz);

/**
 * @fn BaseType_t xSendWebsocketPayload(void*, const void*, const size_t)
 * @brief Transmit a block of a websocket message body. Note that total size of
 *   all transmitted blocks must match the `uxPayloadSz` parameter of the
 *   preceding `xSendWebsocketHeader` call.
 *
 * @pre xSendWebsocketHeader should have been sent immediately prior.
 * @param pxc An anonymized `WebsocketClient_t` instance.
 * @param pvPayload The payload to be transmitted.
 * @param uxLen The size of the payload to be transmitted.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendWebsocketPayload(
	void *pxc,
	const void *pvPayload,
	const size_t uxLen);

/**
 * @fn BaseType_t xSendWebsocketTextMessage(void*, const char*, const size_t)
 * @brief Transmit a websocket message of type "Text" (1).
 *
 * @param pxc An anonymized `WebsocketClient_t` instance.
 * @param pcMsg The payload to be transmitted.
 * @param uxLen The size of the payload to be transmitted.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendWebsocketTextMessage(
	void *pxc,
	const char *pcMsg,
	const size_t uxLen);

/**
 * @fn BaseType_t xSendWebsocketBinaryMessage(void*, const char*, const size_t)
 * @brief Transmit a websocket message of type "Binary" (2).
 *
 * @param pxc An anonymized `WebsocketClient_t` instance.
 * @param pcMsg The payload to be transmitted.
 * @param uxLen The size of the payload to be transmitted.
 * @return
 *   < 0 if an error occurred
 *   = 0 if no error occurred and no data was transmitted
 *   > 0 the number of bytes transmitted
 */
BaseType_t xSendWebsocketBinaryMessage(
	void *pxc,
	const char *pcMsg,
	const size_t uxLen);

#endif /* EMBER_V0_0_INC_WEBSOCKETD_H_ */
