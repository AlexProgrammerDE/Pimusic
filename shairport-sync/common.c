/*
 * Utility routines. This file is part of Shairport.
 * Copyright (c) James Laird 2013
 * The volume to attenuation function vol2attn copyright (c) Mike Brady 2014
 * Further changes and additions (c) Mike Brady 2014 -- 2019
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "common.h"
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <poll.h>
#include <popt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef COMPILE_FOR_OSX
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef CONFIG_OPENSSL
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

#ifdef CONFIG_POLARSSL
#include "polarssl/ctr_drbg.h"
#include "polarssl/entropy.h"
#include <polarssl/base64.h>
#include <polarssl/md.h>
#include <polarssl/version.h>
#include <polarssl/x509.h>

#if POLARSSL_VERSION_NUMBER >= 0x01030000
#include "polarssl/compat-1.2.h"
#endif
#endif

#ifdef CONFIG_MBEDTLS
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/version.h>
#include <mbedtls/x509.h>
#endif

#ifdef CONFIG_LIBDAEMON
#include <libdaemon/dlog.h>
#else
#include <syslog.h>
#endif

#ifdef CONFIG_ALSA
void set_alsa_out_dev(char *);
#endif

const char * sps_format_description_string_array[] = {"unknown", "S8", "U8" ,"S16", "S16_LE", "S16_BE", "S24", "S24_LE", "S24_BE", "S24_3LE", "S24_3BE", "S32", "S32_LE", "S32_BE", "auto", "invalid" };

const char * sps_format_description_string(enum sps_format_t format) {
  if ((format >= SPS_FORMAT_UNKNOWN) && (format <= SPS_FORMAT_AUTO))
    return sps_format_description_string_array[format];
  else
    return sps_format_description_string_array[SPS_FORMAT_INVALID];
}

// true if Shairport Sync is supposed to be sending output to the output device, false otherwise

static volatile int requested_connection_state_to_output = 1;

// this stuff is to direct logging to syslog via libdaemon or directly
// alternatively you can direct it to stderr using a command line option

#ifdef CONFIG_LIBDAEMON
static void (*sps_log)(int prio, const char *t, ...) = daemon_log;
#else
static void (*sps_log)(int prio, const char *t, ...) = syslog;
#endif

void do_sps_log(__attribute__((unused)) int prio, const char *t, ...) {
  char s[1024];
  va_list args;
  va_start(args, t);
  vsnprintf(s, sizeof(s), t, args);
  va_end(args);
  fprintf(stderr,"%s\n",s);    
}

void log_to_stderr() {
  sps_log = do_sps_log;
}

shairport_cfg config;

volatile int debuglev = 0;

sigset_t pselect_sigset;

int usleep_uncancellable(useconds_t usec) {
  int response;
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  response = usleep(usec);
  pthread_setcancelstate(oldState, NULL);
  return response;
}

static uint16_t UDPPortIndex = 0;

void resetFreeUDPPort() {
  debug(3, "Resetting UDP Port Suggestion to %u", config.udp_port_base);
  UDPPortIndex = 0;
}

uint16_t nextFreeUDPPort() {
  if (UDPPortIndex == 0)
    UDPPortIndex = config.udp_port_base;
  else if (UDPPortIndex == (config.udp_port_base + config.udp_port_range - 1))
    UDPPortIndex = config.udp_port_base + 3; // avoid wrapping back to the first three, as they can
                                             // be assigned by resetFreeUDPPort without checking
  else
    UDPPortIndex++;
  return UDPPortIndex;
}

int get_requested_connection_state_to_output() { return requested_connection_state_to_output; }

void set_requested_connection_state_to_output(int v) { requested_connection_state_to_output = v; }

void die(const char *format, ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char s[1024];
  s[0] = 0;
  uint64_t time_now = get_absolute_time_in_fp();
  uint64_t time_since_start = time_now - fp_time_at_startup;
  uint64_t time_since_last_debug_message = time_now - fp_time_at_last_debug_message;
  fp_time_at_last_debug_message = time_now;
  uint64_t divisor = (uint64_t)1 << 32;
  double tss = 1.0 * time_since_start / divisor;
  double tsl = 1.0 * time_since_last_debug_message / divisor;
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(s), format, args);
  va_end(args);

  if ((debuglev) && (config.debugger_show_elapsed_time) && (config.debugger_show_relative_time))
    sps_log(LOG_ERR, "|% 20.9f|% 20.9f|*fatal error: %s", tss, tsl, s);
  else if ((debuglev) && (config.debugger_show_relative_time))
    sps_log(LOG_ERR, "% 20.9f|*fatal error: %s", tsl, s);
  else if ((debuglev) && (config.debugger_show_elapsed_time))
    sps_log(LOG_ERR, "% 20.9f|*fatal error: %s", tss, s);
  else
    sps_log(LOG_ERR, "fatal error: %s", s);  
  pthread_setcancelstate(oldState, NULL);
  abort(); // exit() doesn't always work, by heaven.
}

void warn(const char *format, ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char s[1024];
  s[0] = 0;
  uint64_t time_now = get_absolute_time_in_fp();
  uint64_t time_since_start = time_now - fp_time_at_startup;
  uint64_t time_since_last_debug_message = time_now - fp_time_at_last_debug_message;
  fp_time_at_last_debug_message = time_now;
  uint64_t divisor = (uint64_t)1 << 32;
  double tss = 1.0 * time_since_start / divisor;
  double tsl = 1.0 * time_since_last_debug_message / divisor;
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(s), format, args);
  va_end(args);
  if ((debuglev) && (config.debugger_show_elapsed_time) && (config.debugger_show_relative_time))
    sps_log(LOG_WARNING, "|% 20.9f|% 20.9f|*warning: %s", tss, tsl, s);
  else if ((debuglev) && (config.debugger_show_relative_time))
    sps_log(LOG_WARNING, "% 20.9f|*warning: %s", tsl, s);
  else if ((debuglev) && (config.debugger_show_elapsed_time))
    sps_log(LOG_WARNING, "% 20.9f|*warning: %s", tss, s);
  else
    sps_log(LOG_WARNING, "%s", s);
  pthread_setcancelstate(oldState, NULL);
}

void debug(int level, const char *format, ...) {
  if (level > debuglev)
    return;
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char s[1024];
  s[0] = 0;
  uint64_t time_now = get_absolute_time_in_fp();
  uint64_t time_since_start = time_now - fp_time_at_startup;
  uint64_t time_since_last_debug_message = time_now - fp_time_at_last_debug_message;
  fp_time_at_last_debug_message = time_now;
  uint64_t divisor = (uint64_t)1 << 32;
  double tss = 1.0 * time_since_start / divisor;
  double tsl = 1.0 * time_since_last_debug_message / divisor;
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(s), format, args);
  va_end(args);
  if ((config.debugger_show_elapsed_time) && (config.debugger_show_relative_time))
    sps_log(LOG_DEBUG, "|% 20.9f|% 20.9f|%s", tss, tsl, s);
  else if (config.debugger_show_relative_time)
    sps_log(LOG_DEBUG, "% 20.9f|%s", tsl, s);
  else if (config.debugger_show_elapsed_time)
    sps_log(LOG_DEBUG, "% 20.9f|%s", tss, s);
  else
    sps_log(LOG_DEBUG, "%s", s);
  pthread_setcancelstate(oldState, NULL);
}

void inform(const char *format, ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char s[1024];
  s[0] = 0;
  uint64_t time_now = get_absolute_time_in_fp();
  uint64_t time_since_start = time_now - fp_time_at_startup;
  uint64_t time_since_last_debug_message = time_now - fp_time_at_last_debug_message;
  fp_time_at_last_debug_message = time_now;
  uint64_t divisor = (uint64_t)1 << 32;
  double tss = 1.0 * time_since_start / divisor;
  double tsl = 1.0 * time_since_last_debug_message / divisor;
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(s), format, args);
  va_end(args);
  if ((debuglev) && (config.debugger_show_elapsed_time) && (config.debugger_show_relative_time))
    sps_log(LOG_INFO, "|% 20.9f|% 20.9f|%s", tss, tsl, s);
  else if ((debuglev) && (config.debugger_show_relative_time))
    sps_log(LOG_INFO, "% 20.9f|%s", tsl, s);
  else if ((debuglev) && (config.debugger_show_elapsed_time))
    sps_log(LOG_INFO, "% 20.9f|%s", tss, s);
  else
    sps_log(LOG_INFO, "%s", s);
  pthread_setcancelstate(oldState, NULL);
}

// The following two functions are adapted slightly and with thanks from Jonathan Leffler's sample
// code at
// https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux

int do_mkdir(const char *path, mode_t mode) {
  struct stat st;
  int status = 0;

  if (stat(path, &st) != 0) {
    /* Directory does not exist. EEXIST for race condition */
    if (mkdir(path, mode) != 0 && errno != EEXIST)
      status = -1;
  } else if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    status = -1;
  }

  return (status);
}

// mkpath - ensure all directories in path exist
// Algorithm takes the pessimistic view and works top-down to ensure
// each directory in path exists, rather than optimistically creating
// the last element and working backwards.

int mkpath(const char *path, mode_t mode) {
  char *pp;
  char *sp;
  int status;
  char *copypath = strdup(path);

  status = 0;
  pp = copypath;
  while (status == 0 && (sp = strchr(pp, '/')) != 0) {
    if (sp != pp) {
      /* Neither root nor double slash in path */
      *sp = '\0';
      status = do_mkdir(copypath, mode);
      *sp = '/';
    }
    pp = sp + 1;
  }
  if (status == 0)
    status = do_mkdir(path, mode);
  free(copypath);
  return (status);
}

#ifdef CONFIG_MBEDTLS
char *base64_enc(uint8_t *input, int length) {
  char *buf = NULL;
  size_t dlen = 0;
  int rc = mbedtls_base64_encode(NULL, 0, &dlen, input, length);
  if (rc && (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL))
    debug(1, "Error %d getting length of base64 encode.", rc);
  else {
    buf = (char *)malloc(dlen);
    rc = mbedtls_base64_encode((unsigned char *)buf, dlen, &dlen, input, length);
    if (rc != 0)
      debug(1, "Error %d encoding base64.", rc);
  }
  return buf;
}

uint8_t *base64_dec(char *input, int *outlen) {
  // slight problem here is that Apple cut the padding off their challenges. We must restore it
  // before passing it in to the decoder, it seems
  uint8_t *buf = NULL;
  size_t dlen = 0;
  int inbufsize = ((strlen(input) + 3) / 4) * 4; // this is the size of the input buffer we will
                                                 // send to the decoder, but we need space for 3
                                                 // extra "="s and a NULL
  char *inbuf = malloc(inbufsize + 4);
  if (inbuf == 0)
    debug(1, "Can't malloc memory  for inbuf in base64_decode.");
  else {
    strcpy(inbuf, input);
    strcat(inbuf, "===");
    // debug(1,"base64_dec called with string \"%s\", length %d, filled string: \"%s\", length %d.",
    //		input,strlen(input),inbuf,inbufsize);
    int rc = mbedtls_base64_decode(NULL, 0, &dlen, (unsigned char *)inbuf, inbufsize);
    if (rc && (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL))
      debug(1, "Error %d getting decode length, result is %d.", rc, dlen);
    else {
      // debug(1,"Decode size is %d.",dlen);
      buf = malloc(dlen);
      if (buf == 0)
        debug(1, "Can't allocate memory in base64_dec.");
      else {
        rc = mbedtls_base64_decode(buf, dlen, &dlen, (unsigned char *)inbuf, inbufsize);
        if (rc != 0)
          debug(1, "Error %d in base64_dec.", rc);
      }
    }
    free(inbuf);
  }
  *outlen = dlen;
  return buf;
}
#endif

#ifdef CONFIG_POLARSSL
char *base64_enc(uint8_t *input, int length) {
  char *buf = NULL;
  size_t dlen = 0;
  int rc = base64_encode(NULL, &dlen, input, length);
  if (rc && (rc != POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL))
    debug(1, "Error %d getting length of base64 encode.", rc);
  else {
    buf = (char *)malloc(dlen);
    rc = base64_encode((unsigned char *)buf, &dlen, input, length);
    if (rc != 0)
      debug(1, "Error %d encoding base64.", rc);
  }
  return buf;
}

uint8_t *base64_dec(char *input, int *outlen) {
  // slight problem here is that Apple cut the padding off their challenges. We must restore it
  // before passing it in to the decoder, it seems
  uint8_t *buf = NULL;
  size_t dlen = 0;
  int inbufsize = ((strlen(input) + 3) / 4) * 4; // this is the size of the input buffer we will
                                                 // send to the decoder, but we need space for 3
                                                 // extra "="s and a NULL
  char *inbuf = malloc(inbufsize + 4);
  if (inbuf == 0)
    debug(1, "Can't malloc memory  for inbuf in base64_decode.");
  else {
    strcpy(inbuf, input);
    strcat(inbuf, "===");
    // debug(1,"base64_dec called with string \"%s\", length %d, filled string: \"%s\", length
    // %d.",input,strlen(input),inbuf,inbufsize);
    int rc = base64_decode(buf, &dlen, (unsigned char *)inbuf, inbufsize);
    if (rc && (rc != POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL))
      debug(1, "Error %d getting decode length, result is %d.", rc, dlen);
    else {
      // debug(1,"Decode size is %d.",dlen);
      buf = malloc(dlen);
      if (buf == 0)
        debug(1, "Can't allocate memory in base64_dec.");
      else {
        rc = base64_decode(buf, &dlen, (unsigned char *)inbuf, inbufsize);
        if (rc != 0)
          debug(1, "Error %d in base64_dec.", rc);
      }
    }
    free(inbuf);
  }
  *outlen = dlen;
  return buf;
}
#endif

#ifdef CONFIG_OPENSSL
char *base64_enc(uint8_t *input, int length) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  BIO *bmem, *b64;
  BUF_MEM *bptr;
  b64 = BIO_new(BIO_f_base64());
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(b64, input, length);
  (void) BIO_flush(b64);
  BIO_get_mem_ptr(b64, &bptr);

  char *buf = (char *)malloc(bptr->length);
  if (buf == NULL)
    die("could not allocate memory for buf in base64_enc");
  if (bptr->length) {
    memcpy(buf, bptr->data, bptr->length - 1);
    buf[bptr->length - 1] = 0;
  }

  BIO_free_all(b64);

  pthread_setcancelstate(oldState, NULL);
  return buf;
}

uint8_t *base64_dec(char *input, int *outlen) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  BIO *bmem, *b64;
  int inlen = strlen(input);

  b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);

  // Apple cut the padding off their challenges; restore it
  BIO_write(bmem, input, inlen);
  while (inlen++ & 3)
    BIO_write(bmem, "=", 1);
  (void) BIO_flush(bmem);

  int bufsize = strlen(input) * 3 / 4 + 1;
  uint8_t *buf = malloc(bufsize);
  int nread;

  nread = BIO_read(b64, buf, bufsize);

  BIO_free_all(b64);

  *outlen = nread;
  pthread_setcancelstate(oldState, NULL);
  return buf;
}
#endif

static char super_secret_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt\n"
    "wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n"
    "wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf\n"
    "/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n"
    "UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n"
    "BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n"
    "LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5\n"
    "NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm\n"
    "lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n"
    "aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n"
    "a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM\n"
    "oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z\n"
    "oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+\n"
    "k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n"
    "AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA\n"
    "cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf\n"
    "54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n"
    "17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n"
    "1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n"
    "LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n"
    "2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=\n"
    "-----END RSA PRIVATE KEY-----\0";

#ifdef CONFIG_OPENSSL
uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  RSA *rsa = NULL;
  if (!rsa) {
    BIO *bmem = BIO_new_mem_buf(super_secret_key, -1);
    rsa = PEM_read_bio_RSAPrivateKey(bmem, NULL, NULL, NULL);
    BIO_free(bmem);
  }

  uint8_t *out = malloc(RSA_size(rsa));
  switch (mode) {
  case RSA_MODE_AUTH:
    *outlen = RSA_private_encrypt(inlen, input, out, rsa, RSA_PKCS1_PADDING);
    break;
  case RSA_MODE_KEY:
    *outlen = RSA_private_decrypt(inlen, input, out, rsa, RSA_PKCS1_OAEP_PADDING);
    break;
  default:
    die("bad rsa mode");
  }
  RSA_free(rsa);
  pthread_setcancelstate(oldState, NULL);
  return out;
}
#endif

#ifdef CONFIG_MBEDTLS
uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode) {
  mbedtls_pk_context pkctx;
  mbedtls_rsa_context *trsa;
  const char *pers = "rsa_encrypt";
  size_t olen = *outlen;
  int rc;

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;

  mbedtls_entropy_init(&entropy);

  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                        strlen(pers));

  mbedtls_pk_init(&pkctx);

  rc = mbedtls_pk_parse_key(&pkctx, (unsigned char *)super_secret_key, sizeof(super_secret_key),
                            NULL, 0);
  if (rc != 0)
    debug(1, "Error %d reading the private key.", rc);

  uint8_t *outbuf = NULL;
  trsa = mbedtls_pk_rsa(pkctx);

  switch (mode) {
  case RSA_MODE_AUTH:
    mbedtls_rsa_set_padding(trsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
    outbuf = malloc(trsa->len);
    rc = mbedtls_rsa_pkcs1_encrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE,
                                   inlen, input, outbuf);
    if (rc != 0)
      debug(1, "mbedtls_pk_encrypt error %d.", rc);
    *outlen = trsa->len;
    break;
  case RSA_MODE_KEY:
    mbedtls_rsa_set_padding(trsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);
    outbuf = malloc(trsa->len);
    rc = mbedtls_rsa_pkcs1_decrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE,
                                   &olen, input, outbuf, trsa->len);
    if (rc != 0)
      debug(1, "mbedtls_pk_decrypt error %d.", rc);
    *outlen = olen;
    break;
  default:
    die("bad rsa mode");
  }

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  mbedtls_pk_free(&pkctx);
  return outbuf;
}
#endif

#ifdef CONFIG_POLARSSL
uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode) {
  rsa_context trsa;
  const char *pers = "rsa_encrypt";
  int rc;

  entropy_context entropy;
  ctr_drbg_context ctr_drbg;
  entropy_init(&entropy);
  if ((rc = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, (const unsigned char *)pers,
                          strlen(pers))) != 0)
    debug(1, "ctr_drbg_init returned %d\n", rc);

  rsa_init(&trsa, RSA_PKCS_V21, POLARSSL_MD_SHA1); // padding and hash id get overwritten
  // BTW, this seems to reset a lot of parameters in the rsa_context
  rc = x509parse_key(&trsa, (unsigned char *)super_secret_key, strlen(super_secret_key), NULL, 0);
  if (rc != 0)
    debug(1, "Error %d reading the private key.");

  uint8_t *out = NULL;

  switch (mode) {
  case RSA_MODE_AUTH:
    trsa.padding = RSA_PKCS_V15;
    trsa.hash_id = POLARSSL_MD_NONE;
    debug(2, "rsa_apply encrypt");
    out = malloc(trsa.len);
    rc = rsa_pkcs1_encrypt(&trsa, ctr_drbg_random, &ctr_drbg, RSA_PRIVATE, inlen, input, out);
    if (rc != 0)
      debug(1, "rsa_pkcs1_encrypt error %d.", rc);
    *outlen = trsa.len;
    break;
  case RSA_MODE_KEY:
    debug(2, "rsa_apply decrypt");
    trsa.padding = RSA_PKCS_V21;
    trsa.hash_id = POLARSSL_MD_SHA1;
    out = malloc(trsa.len);
#if POLARSSL_VERSION_NUMBER >= 0x01020900
    rc = rsa_pkcs1_decrypt(&trsa, ctr_drbg_random, &ctr_drbg, RSA_PRIVATE, (size_t *)outlen, input,
                           out, trsa.len);
#else
    rc = rsa_pkcs1_decrypt(&trsa, RSA_PRIVATE, outlen, input, out, trsa.len);
#endif
    if (rc != 0)
      debug(1, "decrypt error %d.", rc);
    break;
  default:
    die("bad rsa mode");
  }
  rsa_free(&trsa);
  debug(2, "rsa_apply exit");
  return out;
}
#endif

int config_set_lookup_bool(config_t *cfg, char *where, int *dst) {
  const char *str = 0;
  if (config_lookup_string(cfg, where, &str)) {
    if (strcasecmp(str, "no") == 0) {
      (*dst) = 0;
      return 1;
    } else if (strcasecmp(str, "yes") == 0) {
      (*dst) = 1;
      return 1;
    } else {
      die("Invalid %s option choice \"%s\". It should be \"yes\" or \"no\"", where, str);
      return 0;
    }
  } else {
    return 0;
  }
}

void command_set_volume(double volume) {
  // this has a cancellation point if waiting is enabled
  if (config.cmd_set_volume) {
    /*Spawn a child to run the program.*/
    pid_t pid = fork();
    if (pid == 0) { /* child process */
      size_t command_buffer_size = strlen(config.cmd_set_volume) + 32;
      char *command_buffer = (char *)malloc(command_buffer_size);
      if (command_buffer == NULL) {
        inform("Couldn't allocate memory for set_volume argument string");
      } else {
        memset(command_buffer, 0, command_buffer_size);
        snprintf(command_buffer, command_buffer_size, "%s %f", config.cmd_set_volume, volume);
        // debug(1,"command_buffer is \"%s\".",command_buffer);
        int argC;
        char **argV;
        // debug(1,"set_volume command found.");
        if (poptParseArgvString(command_buffer, &argC, (const char ***)&argV) != 0) {
          // note that argV should be free()'d after use, but we expect this fork to exit
          // eventually.
          warn("Can't decipher on-set-volume command arguments \"%s\".", command_buffer);
          free(argV);
          free(command_buffer);
        } else {
          free(command_buffer);
          // debug(1,"Executing on-set-volume command %s with %d arguments.",argV[0],argC);
          execv(argV[0], argV);
          warn("Execution of on-set-volume command \"%s\" failed to start", config.cmd_set_volume);
          // debug(1, "Error executing on-set-volume command %s", config.cmd_set_volume);
          exit(EXIT_FAILURE); /* only if execv fails */
        }
      }

    } else {
      if (config.cmd_blocking) { /* pid!=0 means parent process and if blocking is true, wait for
                                    process to finish */
        pid_t rc = waitpid(pid, 0, 0); /* wait for child to exit */
        if (rc != pid) {
          warn("Execution of on-set-volume command returned an error.");
          debug(1, "on-set-volume command %s finished with error %d", config.cmd_set_volume, errno);
        }
      }
      // debug(1,"Continue after on-set-volume command");
    }
  }
}

void command_start(void) {
  // this has a cancellation point if waiting is enabled or a response is awaited
  if (config.cmd_start) {
    pid_t pid;
    int pipes[2];

    if (config.cmd_start_returns_output && pipe(pipes) != 0) {
      warn("Unable to allocate pipe for popen of start command.");
      debug(1, "pipe finished with error %d", errno);
      return;
    }
    /*Spawn a child to run the program.*/
    pid = fork();
    if (pid == 0) { /* child process */
      int argC;
      char **argV;

      if (config.cmd_start_returns_output) {
        close(pipes[0]);
        if (dup2(pipes[1], 1) < 0) {
          warn("Unable to reopen pipe as stdout for popen of start command");
          debug(1, "dup2 finished with error %d", errno);
          close(pipes[1]);
          return;
        }
      }

      // debug(1,"on-start command found.");
      if (poptParseArgvString(config.cmd_start, &argC, (const char ***)&argV) !=
          0) // note that argV should be free()'d after use, but we expect this fork to exit
             // eventually.
        debug(1, "Can't decipher on-start command arguments");
      else {
        // debug(1,"Executing on-start command %s with %d arguments.",argV[0],argC);
        execv(argV[0], argV);
        warn("Execution of on-start command failed to start");
        debug(1, "Error executing on-start command %s", config.cmd_start);
        exit(EXIT_FAILURE); /* only if execv fails */
      }
    } else {
      if (config.cmd_blocking || config.cmd_start_returns_output) { /* pid!=0 means parent process
                                    and if blocking is true, wait for
                                    process to finish */
        pid_t rc = waitpid(pid, 0, 0);                              /* wait for child to exit */
        if ((rc != pid) && (errno != ECHILD)) {
          // In this context, ECHILD means that the child process has already completed, I think!
          warn("Execution of on-start command returned an error.");
          debug(1, "on-start command %s finished with error %d", config.cmd_start, errno);
        }
        if (config.cmd_start_returns_output) {
          static char buffer[256];
          int len;
          close(pipes[1]);
          len = read(pipes[0], buffer, 255);
          close(pipes[0]);
          buffer[len] = '\0';
          if (buffer[len - 1] == '\n')
            buffer[len - 1] = '\0'; // strip trailing newlines
          debug(1, "received '%s' as the device to use from the on-start command", buffer);
#ifdef CONFIG_ALSA
          set_alsa_out_dev(buffer);
#endif
        }
      }
      // debug(1,"Continue after on-start command");
    }
  }
}
void command_execute(const char *command, const char *extra_argument, const int block) {
  // this has a cancellation point if waiting is enabled
  if (command) {
    char new_command_buffer[1024];
    char *full_command = (char *)command;
    if (extra_argument != NULL) {
      memset(new_command_buffer, 0, sizeof(new_command_buffer));
      snprintf(new_command_buffer, sizeof(new_command_buffer), "%s %s", command, extra_argument);
      full_command = new_command_buffer;
    }

    /*Spawn a child to run the program.*/
    pid_t pid = fork();
    if (pid == 0) { /* child process */
      int argC;
      char **argV;
      if (poptParseArgvString(full_command, &argC, (const char ***)&argV) !=
          0) // note that argV should be free()'d after use, but we expect this fork to exit
             // eventually.
        debug(1, "Can't decipher command arguments in \"%s\".", full_command);
      else {
        // debug(1,"Executing command %s",full_command);
        execv(argV[0], argV);
        warn("Execution of command \"%s\" failed to start", full_command);
        debug(1, "Error executing command \"%s\".", full_command);
        exit(EXIT_FAILURE); /* only if execv fails */
      }
    } else {
      if (block) { /* pid!=0 means parent process and if blocking is true, wait for
                                    process to finish */
        pid_t rc = waitpid(pid, 0, 0); /* wait for child to exit */
        if ((rc != pid) && (errno != ECHILD)) {
          // In this context, ECHILD means that the child process has already completed, I think!
          warn("Execution of command \"%s\" returned an error.", full_command);
          debug(1, "Command \"%s\" finished with error %d", full_command, errno);
        }
      }
      // debug(1,"Continue after on-unfixable command");
    }
  }
}

void command_stop(void) {
  // this has a cancellation point if waiting is enabled
  if (config.cmd_stop)
    command_execute(config.cmd_stop, "", config.cmd_blocking);
}

// this is for reading an unsigned 32 bit number, such as an RTP timestamp

uint32_t uatoi(const char *nptr) {
  uint64_t llint = atoll(nptr);
  uint32_t r = llint;
  return r;
}

double flat_vol2attn(double vol, long max_db, long min_db) {
  double vol_setting = min_db; // if all else fails, set this, for safety

  if ((vol <= 0.0) && (vol >= -30.0)) {
    vol_setting = ((max_db - min_db) * (30.0 + vol) / 30) + min_db;
    // debug(2, "Linear profile Volume Setting: %f in range %ld to %ld.", vol_setting, min_db,
    // max_db);
  } else if (vol != -144.0) {
    debug(1,
          "Linear volume request value %f is out of range: should be from 0.0 to -30.0 or -144.0.",
          vol);
  }
  return vol_setting;
}
// Given a volume (0 to -30) and high and low attenuations available in the mixer in dB, return an
// attenuation depending on the volume and the function's transfer function
// See http://tangentsoft.net/audio/atten.html for data on good attenuators.
// We want a smooth attenuation function, like, for example, the ALPS RK27 Potentiometer transfer
// functions referred to at the link above.

// Note that the max_db and min_db are given as dB*100

double vol2attn(double vol, long max_db, long min_db) {

  // We use a little coordinate geometry to build a transfer function from the volume passed in to
  // the device's dynamic range. (See the diagram in the documents folder.) The x axis is the
  // "volume in" which will be from -30 to 0. The y axis will be the "volume out" which will be from
  // the bottom of the range to the top. We build the transfer function from one or more lines. We
  // characterise each line with two numbers: the first is where on x the line starts when y=0 (x
  // can be from 0 to -30); the second is where on y the line stops when when x is -30. thus, if the
  // line was characterised as {0,-30}, it would be an identity transfer. Assuming, for example, a
  // dynamic range of lv=-60 to hv=0 Typically we'll use three lines -- a three order transfer
  // function First: {0,30} giving a gentle slope -- the 30 comes from half the dynamic range
  // Second: {-5,-30-(lv+30)/2} giving a faster slope from y=0 at x=-12 to y=-42.5 at x=-30
  // Third: {-17,lv} giving a fast slope from y=0 at x=-19 to y=-60 at x=-30

#define order 3

  double vol_setting = 0;

  if ((vol <= 0.0) && (vol >= -30.0)) {
    long range_db = max_db - min_db; // this will be a positive number
    // debug(1,"Volume min %ddB, max %ddB, range %ddB.",min_db,max_db,range_db);
    // double first_slope = -3000.0; // this is the slope of the attenuation at the high end -- 30dB
    // for the full rotation.
    double first_slope =
        -range_db /
        2; // this is the slope of the attenuation at the high end -- 30dB for the full rotation.
    if (-range_db > first_slope)
      first_slope = range_db;
    double lines[order][2] = {
        {0, first_slope}, {-5, first_slope - (range_db + first_slope) / 2}, {-17, -range_db}};
    int i;
    for (i = 0; i < order; i++) {
      if (vol <= lines[i][0]) {
        double tvol = lines[i][1] * (vol - lines[i][0]) / (-30 - lines[i][0]);
        // debug(1,"On line %d, end point of %f, input vol %f yields output vol
        // %f.",i,lines[i][1],vol,tvol);
        if (tvol < vol_setting)
          vol_setting = tvol;
      }
    }
    vol_setting += max_db;
  } else if (vol != -144.0) {
    debug(1, "Volume request value %f is out of range: should be from 0.0 to -30.0 or -144.0.",
          vol);
    vol_setting = min_db; // for safety, return the lowest setting...
  } else {
    vol_setting = min_db; // for safety, return the lowest setting...
  }
  // debug(1,"returning an attenuation of %f.",vol_setting);
  // debug(2, "Standard profile Volume Setting for Airplay vol %f: %f in range %ld to %ld.", vol,
  //      vol_setting, min_db, max_db);
  return vol_setting;
}

uint64_t get_absolute_time_in_fp() {
  uint64_t time_now_fp;
#ifdef COMPILE_FOR_LINUX_AND_FREEBSD_AND_CYGWIN_AND_OPENBSD
  struct timespec tn;
  // can't use CLOCK_MONOTONIC_RAW as it's not implemented in OpenWrt
  clock_gettime(CLOCK_MONOTONIC, &tn);
  uint64_t tnfpsec = tn.tv_sec;
  if (tnfpsec > 0x100000000)
    warn("clock_gettime seconds overflow!");
  uint64_t tnfpnsec = tn.tv_nsec;
  if (tnfpnsec > 0x100000000)
    warn("clock_gettime nanoseconds seconds overflow!");
  tnfpsec = tnfpsec << 32;
  tnfpnsec = tnfpnsec << 32;
  tnfpnsec = tnfpnsec / 1000000000;

  time_now_fp = tnfpsec + tnfpnsec; // types okay
#endif
#ifdef COMPILE_FOR_OSX
  uint64_t time_now_mach;
  uint64_t elapsedNano;
  static mach_timebase_info_data_t sTimebaseInfo = {0, 0};

  time_now_mach = mach_absolute_time();

  // If this is the first time we've run, get the timebase.
  // We can use denom == 0 to indicate that sTimebaseInfo is
  // uninitialised because it makes no sense to have a zero
  // denominator in a fraction.

  if (sTimebaseInfo.denom == 0) {
    debug(1, "Mac initialise timebase info.");
    (void)mach_timebase_info(&sTimebaseInfo);
  }

  // Do the maths. We hope that the multiplication doesn't
  // overflow; the price you pay for working in fixed point.

  // this gives us nanoseconds
  uint64_t time_now_ns = time_now_mach * sTimebaseInfo.numer / sTimebaseInfo.denom;

  // take the units and shift them to the upper half of the fp, and take the nanoseconds, shift them
  // to the upper half and then divide the result to 1000000000
  time_now_fp =
      ((time_now_ns / 1000000000) << 32) + (((time_now_ns % 1000000000) << 32) / 1000000000);

#endif
  return time_now_fp;
}

ssize_t non_blocking_write_with_timeout(int fd, const void *buf, size_t count, int timeout) {
  // timeout is in milliseconds
  void *ibuf = (void *)buf;
  size_t bytes_remaining = count;
  int rc = 1;
  struct pollfd ufds[1];
  while ((bytes_remaining > 0) && (rc > 0)) {
    // check that we can do some writing
    ufds[0].fd = fd;
    ufds[0].events = POLLOUT;
    rc = poll(ufds, 1, timeout);
    if (rc < 0) {
      // debug(1, "non-blocking write error waiting for pipe to become ready for writing...");
    } else if (rc == 0) {
      // warn("non-blocking write timeout waiting for pipe to become ready for writing");
      rc = -1;
      errno = -ETIMEDOUT;
    } else { // rc > 0, implying it might be ready
      ssize_t bytes_written = write(fd, ibuf, bytes_remaining);
      if (bytes_written == -1) {
        // debug(1,"Error %d in non_blocking_write: \"%s\".",errno,strerror(errno));
        rc = bytes_written; // to imitate the return from write()
      } else {
        ibuf += bytes_written;
        bytes_remaining -= bytes_written;
      }
    }
  }
  if (rc > 0)
    return count - bytes_remaining; // this is just to mimic a normal write/3.
  else
    return rc;
  //  return write(fd,buf,count);
}

ssize_t non_blocking_write(int fd, const void *buf, size_t count) {
  return non_blocking_write_with_timeout(fd,buf,count,5000); // default is 5 seconds.
}

/* from
 * http://coding.debuntu.org/c-implementing-str_replace-replace-all-occurrences-substring#comment-722
 */

char *str_replace(const char *string, const char *substr, const char *replacement) {
  char *tok = NULL;
  char *newstr = NULL;
  char *oldstr = NULL;
  char *head = NULL;

  /* if either substr or replacement is NULL, duplicate string a let caller handle it */
  if (substr == NULL || replacement == NULL)
    return strdup(string);
  newstr = strdup(string);
  head = newstr;
  if (head) {
    while ((tok = strstr(head, substr))) {
      oldstr = newstr;
      newstr = malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);
      /*failed to alloc mem, free old string and return NULL */
      if (newstr == NULL) {
        free(oldstr);
        return NULL;
      }
      memcpy(newstr, oldstr, tok - oldstr);
      memcpy(newstr + (tok - oldstr), replacement, strlen(replacement));
      memcpy(newstr + (tok - oldstr) + strlen(replacement), tok + strlen(substr),
             strlen(oldstr) - strlen(substr) - (tok - oldstr));
      memset(newstr + strlen(oldstr) - strlen(substr) + strlen(replacement), 0, 1);
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen(replacement);
      free(oldstr);
    }
  } else {
    die("failed to allocate memory in str_replace.");
  }
  return newstr;
}

/* from http://burtleburtle.net/bob/rand/smallprng.html */

// typedef uint64_t u8;
typedef struct ranctx {
  uint64_t a;
  uint64_t b;
  uint64_t c;
  uint64_t d;
} ranctx;

static struct ranctx rx;

#define rot(x, k) (((x) << (k)) | ((x) >> (64 - (k))))
uint64_t ranval(ranctx *x) {
  uint64_t e = x->a - rot(x->b, 7);
  x->a = x->b ^ rot(x->c, 13);
  x->b = x->c + rot(x->d, 37);
  x->c = x->d + e;
  x->d = e + x->a;
  return x->d;
}

void raninit(ranctx *x, uint64_t seed) {
  uint64_t i;
  x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
  for (i = 0; i < 20; ++i) {
    (void)ranval(x);
  }
}

void r64init(uint64_t seed) { raninit(&rx, seed); }

uint64_t r64u() { return (ranval(&rx)); }

int64_t r64i() { return (ranval(&rx) >> 1); }

/* generate an array of 64-bit random numbers */
const int ranarraylength = 1009 * 203; // these will be 8-byte numbers.

int ranarraynext;

void ranarrayinit() {
  ranarray = (uint64_t *)malloc(ranarraylength * sizeof(uint64_t));
  if (ranarray) {
    int i;
    for (i = 0; i < ranarraylength; i++)
      ranarray[i] = r64u();
    ranarraynext = 0;
  } else {
    die("failed to allocate space for the ranarray.");
  }
}

uint64_t ranarrayval() {
  uint64_t v = ranarray[ranarraynext];
  ranarraynext++;
  ranarraynext = ranarraynext % ranarraylength;
  return v;
}

void r64arrayinit() { ranarrayinit(); }

// uint64_t ranarray64u() { return (ranarrayval()); }
uint64_t ranarray64u() { return (ranval(&rx)); }

// int64_t ranarray64i() { return (ranarrayval() >> 1); }
int64_t ranarray64i() { return (ranval(&rx) >> 1); }

uint32_t nctohl(const uint8_t *p) { // read 4 characters from *p and do ntohl on them
  // this is to avoid possible aliasing violations
  uint32_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohl(holder);
}

uint16_t nctohs(const uint8_t *p) { // read 2 characters from *p and do ntohs on them
  // this is to avoid possible aliasing violations
  uint16_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohs(holder);
}

pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;

void memory_barrier() {
  pthread_mutex_lock(&barrier_mutex);
  pthread_mutex_unlock(&barrier_mutex);
}

void sps_nanosleep(const time_t sec, const long nanosec) {
  struct timespec req, rem;
  int result;
  req.tv_sec = sec;
  req.tv_nsec = nanosec;
  do {
    result = nanosleep(&req, &rem);
    rem = req;
  } while ((result == -1) && (errno == EINTR));
  if (result == -1)
    debug(1, "Error in sps_nanosleep of %d sec and %ld nanoseconds: %d.", sec, nanosec, errno);
}

// Mac OS X doesn't have pthread_mutex_timedlock
// Also note that timing must be relative to CLOCK_REALTIME

#ifdef COMPILE_FOR_LINUX_AND_FREEBSD_AND_CYGWIN_AND_OPENBSD
int sps_pthread_mutex_timedlock(pthread_mutex_t *mutex, useconds_t dally_time,
                                const char *debugmessage, int debuglevel) {

  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  struct timespec tn;
  clock_gettime(CLOCK_REALTIME, &tn);
  uint64_t tnfpsec = tn.tv_sec;
  if (tnfpsec > 0x100000000)
    warn("clock_gettime seconds overflow!");
  uint64_t tnfpnsec = tn.tv_nsec;
  if (tnfpnsec > 0x100000000)
    warn("clock_gettime nanoseconds seconds overflow!");
  tnfpsec = tnfpsec << 32;
  tnfpnsec = tnfpnsec << 32;
  tnfpnsec = tnfpnsec / 1000000000;

  uint64_t time_now_in_fp = tnfpsec + tnfpnsec; // types okay

  uint64_t dally_time_in_fp = dally_time;                // microseconds
  dally_time_in_fp = (dally_time_in_fp << 32) / 1000000; // convert to fp format
  uint64_t time_then = time_now_in_fp + dally_time_in_fp;

  uint64_t time_then_nsec = time_then & 0xffffffff; // remove integral part
  time_then_nsec = time_then_nsec * 1000000000;     // multiply fractional part to nanoseconds

  struct timespec timeoutTime;

  time_then = time_then >> 32;           // get the seconds
  time_then_nsec = time_then_nsec >> 32; // and the nanoseconds

  timeoutTime.tv_sec = time_then;
  timeoutTime.tv_nsec = time_then_nsec;
  int64_t start_time = get_absolute_time_in_fp();
  int r = pthread_mutex_timedlock(mutex, &timeoutTime);
  int64_t et = get_absolute_time_in_fp() - start_time;

  if ((debuglevel != 0) && (r != 0) && (debugmessage != NULL)) {
    et = (et * 1000000) >> 32; // microseconds
    char errstr[1000];
    if (r == ETIMEDOUT)
      debug(debuglevel,
            "timed out waiting for a mutex, having waiting %f seconds, with a maximum "
            "waiting time of %d microseconds. \"%s\".",
            (1.0 * et) / 1000000, dally_time, debugmessage);
    else
      debug(debuglevel, "error %d: \"%s\" waiting for a mutex: \"%s\".", r,
            strerror_r(r, errstr, sizeof(errstr)), debugmessage);
  }
  pthread_setcancelstate(oldState, NULL);
  return r;
}
#endif
#ifdef COMPILE_FOR_OSX
int sps_pthread_mutex_timedlock(pthread_mutex_t *mutex, useconds_t dally_time,
                                const char *debugmessage, int debuglevel) {

  // this is not pthread_cancellation safe because is contains a cancellation point
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  int time_to_wait = dally_time;
  int r = pthread_mutex_trylock(mutex);
  while ((r == EBUSY) && (time_to_wait > 0)) {
    int st = time_to_wait;
    if (st > 1000)
      st = 1000;
    sps_nanosleep(0, st * 1000); // this contains a cancellation point
    time_to_wait -= st;
    r = pthread_mutex_trylock(mutex);
  }
  if ((debuglevel != 0) && (r != 0) && (debugmessage != NULL)) {
    char errstr[1000];
    if (r == EBUSY) {
      debug(debuglevel,
            "waiting for a mutex, maximum expected time of %d microseconds exceeded \"%s\".",
            dally_time, debugmessage);
      r = ETIMEDOUT; // for compatibility
    } else {
      debug(debuglevel, "error %d: \"%s\" waiting for a mutex: \"%s\".", r,
            strerror_r(r, errstr, sizeof(errstr)), debugmessage);
    }
  }
  pthread_setcancelstate(oldState, NULL);
  return r;
}
#endif

int _debug_mutex_lock(pthread_mutex_t *mutex, useconds_t dally_time, const char *mutexname,
                      const char *filename, const int line, int debuglevel) {
  if ((debuglevel > debuglev) || (debuglevel == 0))
    return pthread_mutex_lock(mutex);
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  uint64_t time_at_start = get_absolute_time_in_fp();
  char dstring[1000];
  memset(dstring, 0, sizeof(dstring));
  snprintf(dstring, sizeof(dstring), "%s:%d", filename, line);
  if (debuglevel != 0)
    debug(3, "mutex_lock \"%s\" at \"%s\".", mutexname, dstring); // only if you really ask for it!
  int result = sps_pthread_mutex_timedlock(mutex, dally_time, dstring, debuglevel);
  if (result == ETIMEDOUT) {
    result = pthread_mutex_lock(mutex);
    uint64_t time_delay = get_absolute_time_in_fp() - time_at_start;
    uint64_t divisor = (uint64_t)1 << 32;
    double delay = 1.0 * time_delay / divisor;
    debug(debuglevel,
          "mutex_lock \"%s\" at \"%s\" expected max wait: %0.9f, actual wait: %0.9f sec.",
          mutexname, dstring, (1.0 * dally_time) / 1000000, delay);
  }
  pthread_setcancelstate(oldState, NULL);
  return result;
}

int _debug_mutex_unlock(pthread_mutex_t *mutex, const char *mutexname, const char *filename,
                        const int line, int debuglevel) {
  if ((debuglevel > debuglev) || (debuglevel == 0))
    return pthread_mutex_unlock(mutex);
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char dstring[1000];
  char errstr[512];
  memset(dstring, 0, sizeof(dstring));
  snprintf(dstring, sizeof(dstring), "%s:%d", filename, line);
  debug(debuglevel, "mutex_unlock \"%s\" at \"%s\".", mutexname, dstring);
  int r = pthread_mutex_unlock(mutex);
  if ((debuglevel != 0) && (r != 0))
    debug(1, "error %d: \"%s\" unlocking mutex \"%s\" at \"%s\".", r,
          strerror_r(r, errstr, sizeof(errstr)), mutexname, dstring);
  pthread_setcancelstate(oldState, NULL);
  return r;
}

void malloc_cleanup(void *arg) {
  // debug(1, "malloc cleanup called.");
  free(arg);
}

void pthread_cleanup_debug_mutex_unlock(void *arg) { pthread_mutex_unlock((pthread_mutex_t *)arg); }

char *get_version_string() {
  char *version_string = malloc(1024);
  if (version_string) {
    strcpy(version_string, PACKAGE_VERSION);

#ifdef CONFIG_LIBDAEMON
  strcat(version_string, "-libdaemon");
#endif
#ifdef CONFIG_MBEDTLS
  strcat(version_string, "-mbedTLS");
#endif
#ifdef CONFIG_POLARSSL
    strcat(version_string, "-PolarSSL");
#endif
#ifdef CONFIG_OPENSSL
    strcat(version_string, "-OpenSSL");
#endif
#ifdef CONFIG_TINYSVCMDNS
    strcat(version_string, "-tinysvcmdns");
#endif
#ifdef CONFIG_AVAHI
    strcat(version_string, "-Avahi");
#endif
#ifdef CONFIG_DNS_SD
    strcat(version_string, "-dns_sd");
#endif
#ifdef CONFIG_EXTERNAL_MDNS
    strcat(version_string, "-external_mdns");
#endif
#ifdef CONFIG_ALSA
    strcat(version_string, "-ALSA");
#endif
#ifdef CONFIG_SNDIO
    strcat(version_string, "-sndio");
#endif
#ifdef CONFIG_AO
    strcat(version_string, "-ao");
#endif
#ifdef CONFIG_PA
    strcat(version_string, "-pa");
#endif
#ifdef CONFIG_SOUNDIO
    strcat(version_string, "-soundio");
#endif
#ifdef CONFIG_DUMMY
    strcat(version_string, "-dummy");
#endif
#ifdef CONFIG_STDOUT
    strcat(version_string, "-stdout");
#endif
#ifdef CONFIG_PIPE
    strcat(version_string, "-pipe");
#endif
#ifdef CONFIG_SOXR
    strcat(version_string, "-soxr");
#endif
#ifdef CONFIG_CONVOLUTION
    strcat(version_string, "-convolution");
#endif
#ifdef CONFIG_METADATA
    strcat(version_string, "-metadata");
#endif
#ifdef CONFIG_MQTT
    strcat(version_string, "-mqtt");
#endif
#ifdef CONFIG_DBUS_INTERFACE
    strcat(version_string, "-dbus");
#endif
#ifdef CONFIG_MPRIS_INTERFACE
    strcat(version_string, "-mpris");
#endif
    strcat(version_string, "-sysconfdir:");
    strcat(version_string, SYSCONFDIR);
  }
  return version_string;
}

int64_t generate_zero_frames(char *outp, size_t number_of_frames, enum sps_format_t format,
                             int with_dither, int64_t random_number_in) {
  // return the last random number used
  // assuming the buffer has been assigned

  int64_t previous_random_number = random_number_in;
  char *p = outp;
  size_t sample_number;
  for (sample_number = 0; sample_number < number_of_frames * 2; sample_number++) {

    int64_t hyper_sample = 0;
    // add a TPDF dither -- see
    // http://educypedia.karadimov.info/library/DitherExplained.pdf
    // and the discussion around https://www.hydrogenaud.io/forums/index.php?showtopic=16963&st=25

    // I think, for a 32 --> 16 bits, the range of
    // random numbers needs to be from -2^16 to 2^16, i.e. from -65536 to 65536 inclusive, not from
    // -32768 to +32767
    
    // Actually, what would be generated here is from -65535 to 65535, i.e. one less on the limits.

    // See the original paper at
    // http://www.ece.rochester.edu/courses/ECE472/resources/Papers/Lipshitz_1992.pdf
    // by Lipshitz, Wannamaker and Vanderkooy, 1992.

    int64_t dither_mask = 0;
    switch (format) {
    case SPS_FORMAT_S32:
    case SPS_FORMAT_S32_LE:
    case SPS_FORMAT_S32_BE:
      dither_mask = (int64_t)1 << (64 - 32);
      break;
    case SPS_FORMAT_S24:
    case SPS_FORMAT_S24_LE:
    case SPS_FORMAT_S24_BE:
    case SPS_FORMAT_S24_3LE:
    case SPS_FORMAT_S24_3BE:
      dither_mask = (int64_t)1 << (64 - 24);
      break;
    case SPS_FORMAT_S16:
    case SPS_FORMAT_S16_LE:
    case SPS_FORMAT_S16_BE:
      dither_mask = (int64_t)1 << (64 - 16);
      break;
    case SPS_FORMAT_S8:
    case SPS_FORMAT_U8:
      dither_mask = (int64_t)1 << (64 - 8);
      break;
    case SPS_FORMAT_UNKNOWN:
      die("Unexpected SPS_FORMAT_UNKNOWN while calculating dither mask.");
      break;
    case SPS_FORMAT_AUTO:
      die("Unexpected SPS_FORMAT_AUTO while calculating dither mask.");
      break;
    case SPS_FORMAT_INVALID:
      die("Unexpected SPS_FORMAT_INVALID while calculating dither mask.");
      break;
    }
    dither_mask -= 1;
    // int64_t r = r64i();
    int64_t r = ranarray64i();

    int64_t tpdf = (r & dither_mask) - (previous_random_number & dither_mask);

    // add dither if permitted -- no need to check for clipping, as the sample is, uh, zero

    if (with_dither != 0)
      hyper_sample += tpdf;

    // move the result to the desired position in the int64_t
    char *op = p;
    int result; // this is the length of the sample

    uint8_t byt;
    switch (format) {
    case SPS_FORMAT_S32:
      hyper_sample >>= (64 - 32);
      *(int32_t *)op = hyper_sample;
      result = 4;
      break;
    case SPS_FORMAT_S32_LE:
      hyper_sample >>= (64 - 32);
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 24);
      *op++ = byt;
      result = 4;
      break;
    case SPS_FORMAT_S32_BE:
      hyper_sample >>= (64 - 32);
      byt = (uint8_t)(hyper_sample >> 24);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      result = 4;
      break;
    case SPS_FORMAT_S24_3LE:
      hyper_sample >>= (64 - 24);
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      result = 3;
      break;
    case SPS_FORMAT_S24_3BE:
      hyper_sample >>= (64 - 24);
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      result = 3;
      break;
    case SPS_FORMAT_S24:
      hyper_sample >>= (64 - 24);
      *(int32_t *)op = hyper_sample;
      result = 4;
      break;
    case SPS_FORMAT_S24_LE:
      hyper_sample >>= (64 - 24);
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      *op++ = 0;
      result = 4;
      break;
    case SPS_FORMAT_S24_BE:
      hyper_sample >>= (64 - 24);
      *op++ = 0;
      byt = (uint8_t)(hyper_sample >> 16);
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      result = 4;
      break;
    case SPS_FORMAT_S16_LE:
      hyper_sample >>= (64 - 16);
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      result = 2;
      break;
    case SPS_FORMAT_S16_BE:
      hyper_sample >>= (64 - 16);
      byt = (uint8_t)(hyper_sample >> 8);
      *op++ = byt;
      byt = (uint8_t)hyper_sample;
      *op++ = byt;
      result = 2;
      break;
    case SPS_FORMAT_S16:
      hyper_sample >>= (64 - 16);
      *(int16_t *)op = (int16_t)hyper_sample;
      result = 2;
      break;
    case SPS_FORMAT_S8:
      hyper_sample >>= (int8_t)(64 - 8);
      *op = hyper_sample;
      result = 1;
      break;
    case SPS_FORMAT_U8:
      hyper_sample >>= (uint8_t)(64 - 8);
      hyper_sample += 128;
      *op = hyper_sample;
      result = 1;
      break;
    default:
      result = 0; // stop a compiler warning
      die("Unexpected SPS_FORMAT_* with index %d while outputting silence",format);
    }
    p += result;
    previous_random_number = r;
  }
  // hack
  // memset(outp,0,number_of_frames * 4);
  return previous_random_number;
}
