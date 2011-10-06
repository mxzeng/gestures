// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_LOGGING_H__
#define GESTURES_LOGGING_H__

// TODO(adlr): Let clients specify style of logging rather than using #defines

#ifndef XLOGGING

#include <stdio.h>

#define Log(format, ...) \
  fprintf(stderr, "INFO:%s:%d:" format "\n", __FILE__, __LINE__, ## __VA_ARGS__)
#define Err(format, ...) \
  fprintf(stderr, "ERROR:%s:%d:" format "\n", \
          __FILE__, __LINE__, ## __VA_ARGS__)
#else

extern "C" {

typedef enum {
  X_PROBED,
  X_CONFIG,
  X_DEFAULT,
  X_CMDLINE,
  X_NOTICE,
  X_ERROR,
  X_WARNING,
  X_INFO,
  X_NONE,
  X_NOT_IMPLEMENTED,
  X_UNKNOWN = -1
} MessageType;

extern void xf86MsgVerb(MessageType type, int verb, const char *format, ...);

}  // extern "C"

#define Log(format, ...)                                \
    xf86MsgVerb(X_INFO, 7, "%s:%d:" format "\n",        \
                __FILE__, __LINE__, ## __VA_ARGS__)
#define Err(format, ...)                                \
    xf86MsgVerb(X_ERROR, 0, "%s:%d:" format "\n",       \
                __FILE__, __LINE__, ## __VA_ARGS__)
#endif

#endif  // GESTURES_LOGGING_H__
