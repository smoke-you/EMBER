# HTTP Daemon

The EMBER httpd protocol server is based upon the FreeRTOS Plus TCP Server demonstration project.

In addition to the work required to port it to the modified TCP server architecture of EMBER, a significant amount of additional work has been carried out by the author to extend its feature set, including:

* Direct incoming requests to individual route handlers.
* Select route handler by wildcard or perfect match.
* Handle URL parameters.
* Handle any HTTP verb.
* Respond to requests with static (e.g. filesystem) content or dynamically generated content.
* Perform HTTP connection upgrades to websocket connections.

The inspiration for the EMBER framework was Python-based web frameworks, such as [FastAPI](https://fastapi.tiangolo.com/) and [Django](https://www.djangoproject.com/).  EMBER is far less sophisticated and capable than these frameworks, of course, but it is also far more sophisticated and capable than any of the open source embedded webservers that the author was able to locate.

See [here](./HTTPD_getting_started.md) to get started.

## Dependencies

| Library | Version |
| :-- | --: |
| EMBER | 0.0 |
| [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | [11.1.0](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0) |
| [FreeRTOS TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP) | [4.2.2](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/tree/V4.2.2) |
| [FreeRTOS Labs FAT](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT) | N/A |
| [fnmatch](https://github.com/lattera/freebsd/blob/master/lib/libc/gen/fnmatch.c) | N/A |
| [strcasestr](https://github.com/lattera/freebsd/blob/master/lib/libc/string/strcasestr.c) | N/A |
| [base64](https://github.com/JoeMerten/Stm32-Tools-Evaluation/blob/master/STM32Cube_FW_F4_V1.9.0/Middlewares/Third_Party/PolarSSL/library/base64.c) | N/A |
| [sha1](https://github.com/JoeMerten/Stm32-Tools-Evaluation/blob/master/STM32Cube_FW_F4_V1.9.0/Middlewares/Third_Party/PolarSSL/library/sha1.c) | N/A |

## Operation

httpd exposes the worker function `xHttpWork` that is periodically called by the EMBER task for each HTTP client connection. The worker function listens for and attempts to handle incoming HTTP requests. Where a request is well-formed and its route is recognized, a corresponding handler function is executed. Handler functions have the signature `BaseType_t (*)(void*)`, where the void pointer argument is an anonymised `HTTPClient_t` instance, and the return value is either an error (if negative) or the number of bytes transmitted in response to the request (if zero or positive).

## Limitations

Modern web browsers tend to want to open a single connection to a server; use that connection to transport multiple resources; and keep the connection open for future exchanges. This is a reasonable approach for high-capacity servers, but it is not practical for small embedded servers like EMBER. To prevent this undesirable behaviour, EMBER sends a response header `"Connection: close"` when responding to any HTTP request, instructing the browser/client to close the connection after the exchange. This means that one connection is opened per requested resource, so e.g. requesting a web page that loads additional resources (javascript files, css files, images, etc) will open multiple, possibly concurrent, connections.
