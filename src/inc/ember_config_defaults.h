#ifndef _EMBER_CONFIG_DEFAULTS_H_
#define _EMBER_CONFIG_DEFAULTS_H_

/*===============================================
 includes
 ===============================================*/

#include	<ember_config.h>

/*===============================================
 public constants
 ===============================================*/

/**
 * @def emberSTACK_SIZE
 * @brief The size (in bytes) of the EMBER task's stack
 */
#ifndef emberSTACK_SIZE
#define	emberSTACK_SIZE            (2048)
#endif

/**
 * @def emberSTARTUP_DELAY_MS
 * @brief The delay (in ms) after the EMBER task starts before it starts to interact with the network
 */
#ifndef emberSTARTUP_DELAY_MS
#define	emberSTARTUP_DELAY_MS      (3000)
#endif

/**
 * @def emberPERIOD_MS
 * @brief The periodicity (in ms) with which the EMBER task will action clients
 */
#ifndef emberPERIOD_MS
#define	emberPERIOD_MS             (10)
#endif

/**
 * @def emberTCP_RCV_BUFFER_SIZE
 * @brief The size (in bytes) of the EMBER TCP receive buffer
 */
#ifndef emberTCP_RCV_BUFFER_SIZE
#define	emberTCP_RCV_BUFFER_SIZE   (2048)
#endif

/**
 * @def emberTCP_SND_BUFFER_SIZE
 * @brief The size (in bytes) of the EMBER TCP send buffer
 */
#ifndef emberTCP_SND_BUFFER_SIZE
#define	emberTCP_SND_BUFFER_SIZE   (2048)
#endif

/**
 * @def emberHTTP_ROUTE_PARTS
 * @brief The (maximum + 1) number of request URL parts that can make up a
 *   route
 */
#ifndef emberHTTP_ROUTE_PARTS
#define	emberHTTP_ROUTE_PARTS      (9)
#endif

/**
 * @def emberHTTP_PARAM_PARTS
 * @brief The (maximum + 1) number of parameters in any request URL
 */
#ifndef emberHTTP_PARAM_PARTS
#define	emberHTTP_PARAM_PARTS      (9)
#endif

/**
 * @def emberHTTP_HEADER_PARTS
 * @brief The (maximum + 1) number of headers (of interest) in any HTTP request
 */
#ifndef emberHTTP_HEADER_PARTS
#define	emberHTTP_HEADER_PARTS     (10)
#endif

/**
 * @def emberHTTP_FILE_CHUNK_SIZE
 * @brief The approximate number of bytes that will be uploaded for a file before
 * cooperatively switching to another client.
 */
#ifndef emberHTTP_FILE_CHUNK_SIZE
#define emberHTTP_FILE_CHUNK_SIZE  (20*1024)
#endif


#endif /* _EMBER_CONFIG_DEFAULTS_H_ */
