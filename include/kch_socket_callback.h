#ifndef _KII_SOCKET_CALLBACK
#define _KII_SOCKET_CALLBACK

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kch_sock_code_t {
    /** Retrun this code when operation succeed. */
    KIISOCK_OK,

    /** Return this code when operation failed. */
    KIISOCK_FAIL,

    /** Return this code when operation is in progress.
     *
     *  SDK calls the callback again until the callbacks returns
     *  KIISOCK_OK or KIISOCK_FAIL.
     */
    KIISOCK_AGAIN
} kch_sock_code_t;

/** Callback for connecting socket to server.
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] socket_context context object.
 * @param [in] host host name.
 * @param [in] port port number.
 * @return If connection is succeeded, applications need to return
 * KIISOCK_OK. If connection is failed, applications need to
 * return KIISOCK_FAIL. If applications want to pend returning
 * success or fail, applications need to return KII_SOCKETC_AGAIN.
 */
typedef kch_sock_code_t
    (*KII_CB_SOCK_CONNECT)
    (void* sock_ctx, const char* host, unsigned int port);

/** Callback for sending data to server.
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] socket_context context object.
 * @param [in] buffer data to send server.
 * @param [in] length length of buffer.

 * @return If applications succeed to send data, applications need to
 * return KIISOCK_OK. If connection is failed. applications need
 * to return KII_SOCKETC__FAIL. If applications don't want to send
 * data, applications need to return KII_SOCKETC_AGAIN. In this case,
 * KiiThingSDK Embedded Core pass same data to this callback again.
 */
typedef kch_sock_code_t
    (*KII_CB_SOCK_SEND)
    (void* sock_ctx, const char* buffer, size_t length);

/** Callback for receiving data from server.
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] socket_context context object.
 * @param [out] buffer buffer to set receiving data.
 * @param [in] length_to_read buffer size.
 * @param [out] out_actual_length actual set data size.
 * @return If applications succeed to receive data and set the data to
 * buffer, applications need to return KIISOCK_OK. Applications
 * also set data size to out_actual_length. If applications fail,
 * applications need to return KII_HTTPC_FAIL. If applications want to
 * wait to receive data, applications need to return
 * KII_HTTPC_AGAIN. In this case, applications must not set receving
 * data to buffer if some data is received.
 */
typedef kch_sock_code_t
    (*KII_CB_SOCK_RECV)
    (void* sock_ctx, char* buffer, size_t length_to_read,
     size_t* out_actual_length);

/** Callback for closing socket.
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] socket_context context object.
 *
 * @return If applications succeed to close socket, applications need
 * to return KIISOCK_OK. If applications fail to close socket,
 * applications need to return KIISOCK_FAIL. If applications want
 * to pend returning success or fail, applications need to return
 * KII_SOCKETC_AGAIN.
 */
typedef kch_sock_code_t
    (*KII_CB_SOCK_CLOSE)(void* sock_context);


#ifdef __cplusplus
}
#endif

#endif /* _KII_SOCKET_CALBACK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */