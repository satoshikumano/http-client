#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kii_http.h"
#include "kii_http_impl.h"

void kii_state_idle(kii_http* kii_http) {
  if (kii_http->host == NULL) {
    // Fallback to localhost
    kii_http->host = "localhost";
  }
  if (kii_http->method == NULL) {
    // Fallback to GET.
    kii_http->method = "GET";
  }
  memset(kii_http->read_buffer, '\0', READ_REQ_BUFFER_SIZE);
  kii_http->state = CONNECT;
  kii_http->resp_header_buffer_size = 0;
  kii_http->read_end = 0;
  kii_http->result = KIIE_OK;
  return;
}

void kii_state_connect(kii_http* kii_http) {
  kii_socket_code_t con_res = kii_http->sc_connect_cb(kii_http->socket_context, kii_http->host, 8080);
  if (con_res == KII_SOCKETC_OK) {
    kii_http->state = REQUEST_LINE;
    return;
  }
  if (con_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (con_res == KII_SOCKETC_FAIL) {
    kii_http->result = KIIE_SC_CONNECT;
    kii_http->state = FINISHED;
    return;
  }
}

static const char schema[] = "https://";
static const char http_version[] = "HTTP 1.0\r\n";

static size_t request_line_len(kii_http* kii_http) {
  char* method = kii_http->method;
  char* host = kii_http->host;
  // Path must be started with '/'
  char* path = kii_http->path;

  return ( // example)GET https://api.kii.com/v1/users HTTP1.0\r\n
    strlen(method) + 1
    + strlen(schema) + strlen(host) + strlen(path) + 1
    + strlen(http_version)
  );
}

void kii_state_request_line(kii_http* kii_http) {
  size_t len = request_line_len(kii_http);
  char request_line[len+1];
  char* host = kii_http->host;
  char* path = kii_http->path;

  request_line[0] = '\0';
  strcat(request_line, kii_http->method);
  strcat(request_line, " ");
  strcat(request_line, schema);
  strcat(request_line, host);
  strcat(request_line, path);
  strcat(request_line, " ");
  strcat(request_line, http_version);
  kii_socket_code_t send_res = kii_http->sc_send_cb(kii_http->socket_context, request_line, strlen(request_line));
  if (send_res == KII_SOCKETC_OK) {
    kii_http->state = REQUEST_HEADER;
    kii_http->current_request_header = kii_http->reaquest_headers;
    return;
  }
  if (send_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (send_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_SEND;
    return;
  } 
}

void kii_state_request_header(kii_http* kii_http) {
  if (kii_http->current_request_header == NULL) {
    kii_http->state = REQUEST_HEADER_END;
    return;
  }
  char* data = kii_http->current_request_header->data;
  if (data == NULL) {
    // Skip if data is NULL.
    kii_http->current_request_header = kii_http->current_request_header->next;
    return;
  }
  size_t len = strlen(data);
  if (len == 0) {
    // Skip if nothing to send.
    kii_http->current_request_header = kii_http->current_request_header->next;
    return;
  }
  kii_http->state = REQUEST_HEADER_SEND;
}

void kii_state_request_header_send(kii_http* kii_http) {
  char* line = kii_http->current_request_header->data;
  kii_socket_code_t send_res = kii_http->sc_send_cb(kii_http->socket_context, line, strlen(line));
  if (send_res == KII_SOCKETC_OK) {
    kii_http->state = REQUEST_HEADER_SEND_CRLF;
    return;
  }
  if (send_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (send_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_SEND;
    return;
  } 
}

void kii_state_request_header_send_crlf(kii_http* kii_http) {
  kii_socket_code_t send_res = kii_http->sc_send_cb(kii_http->socket_context, "\r\n", 2);
  if (send_res == KII_SOCKETC_OK) {
    kii_http->current_request_header = kii_http->current_request_header->next;
    kii_http->state = REQUEST_HEADER;
    return;
  }
  if (send_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (send_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_SEND;
    return;
  } 
}

void kii_state_request_header_end(kii_http* kii_http) {
  kii_socket_code_t send_res = kii_http->sc_send_cb(kii_http->socket_context, "\r\n", 2);
  if (send_res == KII_SOCKETC_OK) {
    kii_http->state = REQUEST_BODY_READ;
    kii_http->read_request_end = 0;
    return;
  }
  if (send_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (send_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_SEND;
    return;
  }
}

void kii_state_request_body_read(kii_http* kii_http) {
  kii_http->read_size = kii_http->read_callback(kii_http->read_buffer, 1, READ_BODY_SIZE, kii_http->read_data);
  // TODO: handle read failure. let return signed value?
  kii_http->state = REQUEST_BODY_SEND;
  if (kii_http->read_size == 0) {
    kii_http->read_request_end = 1;
  }
  return;
}

void kii_state_request_body_send(kii_http* kii_http) {
  kii_socket_code_t send_res = kii_http->sc_send_cb(kii_http->socket_context, kii_http->read_buffer, kii_http->read_size);
  if (send_res == KII_SOCKETC_OK) {
    if (kii_http->read_request_end == 1) {
      kii_http->state = RESPONSE_HEADERS_ALLOC;
    } else {
      kii_http->state = REQUEST_BODY_READ;
    }
    return;
  }
  if (send_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (send_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_SEND;
    return;
  }
}

void kii_state_response_headers_alloc(kii_http* kii_http) {
  kii_http->resp_header_buffer = malloc(READ_RESP_HEADER_SIZE);
  if (kii_http->resp_header_buffer == NULL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_ALLOCATION;
    return;
  }
  kii_http->resp_header_buffer_size = READ_RESP_HEADER_SIZE;
  kii_http->resp_header_buffer_current_pos = kii_http->resp_header_buffer;
  kii_http->state = RESPONSE_HEADERS_READ;
  return;
}

void kii_state_response_headers_realloc(kii_http* kii_http) {
  void* newBuff = realloc(kii_http->resp_header_buffer, kii_http->resp_header_buffer_size + READ_RESP_HEADER_SIZE);
  if (newBuff == NULL) {
    free(kii_http->resp_header_buffer);
    kii_http->resp_header_buffer = NULL;
    kii_http->resp_header_buffer_size = 0;
    kii_http->state = CLOSE;
    kii_http->result = KIIE_ALLOCATION;
    return;
  }
  // Pointer: last part newly allocated.
  kii_http->resp_header_buffer = newBuff;
  kii_http->resp_header_buffer_current_pos = newBuff + kii_http->resp_header_buffer_size;
  kii_http->resp_header_buffer_size = kii_http->resp_header_buffer_size + READ_RESP_HEADER_SIZE;
  kii_http->state = RESPONSE_HEADERS_READ;
  return;
}

void kii_state_response_headers_read(kii_http* kii_http) {
  size_t read_size = 0;
  kii_socket_code_t read_res = 
    kii_http->sc_recv_cb(kii_http->socket_context, kii_http->resp_header_buffer_current_pos, READ_RESP_HEADER_SIZE, &read_size);
  if (read_res == KII_SOCKETC_OK) {
    if (read_size == 0) {
      kii_http->read_end = 1;
    }
    // Search boundary for whole buffer.
    char* boundary = strnstr(kii_http->resp_header_buffer, "\r\n\r\n", kii_http->resp_header_buffer_size);
    if (boundary == NULL) {
      // Not reached to end of headers.
      kii_http->state = RESPONSE_HEADERS_REALLOC;
      return;
    } else {
      kii_http->body_boundary = boundary;
      kii_http->state = RESPONSE_HEADERS_CALLBACK;
      kii_http->cb_header_remaining_size = kii_http->resp_header_buffer_size;
      kii_http->cb_header_pos = kii_http->resp_header_buffer;
      return;
    }
  }
  if (read_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (read_res == KII_SOCKETC_FAIL) {
    char* start = kii_http->resp_header_buffer + READ_RESP_HEADER_SIZE - kii_http->resp_header_buffer_size;
    free(start);
    kii_http->resp_header_buffer = NULL;
    kii_http->resp_header_buffer_size = 0;
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_RECV;
    return;
  }
}

void kii_state_response_headers_callback(kii_http* kii_http) {
  char* header_boundary = strnstr(kii_http->cb_header_pos, "\r\n", kii_http->cb_header_remaining_size);
  size_t header_size = header_boundary - kii_http->cb_header_pos;
  size_t header_written = 
    kii_http->header_callback(kii_http->cb_header_pos, 1, header_size, kii_http->header_data);
  if (header_written != header_size) { // Error in callback function.
    kii_http->state = CLOSE;
    kii_http->result = KIIE_HEADER_CALLBACK;
    free(kii_http->resp_header_buffer);
    kii_http->resp_header_buffer = NULL;
    kii_http->resp_header_buffer_size = 0;
    return;
  }
  if (header_boundary < kii_http->body_boundary) {
    kii_http->cb_header_pos = header_boundary + 2; // +2 : Skip CRLF
    kii_http->cb_header_remaining_size = kii_http->cb_header_remaining_size - header_size - 2;
    return;
  } else { // Callback called for all headers.
    // Check if body is included in the buffer.
    size_t body_size = kii_http->cb_header_remaining_size - 4;
    if (body_size > 0) {
      kii_http->body_flagment = kii_http->body_boundary + 4;
      kii_http->body_flagment_size = body_size;
      kii_http->state = RESPONSE_BODY_FLAGMENT;
      return;
    } else {
      free(kii_http->resp_header_buffer);
      kii_http->resp_header_buffer = NULL;
      kii_http->resp_header_buffer_size = 0;
      if (kii_http->read_end == 1) {
        kii_http->state = CLOSE;
        return;
      } else {
        kii_http->state = RESPONSE_BODY_READ;
        return;
      }
    }
  }
}

void kii_state_response_body_flagment(kii_http* kii_http) {
  size_t written = 
    kii_http->write_callback(kii_http->body_flagment, 1, kii_http->body_flagment_size, kii_http->write_data);
  free(kii_http->resp_header_buffer);
  kii_http->resp_header_buffer = NULL;
  kii_http->resp_header_buffer_size = 0;
  if (written != kii_http->body_flagment_size) { // Error in write callback.
    kii_http->state = CLOSE;
    kii_http->result = KIIE_WRITE_CALLBACK;
    return;
  }
  if (kii_http->read_end == 1) {
    kii_http->state = CLOSE;
    return;
  } else {
    kii_http->state = RESPONSE_BODY_READ;
    return;
  }
}

void kii_state_response_body_read(kii_http* kii_http) {
  size_t read_size = 0;
  kii_socket_code_t read_res = 
    kii_http->sc_recv_cb(kii_http->socket_context, kii_http->body_buffer, READ_BODY_SIZE, &read_size);
  if (read_res == KII_SOCKETC_OK) {
    if (read_size < READ_BODY_SIZE) {
      kii_http->read_end = 1;
    }
    kii_http->body_read_size = read_size;
    kii_http->state = RESPONSE_BODY_CALLBACK;
    return;
  }
  if (read_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (read_res == KII_SOCKETC_FAIL) {
    kii_http->state = CLOSE;
    kii_http->result = KIIE_SC_RECV;
    return;
  }
}

void kii_state_response_body_callback(kii_http* kii_http) {
  size_t written = kii_http->write_callback(kii_http->body_buffer, 1, kii_http->body_read_size, kii_http->write_data);
  if (written < kii_http->body_read_size) { // Error in write callback.
    kii_http->state = CLOSE;
    kii_http->result = KIIE_WRITE_CALLBACK;
    return;
  }
  if (kii_http->read_end == 1) {
    kii_http->state = CLOSE;
  } else {
    kii_http->state = RESPONSE_BODY_READ;
  }
  return;
}

void kii_state_close(kii_http* kii_http) {
  kii_socket_code_t close_res = kii_http->sc_close_cb(kii_http->socket_context);
  if (close_res == KII_SOCKETC_OK) {
    kii_http->state = FINISHED;
    kii_http->result = KIIE_OK;
    return;
  }
  if (close_res == KII_SOCKETC_AGAIN) {
    return;
  }
  if (close_res == KII_SOCKETC_FAIL) {
    kii_http->state = FINISHED;
    kii_http->result = KIIE_SC_CLOSE;
    return;
  }
}

const KII_STATE_HANDLER state_handlers[] = {
  kii_state_idle,
  kii_state_connect,
  kii_state_request_line,
  kii_state_request_header,
  kii_state_request_header_send,
  kii_state_request_header_send_crlf,
  kii_state_request_header_end,
  kii_state_request_body_read,
  kii_state_request_body_send,
  kii_state_response_headers_alloc,
  kii_state_response_headers_realloc,
  kii_state_response_headers_callback,
  kii_state_response_body_flagment,
  kii_state_response_body_read,
  kii_state_response_body_callback,
  kii_state_close
};