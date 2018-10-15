// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/gestures.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_
#define GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_

// This class fixes up the timestamp of the hardware state. There are three
// possibilities:
//   1) hwstate->timestamp is reliable.
//   2) hwstate->timestamp may be unreliable, but a reliable
//      hwstate->msc_timestamp has been provided.
//   3) hwstate->timestamp and hwstate->msc_timestamp are both unreliable.
// ComputeTimestampDefault handles the first two cases, and
// ComputeTimestampUsingFake handles the third case.

namespace gestures {

class TimestampFilterInterpreter : public FilterInterpreter {
  FRIEND_TEST(TimestampFilterInterpreterTest, FakeTimestampTest);
  FRIEND_TEST(TimestampFilterInterpreterTest, FakeTimestampJumpForwardTest);
  FRIEND_TEST(TimestampFilterInterpreterTest, FakeTimestampFallBackwardTest);
 public:
  // Takes ownership of |next|:
  explicit TimestampFilterInterpreter(PropRegistry* prop_reg,
                                      Interpreter* next,
                                      Tracer* tracer);
  virtual ~TimestampFilterInterpreter() {}

 protected:
  virtual void SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

 private:

  // Before this function is applied, there are two possibilities:
  //   1) hwstate->timestamp == CLOCK_MONOTONIC &&
  //      hwstate->msc_timestamp == 0.0
  //        - No changes are needed in this case
  //   2) hwstate->timestamp == CLOCK_MONOTONIC &&
  //      hwstate->msc_timestamp == MSC_TIMESTAMP
  //        - MSC_TIMESTAMP will be more accurate than CLOCK_MONOTONIC, so we
  //          want to use it for time deltas in the gesture library.  However,
  //          MSC_TIMESTAMP will reset to 0.0 if there are no touch events for
  //          at least 1 second. So whenever MSC_TIMESTAMP resets, we record the
  //          offset between CLOCK_MONOTONIC and MSC_TIMESTAMP and add this
  //          offset to subsequent events.
  // After this function is applied:
  //   - hwstate->timestamp uses CLOCK_MONOTONIC as the time base, possibly with
  //     fine tuning provided by MSC_TIMESTAMP.
  //   - hwstate->msc_timestamp should not be used.
  void ChangeTimestampDefault(HardwareState* hwstate);

  // If neither hwstate->timestamp nor hwstate->msc_timestamp has reliable
  // deltas, we use fake_timestamp_delta_ as the delta between consecutive
  // reports, but don't allow our faked timestamp to diverge too far from
  // hwstate->timestamp.
  void ChangeTimestampUsingFake(HardwareState* hwstate);

  stime_t prev_msc_timestamp_;

  // Difference between msc_timestamp and timestamp as of last timestamp reset.
  stime_t msc_timestamp_offset_;

  // If we are using fake timestamps, this holds the most recent fake
  stime_t fake_timestamp_;
  // If we don't have a reliable timestamp, we use this as the timestamp delta.
  DoubleProperty fake_timestamp_delta_;
  // Maximum we let fake_timestamp_ diverge from hwstate->timestamp
  stime_t fake_timestamp_max_divergence_;
};

}  // namespace gestures

#endif  // GESTURES_TIMESTAMP_FILTER_INTERPRETER_H_
