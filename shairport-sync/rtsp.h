#ifndef _RTSP_H
#define _RTSP_H

#include "player.h"

rtsp_conn_info *playing_conn;
rtsp_conn_info **conns;

void rtsp_listen_loop(void);
// void rtsp_shutdown_stream(void);
void rtsp_request_shutdown_stream(void);

void cancel_all_RTSP_threads(void);

// initialise and completely delete the metadata stuff

void metadata_init(void);
void metadata_stop(void);

// sends metadata out to the metadata pipe, if enabled.
// It is sent with the type 'ssnc' the given code, data and length
// The handler at the other end must know what to do with the data
// e.g. if it's malloced, to free it, etc.
// nothing is done automatically

int send_ssnc_metadata(uint32_t code, char *data, uint32_t length, int block);

#endif // _RTSP_H
