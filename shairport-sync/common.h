#ifndef _COMMON_H
#define _COMMON_H

#include <libconfig.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio.h"
#include "config.h"
#include "definitions.h"
#include "mdns.h"

// struct sockaddr_in6 is bigger than struct sockaddr. derp
#ifdef AF_INET6
#define SOCKADDR struct sockaddr_storage
#define SAFAMILY ss_family
#else
#define SOCKADDR struct sockaddr
#define SAFAMILY sa_family
#endif

#if defined(CONFIG_DBUS_INTERFACE) || defined(CONFIG_MPRIS_INTERFACE)
enum dbus_session_type {
  DBT_system = 0, // use the session bus
  DBT_session,    // use the system bus
} dbt_type;
#endif

#define sps_extra_code_output_stalled 32768
#define sps_extra_code_output_state_cannot_make_ready 32769

// yeah/no/auto
enum yna_type {
  YNA_AUTO = -1,
  YNA_NO = 0,
  YNA_YES = 1
} yna_type;

// yeah/no/dont-care
enum yndk_type {
  YNDK_DONT_KNOW = -1,
  YNDK_NO = 0,
  YNDK_YES = 1
} yndk_type;

enum endian_type {
  SS_LITTLE_ENDIAN = 0,
  SS_PDP_ENDIAN,
  SS_BIG_ENDIAN,
} endian_type;

enum stuffing_type {
  ST_basic = 0, // straight deletion or insertion of a frame in a 352-frame packet
  ST_soxr,      // use libsoxr to make a 352 frame packet one frame longer or shorter
  ST_auto,			// use soxr if compiled for it and if the soxr_index is low enough
} s_type;

enum playback_mode_type {
  ST_stereo = 0,
  ST_mono,
  ST_reverse_stereo,
  ST_left_only,
  ST_right_only,
} playback_mode_type;

enum volume_control_profile_type {
  VCP_standard = 0,
  VCP_flat,
} volume_control_profile_type;

enum decoders_supported_type {
  decoder_hammerton = 0,
  decoder_apple_alac,
} decoders_supported_type;

enum disable_standby_mode_type {
  disable_standby_off = 0,
  disable_standby_auto,
  disable_standby_always
};

// the following enum is for the formats recognised -- currently only S16LE is recognised for input,
// so these are output only for the present

enum sps_format_t {
  SPS_FORMAT_UNKNOWN = 0,
  SPS_FORMAT_S8,
  SPS_FORMAT_U8,
  SPS_FORMAT_S16,
  SPS_FORMAT_S16_LE,
  SPS_FORMAT_S16_BE,
  SPS_FORMAT_S24,
  SPS_FORMAT_S24_LE,
  SPS_FORMAT_S24_BE,  
  SPS_FORMAT_S24_3LE,
  SPS_FORMAT_S24_3BE,
  SPS_FORMAT_S32,
  SPS_FORMAT_S32_LE,
  SPS_FORMAT_S32_BE,
  SPS_FORMAT_AUTO,
  SPS_FORMAT_INVALID,
} sps_format_t;

const char * sps_format_description_string(enum sps_format_t format);

typedef struct {
  config_t *cfg;
  int endianness;
  double airplay_volume; // stored here for reloading when necessary
  char *appName;         // normally the app is called shairport-syn, but it may be symlinked
  char *password;
  char *service_name; // the name for the shairport service, e.g. "Shairport Sync Version %v running
                      // on host %h"

#ifdef CONFIG_PA
  char *pa_application_name; // the name under which Shairport Sync shows up as an "Application" in
                             // the Sound Preferences in most desktop Linuxes.
  // Defaults to "Shairport Sync". Shairport Sync must be playing to see it.

  char *pa_sink; // the name (or id) of the sink that Shairport Sync will play on.
#endif
#ifdef CONFIG_METADATA
  int metadata_enabled;
  char *metadata_pipename;
  char *metadata_sockaddr;
  int metadata_sockport;
  size_t metadata_sockmsglength;
  int get_coverart;
#endif
#ifdef CONFIG_MQTT
  int mqtt_enabled;
  char *mqtt_hostname;
  int mqtt_port;
  char *mqtt_username;
  char *mqtt_password;
  char *mqtt_capath;
  char *mqtt_cafile;
  char *mqtt_certfile;
  char *mqtt_keyfile;
  char *mqtt_topic;
  int mqtt_publish_raw;
  int mqtt_publish_parsed;
  int mqtt_publish_cover;
  int mqtt_enable_remote;
#endif
  uint8_t hw_addr[6];
  int port;
  int udp_port_base;
  int udp_port_range;
  int ignore_volume_control;
  int volume_max_db_set; // set to 1 if a maximum volume db has been set
  int volume_max_db;
  int no_sync;            // disable synchronisation, even if it's available
  int no_mmap;            // disable use of mmap-based output, even if it's available
  double resyncthreshold; // if it get's out of whack my more than this number of seconds, resync.
                          // Zero means never
                          // resync.
  int allow_session_interruption;
  int timeout; // while in play mode, exit if no packets of audio come in for more than this number
               // of seconds . Zero means never exit.
  int dont_check_timeout; // this is used to maintain backward compatibility with the old -t option
                          // behaviour; only set by -t 0, cleared by everything else
  char *output_name;
  audio_output *output;
  char *mdns_name;
  mdns_backend *mdns;
  int buffer_start_fill;
  uint32_t userSuppliedLatency; // overrides all other latencies -- use with caution
  uint32_t fixedLatencyOffset;  // add this to all automatic latencies supplied to get the actual
                                // total latency
  // the total latency will be limited to the min and max-latency values, if supplied
#ifdef CONFIG_LIBDAEMON
  int daemonise;
  int daemonise_store_pid; // don't try to save a PID file
  char *piddir;
  char *computed_piddir; // the actual pid directory to create, if any
  char *pidfile;
#endif

  int logOutputLevel;              // log output level
  int debugger_show_elapsed_time;  // in the debug message, display the time since startup
  int debugger_show_relative_time; // in the debug message, display the time since the last one
  int statistics_requested, use_negotiated_latencies;
  enum playback_mode_type playback_mode;
  char *cmd_start, *cmd_stop, *cmd_set_volume, *cmd_unfixable;
  char *cmd_active_start, *cmd_active_stop;
  int cmd_blocking, cmd_start_returns_output;
  double tolerance; // allow this much drift before attempting to correct it
  enum stuffing_type packet_stuffing;
  int soxr_delay_index;
  int soxr_delay_threshold; // the soxr delay must be less or equal to this for soxr interpolation to be enabled under the auto setting
  int decoders_supported;
  int use_apple_decoder; // set to 1 if you want to use the apple decoder instead of the original by
                         // David Hammerton
  // char *logfile;
  // char *errfile;
  char *configfile;
  char *regtype; // The regtype is the service type followed by the protocol, separated by a dot, by
                 // default “_raop._tcp.”.
  char *interface;     // a string containg the interface name, or NULL if nothing specified
  int interface_index; // only valid if the interface string is non-NULL
  double audio_backend_buffer_desired_length; // this will be the length in seconds of the
                                              // audio backend buffer -- the DAC buffer for ALSA
  double audio_backend_buffer_interpolation_threshold_in_seconds; // below this, soxr interpolation
                                                                  // will not occur -- it'll be
                                                                  // basic interpolation instead.
  double audio_backend_silence_threshold; // below this, silence will be added to the output buffer
  double audio_backend_silence_scan_interval; // check the threshold this often

  double audio_backend_latency_offset; // this will be the offset in seconds to compensate for any
                                       // fixed latency there might be in the audio path
  double audio_backend_silent_lead_in_time; // the length of the silence that should precede a play.
  double active_state_timeout; // the amount of time from when play ends to when the system leaves
                              // into the "active" mode.
  uint32_t volume_range_db;   // the range, in dB, from max dB to min dB. Zero means use the mixer's
                              // native range.
  int volume_range_hw_priority; // when extending the volume range by combining sw and hw attenuators, lowering the volume, use all the hw attenuation before using
                                // sw attenuation
  enum volume_control_profile_type volume_control_profile;

	int output_format_auto_requested; // true if the configuration requests auto configuration
  enum sps_format_t output_format;
	int output_rate_auto_requested; // true if the configuration requests auto configuration
  unsigned int output_rate;

#ifdef CONFIG_CONVOLUTION
  int convolution;
  const char *convolution_ir_file;
  float convolution_gain;
  int convolution_max_length;
#endif

  int loudness;
  float loudness_reference_volume_db;
  int alsa_use_hardware_mute;
  double alsa_maximum_stall_time;
  enum disable_standby_mode_type disable_standby_mode;
  volatile int keep_dac_busy;
  enum yna_type use_precision_timing; // defaults to no

#if defined(CONFIG_DBUS_INTERFACE)
  enum dbus_session_type dbus_service_bus_type;
#endif
#if defined(CONFIG_MPRIS_INTERFACE)
  enum dbus_session_type mpris_service_bus_type;
#endif

#ifdef CONFIG_METADATA_HUB
  char *cover_art_cache_dir;
  int scan_interval_when_active;   // number of seconds between DACP server scans when playing
                                   // something (1)
  int scan_interval_when_inactive; // number of seconds between DACP server scans playing nothing
                                   // (3)
  int scan_max_bad_response_count; // number of successive bad results to ignore before giving up
                                   // (10)
  int scan_max_inactive_count;     // number of scans to do before stopping if not made active again
                                   // (about 15 minutes worth)
#endif
  int disable_resend_requests; // set this to stop resend request being made for missing packets
  double diagnostic_drop_packet_fraction; // pseudo randomly drop this fraction of packets, for
                                          // debugging. Currently audio packets only...
#ifdef CONFIG_JACK
  char *jack_client_name;
  char *jack_autoconnect_pattern;
#endif

} shairport_cfg;

uint32_t nctohl(const uint8_t *p); // read 4 characters from *p and do ntohl on them
uint16_t nctohs(const uint8_t *p); // read 2 characters from *p and do ntohs on them

void memory_barrier();

void log_to_stderr(); // call this to director logging to stderr;

// true if Shairport Sync is supposed to be sending output to the output device, false otherwise

int get_requested_connection_state_to_output();

void set_requested_connection_state_to_output(int v);

ssize_t non_blocking_write_with_timeout(int fd, const void *buf, size_t count, int timeout); // timeout in milliseconds

ssize_t non_blocking_write(int fd, const void *buf, size_t count); // used in a few places

/* from
 * http://coding.debuntu.org/c-implementing-str_replace-replace-all-occurrences-substring#comment-722
 */
char *str_replace(const char *string, const char *substr, const char *replacement);

// based on http://burtleburtle.net/bob/rand/smallprng.html

void r64init(uint64_t seed);
uint64_t r64u();
int64_t r64i();

uint64_t *ranarray;
void r64arrayinit();
uint64_t ranarray64u();
int64_t ranarray64i();

// if you are breaking in to a session, you need to avoid the ports of the current session
// if you are law-abiding, then you can reuse the ports.
// so, you can reset the free UDP ports minder when you're legit, and leave it otherwise

// the downside of using different ports each time is that it might make the firewall
// rules a bit more complex, as they need to allow more than the minimum three ports.
// a range of 10 is suggested anyway

void resetFreeUDPPort();
uint16_t nextFreeUDPPort();

volatile int debuglev;

void die(const char *format, ...);
void warn(const char *format, ...);
void inform(const char *format, ...);
void debug(int level, const char *format, ...);

uint8_t *base64_dec(char *input, int *outlen);
char *base64_enc(uint8_t *input, int length);

#define RSA_MODE_AUTH (0)
#define RSA_MODE_KEY (1)
uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode);

// given a volume (0 to -30) and high and low attenuations in dB*100 (e.g. 0 to -6000 for 0 to -60
// dB), return an attenuation depending on a linear interpolation along along the range
double flat_vol2attn(double vol, long max_db, long min_db);

// given a volume (0 to -30) and high and low attenuations in dB*100 (e.g. 0 to -6000 for 0 to -60
// dB), return an attenuation depending on the transfer function
double vol2attn(double vol, long max_db, long min_db);

// return a monolithic (always increasing) time in nanoseconds
uint64_t get_absolute_time_in_fp(void);

// time at startup for debugging timing
uint64_t fp_time_at_startup, fp_time_at_last_debug_message;

// this is for reading an unsigned 32 bit number, such as an RTP timestamp

uint32_t uatoi(const char *nptr);

// this is for allowing us to cancel the whole program
pthread_t main_thread_id;

shairport_cfg config;
config_t config_file_stuff;

int config_set_lookup_bool(config_t *cfg, char *where, int *dst);

void command_start(void);
void command_stop(void);
void command_execute(const char *command, const char *extra_argument, const int block);
void command_set_volume(double volume);

int mkpath(const char *path, mode_t mode);

void shairport_shutdown();

extern sigset_t pselect_sigset;

// wait for the specified time in microseconds -- it checks every 20 milliseconds
int sps_pthread_mutex_timedlock(pthread_mutex_t *mutex, useconds_t dally_time,
                                const char *debugmessage, int debuglevel);
// wait for the specified time, checking every 20 milliseconds, and block if it can't acquire the
// lock
int _debug_mutex_lock(pthread_mutex_t *mutex, useconds_t dally_time, const char *mutexName,
                      const char *filename, const int line, int debuglevel);

#define debug_mutex_lock(mu, t, d) _debug_mutex_lock(mu, t, #mu, __FILE__, __LINE__, d)

int _debug_mutex_unlock(pthread_mutex_t *mutex, const char *mutexName, const char *filename,
                        const int line, int debuglevel);

#define debug_mutex_unlock(mu, d) _debug_mutex_unlock(mu, #mu, __FILE__, __LINE__, d)

void pthread_cleanup_debug_mutex_unlock(void *arg);

#define pthread_cleanup_debug_mutex_lock(mu, t, d)                                                 \
  if (_debug_mutex_lock(mu, t, #mu, __FILE__, __LINE__, d) == 0)                                   \
  pthread_cleanup_push(pthread_cleanup_debug_mutex_unlock, (void *)mu)

char *get_version_string(); // mallocs a string space -- remember to free it afterwards

void sps_nanosleep(const time_t sec,
                   const long nanosec); // waits for this time, even through interruptions

int64_t generate_zero_frames(char *outp, size_t number_of_frames, enum sps_format_t format,
                             int with_dither, int64_t random_number_in);

void malloc_cleanup(void *arg);

#endif // _COMMON_H
