#include "kii_http.h"

void kii_state_idle(kii_http* kii_http);
void kii_state_connect(kii_http* kii_http);
void kii_state_request_line(kii_http* kii_http);
void kii_state_request_header(kii_http* kii_http);
void kii_state_request_header_send(kii_http* kii_http);
void kii_state_request_header_end(kii_http* kii_http);
void kii_state_request_body_read(kii_http* kii_http);
void kii_state_request_body_send(kii_http* kii_http);
void kii_state_response_headers_alloc(kii_http* kii_http);
void kii_state_response_headers_realloc(kii_http* kii_http);
void kii_state_response_headers_callback(kii_http* kii_http);
void kii_state_response_body_flagment(kii_http* kii_http);