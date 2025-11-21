# EMBER - An <ins>EMB</ins>edded C Webserv<ins>ER</ins>

EMBER is an embedded C webserver, or perhaps more correctly a web *framework*, as it designed to be easily extended.  It was developed primarily because the author wanted to use a dynamic webpage to provide a GUI for an amusement project based on an [ST Nucleo-F439ZI](https://www.st.com/en/evaluation-tools/nucleo-f439zi.html).

See [here](docs/EMBER_getting_started.md) to get started.

## Background

The backing project is written in C and uses FreeRTOS, and various FreeRTOS Plus and Lab components including the FreeRTOS+TCP and FreeRTOS+FAT libraries.  I explored a number of open-source options for a simple webserver that I could use to provide a GUI, but was disappointed to find nothing suitable. The FreeRTOS demonstration TCP server was the best of the lot, but in terms of HTTP support it could only serve static web pages; only supported the GET verb; and did not support websockets at all. In order to provide a responsive GUI, I wanted a solution that supported both POST and websockets. I set out to adapt the FreeRTOS demonstration server to my needs; EMBER is the result.

I've decided to go to the trouble of publishing it because if I couldn't find anything halfway suitable for my use-case then chances are neither can anyone else for similar use-cases.

## Language

EMBER is written in C (not C++) and is procedural, but it does employ some object-oriented design principles.

## Components

At the time of writing, EMBER is provided with three protocol daemons:
* [ftpd](docs/FTPD.md)
* [httpd](docs/HTTPD.md)
* [websocketd](docs/WEBSOCKETD.md)

ftpd is an almost unmodified copy of the FreeRTOS demonstration ftp server, and I claim no credit for it whatsoever. Any changes that I have made to the original code were made only to adapt it to my modified TCP server design, as it did everything that I needed out of the box.

httpd is based on the FreeRTOS demonstration http server, but it has been completely re-written in order to add support for dynamically-constructed endpoints; handling any HTTP verb; and upgrading HTTP connections to websockets.

websocketd is entirely my own design, although it is based on the protocol architecture developed by FreeRTOS.

## Capabilities

* An FTP server daemon, provided by FreeRTOS.
* An HTTP server daemon that is able to:
  * Direct incoming requests to individual route handlers.
  * Optionally select route handlers by wildcard.
  * Handle URL parameters (not extensively tested).
  * Handle any HTTP verb (only GET and POST tested).
  * Respond to requests with static (e.g. filesystem) or dynamically generated content.
  * Perform HTTP connection upgrades to websocket connections.
* A Websocket server daemon that is able to:
  * Respond to ping messages.
  * Receive and transmit text and binary messages.
  * Push messages to connected clients; message pushes may be triggered by any FreeRTOS task.

## Limitations

There are really too many to list, as the framework is far from complete (and likely never will be). A few include:
* No ability to control which network interface is used.
* No pretence of providing, or being designed for, any security.
* No TLS support at all, so no support for https, wss or sftp.
* Incomplete implementations of almost everything.
* No HTTP sessions or cookie support.
* Limited transmit and receive buffer sizes, defaulting to 2kB, that (may, in some circumstances) constrain the maximum size of Websocket and HTTP POST messages both to and from the server.

I hope that some of these (and other) limitations will be addressed as time permits.

## Dependencies

EMBER itself relies upon the FreeRTOS kernel, and FreeRTOS TCP. At the time of writing, the specific versions were:
| Library | Version |
| :-- | --: |
| [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | [11.1.0](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0) |
| [FreeRTOS TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP) | [4.2.2](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/tree/V4.2.2) |

Additionally, because EMBER uses FreeRTOS mutexes, `FreeRTOSConfig.h` must include:
```
#define configUSE_MUTEXES      (1)
```

## Compatibility

EMBER has been tested with FreeRTOS 11.1.0 and FreeRTOS TCP 4.2.2, running on an ST Nucleo-439ZI. It *should* work with any otherwise-functional FreeRTOS+TCP+FAT project.

## Operation

The principles underpinning the framework were inherited from the [FreeRTOS Plus TCP server demonstration project](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS-Plus/Demo/Common/Demo_IP_Protocols).

EMBER is managed by a FreeRTOS task. During each iteration of the task loop, it:

* Listens on the TCP ports configured for each protocol daemon and creates corresponding protocol client instances for incoming connections on those ports.
  * The subclass of protocol client that is created depends on the TCP port that the connection is made to.
  * The client is created by a creator function associated with the protocol, allowing additional protocol-specific configuration of the connection to be carried out.
* Executes the associated protocol worker function for each existing client connection.
  * Client connection worker methods return an signed integer (a `BaseType_t` in FreeRTOS terms). If the method returns a negative value (representing an error), the connection is closed and the client instance is deleted.
  * Deletion of clients is facilitated by a protocol-specific deletion function that e.g. releases any open file handles.

Accesses (for creation, deletion, and some other purposes) to the list of connected clients are protected by a mutex to manage concurrency risks.

## Example Code

Examples of an EMBER configuration and a websocket server task can be found [here](/example/). Note that the example is in no way a complete, working project. Its purpose is to demonstrate configuring EMBER with: 

* An FTP server.
* A set of HTTP routes, including a route that performs a websocket upgrade.
* A websocket message handler.
* A websocket server task that is able to push websocket messages to connected clients.

`socket_counter.c` in particular has a dependency on [coreJSON](https://github.com/FreeRTOS/coreJSON/tree/main).

The example also relies upon a FreeRTOS+FAT filesystem being available.

A simple example [HTTP source directory](./example/web) is included. The [example code](./example/ember_config.c) relies upon the example HTTP source directory being copied (presumably using FTP) into the root of a FreeRTOS+FAT volume called "spidisk".

An FTP server without a filesystem is also not terribly useful.

