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

#ifndef _EMBER_CONFIG_DEFAULTS_H_
#define _EMBER_CONFIG_DEFAULTS_H_

/*===============================================
 includes
 ===============================================*/

#include <ember_config.h>

/*===============================================
 public constants
 ===============================================*/

/**
 * @def emberTCP_RCV_BUFFER_SIZE
 * @brief The size (in bytes) of the EMBER TCP receive buffer
 */
#define	emberTCP_RCV_BUFFER_SIZE   (2048)

/**
 * @def emberTCP_SND_BUFFER_SIZE
 * @brief The size (in bytes) of the EMBER TCP send buffer
 */
#define	emberTCP_SND_BUFFER_SIZE   (2048)

/**
 * @def emberHTTP_ROUTE_PARTS
 * @brief The (maximum + 1) number of request URL parts that can make up a
 *   route
 */
#define	emberHTTP_ROUTE_PARTS      (9)

/**
 * @def emberHTTP_PARAM_PARTS
 * @brief The (maximum + 1) number of parameters in any request URL
 */
#define	emberHTTP_PARAM_PARTS      (9)

/**
 * @def emberHTTP_HEADER_PARTS
 * @brief The (maximum + 1) number of headers (of interest) in any HTTP request
 */
#define	emberHTTP_HEADER_PARTS     (10)


#endif /* _EMBER_CONFIG_DEFAULTS_H_ */
