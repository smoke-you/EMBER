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

#ifndef EMBER_V0_0_INC_EMBER_H_
#define EMBER_V0_0_INC_EMBER_H_

/*===============================================
 includes
 ===============================================*/

#include <FreeRTOS.h>
#include "./ember_config_defaults.h"

/*===============================================
 public constants
 ===============================================*/

/*===============================================
 public data prototypes
 ===============================================*/

/*===============================================
 public function prototypes
 ===============================================*/
/**
 * @fn Ember_Init()
 * @brief Start the Ember server if it is not running. Take no action if it is running.
 * */
void Ember_Init();

/**
 * @fn void Ember_DeInit()
 * @brief Stop the Ember server if it is running. Take no action if it is not running.
 */
void Ember_DeInit();

/**
 * @fn void Ember_SelectClients(BaseType_t(*)(void*, void*), void*)
 * @brief Execute a function with a common piece of data against each client
 * connection to the Ember server.
 *
 * @param xSelect A function with the signature `BaseType_t (*)(void*, void*)`
 * that will be executed against each client connection to the Ember server.
 * The first parameter is a pointer to the client connection. The second is a
 * pointer to the data.
 *
 * @param pvArg A pointer to the data that will be passed, as the second argument,
 * to `actionfunc` each time it is executed.
 */
void Ember_SelectClients(BaseType_t (*xSelect)(void *, void *), void *pvArg);

#endif // SOURCE_INCLUDE_EMBER_H_
