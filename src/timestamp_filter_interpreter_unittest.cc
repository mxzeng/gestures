// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/timestamp_filter_interpreter.h"
#include "gestures/include/unittest_util.h"

namespace gestures {

class TimestampFilterInterpreterTest : public ::testing::Test {};

class TimestampFilterInterpreterTestInterpreter : public Interpreter {
 public:
  TimestampFilterInterpreterTestInterpreter()
      : Interpreter(NULL, NULL, false) {}
};

TEST(TimestampFilterInterpreterTest, SimpleTest) {
  TimestampFilterInterpreterTestInterpreter* base_interpreter =
      new TimestampFilterInterpreterTestInterpreter;
  TimestampFilterInterpreter interpreter(base_interpreter, NULL);
  TestInterpreterWrapper wrapper(&interpreter);

  HardwareState hs[] = {
    { 1.000, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },
    { 1.010, 0, 1, 1, NULL, 0, 0, 0, 0, 0.012 },
    { 1.020, 0, 1, 1, NULL, 0, 0, 0, 0, 0.018 },
    { 1.030, 0, 1, 1, NULL, 0, 0, 0, 0, 0.031 }
  };

  stime_t expected_timestamps[] = { 1.000, 1.012, 1.018, 1.031 };

  for (size_t i = 0; i < arraysize(hs); i++) {
    wrapper.SyncInterpret(&hs[i], NULL);
    EXPECT_EQ(hs[i].timestamp, expected_timestamps[i]);
  }
}

TEST(TimestampFilterInterpreterTest, NoMscTimestampTest) {
  TimestampFilterInterpreterTestInterpreter* base_interpreter =
      new TimestampFilterInterpreterTestInterpreter;
  TimestampFilterInterpreter interpreter(base_interpreter, NULL);
  TestInterpreterWrapper wrapper(&interpreter);

  HardwareState hs[] = {
    { 1.000, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },
    { 1.010, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },
    { 1.020, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },
    { 1.030, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 }
  };

  for (size_t i = 0; i < arraysize(hs); i++) {
    stime_t expected_timestamp = hs[i].timestamp;
    wrapper.SyncInterpret(&hs[i], NULL);
    EXPECT_EQ(hs[i].timestamp, expected_timestamp);
  }
}

TEST(TimestampFilterInterpreterTest, MscTimestampResetTest) {
  TimestampFilterInterpreterTestInterpreter* base_interpreter =
      new TimestampFilterInterpreterTestInterpreter;
  TimestampFilterInterpreter interpreter(base_interpreter, NULL);
  TestInterpreterWrapper wrapper(&interpreter);

  HardwareState hs[] = {
    { 1.000, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },
    { 1.010, 0, 1, 1, NULL, 0, 0, 0, 0, 0.012 },
    { 1.020, 0, 1, 1, NULL, 0, 0, 0, 0, 0.018 },
    { 1.030, 0, 1, 1, NULL, 0, 0, 0, 0, 0.031 },
    { 3.000, 0, 1, 1, NULL, 0, 0, 0, 0, 0.000 },  //msc_timestamp reset to 0
    { 3.010, 0, 1, 1, NULL, 0, 0, 0, 0, 0.008 },
    { 3.020, 0, 1, 1, NULL, 0, 0, 0, 0, 0.020 },
    { 3.030, 0, 1, 1, NULL, 0, 0, 0, 0, 0.035 }
  };

  stime_t expected_timestamps[] = {
    1.000, 1.012, 1.018, 1.031,
    3.000, 3.008, 3.020, 3.035
  };

  for (size_t i = 0; i < arraysize(hs); i++) {
    wrapper.SyncInterpret(&hs[i], NULL);
    EXPECT_EQ(hs[i].timestamp, expected_timestamps[i]);
  }
}
}  // namespace gestures
