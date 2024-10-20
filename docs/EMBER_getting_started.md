# Getting started with EMBER

## Configuration Macros

You <ins>must</ins> have a file `ember_config.h` visible in the project include paths. The file may be blank, as [ember_config_defaults.h](inc/ember_config_defaults.h) will populate reasonable default configuration constants, but it <ins>must</ins> exist or the project will not compile.

EMBER configuration options and their associated #define macros are documented in `ember_config_defaults.h`.

## Configuration Objects

A configuration object, `xWebProtoConfig`, is used to specify which protocols should be available and how those protocols should be configured. If a public object of this name is not defined by the user, then a very limited default object (that simply rejects all connections) will be used instead.

`xWebProtoConfig` must be an instance of `TCPServerConfig_t`, which is made up of:

| Field Name | Description |
| --- | --- |
| `uxNumProtocols` | The number of protocols that it specifies |
| `pxProtocols` | An array of protocol configurations of type `WebProtoConfig_t` |

Each `WebProtoConfig_t` instance is made up of:
| Field Name | Description |
| --- | --- |
| `xPortNum` | The TCP port to listen on |
| `xBacklog` | The maximum number of client connections |
| `pcRootDir` | The root directory for client connections (really only relevant to FTP) |
| `uxClientSz` | The size (in bytes) of each client connection object, typically obtained using e.g. `sizeof(HTTPClient_t)` |
| `xCreator` | The creator method for client connection objects, or NULL for default creation. Should be the associated client class's creator function. |
| `xWorker` | The worker method for client connection objects. Should be the associated client class's worker function, e.g. `xHttpWork`. |
| `xDelete` | The delete method for client connection objects, or NULL for default deletion. Should be the associated client class's delete function, e.g. `xHttpDelete`. |

### Example Web Protocol Configuration

A typical `xWebProtoConfig` might look like:
```
const WebProtoConfig_t pxWebProtocols[] = {
  { 21, 4, "/", FTPD_CLIENT_SZ, FTPD_CREATOR_METHOD, FTPD_WORKER_METHOD, FTPD_WORKER_METHOD },
  { 80, 12, "/", HTTPD_CLIENT_SZ, HTTPD_CREATOR_METHOD, HTTPD_WORKER_METHOD, HTTPD_DELETE_METHOD },
};
const TCPServerConfig_t xWebProtoConfig = { 2, webProtocols };

```

It is thus trivial to change which listen port is used, or even to add multiple server instances listening on different ports.  However, please note that at this time all HTTP protocol servers share the same set of routes.

## Starting and Stopping EMBER

In order to start the EMBER server, call `Ember_Init()`.

In order to stop the EMBER server, call `Ember_DeInit()`.
