// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/gestures.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_
#define GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_

// This class fixes up the timestamp of the hardware state.
// Before this filter is applied, there are two possibilities:
//   1) hwstate->timestamp == CLOCK_MONOTONIC &&
//      hwstate->msc_timestamp == 0.0
//        - No changes are needed in this case
//   2) hwstate->timestamp == CLOCK_MONOTONIC &&
//      hwstate->msc_timestamp == MSC_TIMESTAMP
//        - MSC_TIMESTAMP will be more accurate than CLOCK_MONOTONIC, so we want
//          to use it for time deltas in the gesture library.  However,
//          MSC_TIMESTAMP will reset to 0.0 if there are no touch events for at
//          least 1 second. So whenever MSC_TIMESTAMP resets, we record the
//          offset between CLOCK_MONOTONIC and MSC_TIMESTAMP and add this offset
//          to subsequent events.
// After this filter is applied:
//   - hwstate->timestamp uses CLOCK_MONOTONIC as the time base, possibly with
//     fine tuning provided by MSC_TIMESTAMP.
//   - hwstate->msc_timestamp should not be used.

namespace gestures {

class TimestampFilterInterpreter : public FilterInterpreter {
 public:
  // Takes ownership of |next|:
  explicit TimestampFilterInterpreter(Interpreter* next, Tracer* tracer);
  virtual ~TimestampFilterInterpreter() {}

 protected:
  virtual void SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

 private:
  stime_t prev_msc_timestamp_;

  // Difference between msc_timestamp and timestamp as of last timestamp reset.
  stime_t msc_timestamp_offset_;
};

}  // namespace gestures

#endif  // GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_
