#ifndef _RTP_H
#define _RTP_H

#include <sys/socket.h>

#include "player.h"

void rtp_initialise(rtsp_conn_info *conn);
void rtp_terminate(rtsp_conn_info *conn);

void *rtp_audio_receiver(void *arg);
void *rtp_control_receiver(void *arg);
void *rtp_timing_receiver(void *arg);

void rtp_setup(SOCKADDR *local, SOCKADDR *remote, uint16_t controlport, uint16_t timingport,
               rtsp_conn_info *conn);
void rtp_request_resend(seq_t first, uint32_t count, rtsp_conn_info *conn);
void rtp_request_client_pause(rtsp_conn_info *conn); // ask the client to pause

void get_reference_timestamp_stuff(uint32_t *timestamp, uint64_t *timestamp_time,
                                   uint64_t *remote_timestamp_time, rtsp_conn_info *conn);
void clear_reference_timestamp(rtsp_conn_info *conn);

int have_timestamp_timing_information(rtsp_conn_info *conn);

int get_frame_play_time(int64_t timestamp, int sample_ratio, uint64_t *time_to_play);

int frame_to_local_time(uint32_t timestamp, uint64_t *time, rtsp_conn_info *conn);
int local_time_to_frame(uint64_t time, uint32_t *frame, rtsp_conn_info *conn);

int sanitised_source_rate_information(uint32_t *frames, uint64_t *time, rtsp_conn_info *conn);

#endif // _RTP_H
