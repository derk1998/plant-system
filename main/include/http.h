//
// Created by derk on 10/18/20.
//

#ifndef HTTP_H
#define HTTP_H

#define MAX_BUF_SIZE 1024

typedef void (*http_cb_t)(void* data);

typedef struct
{
    http_cb_t on_receive_credentials;
} http_callbacks_t;

void register_on_receive_credentials_cb(http_cb_t callback);

void start_webserver(void * arg);
void stop_webserver(void);

#endif //HTTP_H
