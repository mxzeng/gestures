// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/timestamp_filter_interpreter.h"

#include "gestures/include/logging.h"
#include "gestures/include/tracer.h"

namespace gestures {

TimestampFilterInterpreter::TimestampFilterInterpreter(
    Interpreter* next, Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      prev_msc_timestamp_(-1.0),
      msc_timestamp_offset_(-1.0) {
  InitName();
}

void TimestampFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {

  // Check if this is the first event or there has been a jump backwards.
  if (prev_msc_timestamp_ < 0.0 ||
      hwstate->msc_timestamp == 0.0 ||
      hwstate->msc_timestamp < prev_msc_timestamp_) {
    msc_timestamp_offset_ = hwstate->timestamp - hwstate->msc_timestamp;
  }
  prev_msc_timestamp_ = hwstate->msc_timestamp;
  hwstate->timestamp = hwstate->msc_timestamp + msc_timestamp_offset_;
  hwstate->msc_timestamp = 0.0;
  next_->SyncInterpret(hwstate, timeout);
}

}  // namespace gestures
