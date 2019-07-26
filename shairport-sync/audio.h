#ifndef _AUDIO_H
#define _AUDIO_H

#include <libconfig.h>
#include <stdint.h>

typedef struct {
  double current_volume_dB;
  int32_t minimum_volume_dB;
  int32_t maximum_volume_dB;
} audio_parameters;

typedef struct {
  void (*help)(void);
  char *name;

  // start of program
  int (*init)(int argc, char **argv);
  // at end of program
  void (*deinit)(void);
  
  int (*prepare)(void); // looks and sets stuff in the config data structure

  void (*start)(int sample_rate, int sample_format);

  // block of samples
  int (*play)(void *buf, int samples);
  void (*stop)(void);

  // may be null if no implemented
  int (*is_running)(
      void); // if implemented, will return 0 if everything is okay, non-zero otherwise

  // may be null if not implemented
  void (*flush)(void);

  // returns the delay before the next frame to be sent to the device would actually be audible.
  // almost certainly wrong if the buffer is empty, so put silent buffers into it to make it busy.
  // will change dynamically, so keep watching it. Implemented in ALSA only.
  // returns a negative error code if there's a problem
  int (*delay)(long *the_delay); // snd_pcm_sframes_t is a signed long
  int (*rate_info)(uint64_t *elapsed_time,
                   uint64_t *frames_played); // use this to get the true rate of the DAC

  // may be NULL, in which case soft volume is applied
  void (*volume)(double vol);

  // may be NULL, in which case soft volume parameters are used
  void (*parameters)(audio_parameters *info);

  // may be NULL, in which case software muting is used.
  // also, will return a 1 if it is actually using the mute facility, 0 otherwise
  int (*mute)(int do_mute);

} audio_output;

audio_output *audio_get_output(char *name);
void audio_ls_outputs(void);
void parse_general_audio_options(void);

#endif //_AUDIO_H
