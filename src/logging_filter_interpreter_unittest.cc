// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/activity_replay.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging_filter_interpreter.h"
#include "gestures/include/prop_registry.h"

using std::string;

namespace gestures {

class LoggingFilterInterpreterTest : public ::testing::Test {};

class LoggingFilterInterpreterTestInterpreter : public Interpreter {
 public:
  explicit LoggingFilterInterpreterTestInterpreter(PropRegistry* prop_reg)
      : expected_hwstate_(NULL),
        set_hwprops_call_count_(0),
        interpret_call_count_(0),
        handle_timer_call_count_(0),
        bool_prop_(prop_reg, "BoolProp", 0),
        double_prop_(prop_reg, "DoubleProp", 0),
        int_prop_(prop_reg, "IntProp", 0),
        short_prop_(prop_reg, "ShortProp", 0),
        string_prop_(prop_reg, "StringProp", "") {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    interpret_call_count_++;
    EXPECT_NE(0, bool_prop_.val_);
    EXPECT_NE(0, double_prop_.val_);
    EXPECT_NE(0, int_prop_.val_);
    EXPECT_NE(0, short_prop_.val_);
    EXPECT_NE("", string_prop_.val_);
    EXPECT_TRUE(expected_hwstate_);
    EXPECT_DOUBLE_EQ(expected_hwstate_->timestamp, hwstate->timestamp);
    EXPECT_EQ(expected_hwstate_->buttons_down, hwstate->buttons_down);
    EXPECT_EQ(expected_hwstate_->finger_cnt, hwstate->finger_cnt);
    EXPECT_EQ(expected_hwstate_->touch_cnt, hwstate->touch_cnt);
    if (expected_hwstate_->finger_cnt == hwstate->finger_cnt)
      for (size_t i = 0; i < expected_hwstate_->finger_cnt; i++)
        EXPECT_TRUE(expected_hwstate_->fingers[i] == hwstate->fingers[i]);
    *timeout = 0.01;
    return &return_value_;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) {
    handle_timer_call_count_++;
    return &return_value_;
  }

  virtual void SetHardwareProperties(const HardwareProperties& hw_props) {
    set_hwprops_call_count_++;
    EXPECT_FLOAT_EQ(expected_hwprops_.left, hw_props.left);
    EXPECT_FLOAT_EQ(expected_hwprops_.top, hw_props.top);
    EXPECT_FLOAT_EQ(expected_hwprops_.right, hw_props.right);
    EXPECT_FLOAT_EQ(expected_hwprops_.bottom, hw_props.bottom);
    EXPECT_FLOAT_EQ(expected_hwprops_.res_x, hw_props.res_x);
    EXPECT_FLOAT_EQ(expected_hwprops_.res_y, hw_props.res_y);
    EXPECT_FLOAT_EQ(expected_hwprops_.screen_x_dpi, hw_props.screen_x_dpi);
    EXPECT_FLOAT_EQ(expected_hwprops_.screen_y_dpi, hw_props.screen_y_dpi);
    EXPECT_EQ(expected_hwprops_.max_finger_cnt, hw_props.max_finger_cnt);
    EXPECT_EQ(expected_hwprops_.max_touch_cnt, hw_props.max_touch_cnt);
    EXPECT_EQ(expected_hwprops_.supports_t5r2, hw_props.supports_t5r2);
    EXPECT_EQ(expected_hwprops_.support_semi_mt, hw_props.support_semi_mt);
    EXPECT_EQ(expected_hwprops_.is_button_pad, hw_props.is_button_pad);
  };

  Gesture return_value_;
  HardwareState* expected_hwstate_;
  HardwareProperties expected_hwprops_;
  int set_hwprops_call_count_;
  int interpret_call_count_;
  int handle_timer_call_count_;
  BoolProperty bool_prop_;
  DoubleProperty double_prop_;
  IntProperty int_prop_;
  ShortProperty short_prop_;
  StringProperty string_prop_;
};


TEST(LoggingFilterInterpreterTest, SimpleTest) {
  PropRegistry prop_reg;
  LoggingFilterInterpreterTestInterpreter* base_interpreter =
      new LoggingFilterInterpreterTestInterpreter(&prop_reg);
  base_interpreter->bool_prop_.val_ = 1;
  base_interpreter->double_prop_.val_ = 1;
  base_interpreter->int_prop_.val_ = 1;
  base_interpreter->short_prop_.val_ = 1;
  base_interpreter->string_prop_.val_ = "x";

  LoggingFilterInterpreter interpreter(&prop_reg, base_interpreter);

  HardwareProperties hwprops = {
    0, 0, 100, 100,  // left, top, right, bottom
    10,  // x res (pixels/mm)
    10,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    1, 0, 0  //t5r2, semi, button pad
  };
  base_interpreter->expected_hwprops_ = hwprops;
  interpreter.SetHardwareProperties(hwprops);
  EXPECT_EQ(1, base_interpreter->set_hwprops_call_count_);
  base_interpreter->return_value_ = Gesture(kGestureMove,
                                            0,  // start time
                                            1,  // end time
                                            -4,  // dx
                                            2.8);  // dy

  FingerState finger_state = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    0, 0, 0, 0, 10, 0, 50, 50, 1, 0
  };
  HardwareState hardware_state = {
    // time, buttons, finger count, touch count, finger states pointer
    200000, 0, 1, 1, &finger_state
  };
  stime_t timeout = -1.0;
  base_interpreter->expected_hwstate_ = &hardware_state;
  Gesture* result = interpreter.SyncInterpret(&hardware_state, &timeout);
  EXPECT_TRUE(base_interpreter->return_value_ == *result);
  ASSERT_GT(timeout, 0);
  stime_t now = hardware_state.timestamp + timeout;
  timeout = -1.0;
  result = interpreter.HandleTimer(now, &timeout);
  EXPECT_TRUE(base_interpreter->return_value_ == *result);
  ASSERT_LT(timeout, 0);
  EXPECT_EQ(1, base_interpreter->interpret_call_count_);
  EXPECT_EQ(1, base_interpreter->handle_timer_call_count_);

  // Now, get the log
  string initial_log = interpreter.log_.Encode();

  // Make a new interpreter and push the log through it
  PropRegistry prop_reg2;
  LoggingFilterInterpreterTestInterpreter* base_interpreter2 =
      new LoggingFilterInterpreterTestInterpreter(&prop_reg2);
  base_interpreter2->return_value_ = base_interpreter->return_value_;
  LoggingFilterInterpreter interpreter2(&prop_reg2, base_interpreter2);

  ActivityReplay replay(&prop_reg2);
  replay.Parse(initial_log);

  base_interpreter2->expected_hwstate_ = &hardware_state;
  base_interpreter2->expected_hwprops_ = hwprops;

  EXPECT_TRUE(replay.Replay(&interpreter2));
  string final_log = interpreter2.log_.Encode();
  EXPECT_EQ(initial_log, final_log);
  EXPECT_EQ(1, base_interpreter2->interpret_call_count_);
  EXPECT_EQ(1, base_interpreter2->handle_timer_call_count_);
}

class LoggingFilterInterpreterResetLogTestInterpreter : public Interpreter {
  public:
    LoggingFilterInterpreterResetLogTestInterpreter() {}
    virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
      return NULL;
    }
    virtual void SetHardwareProperties(const HardwareProperties& hw_props) {}
    virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) {
      return NULL;
    }
};

TEST(LoggingFilterInterpreterTest, ResetLogTest) {
  PropRegistry prop_reg;
  LoggingFilterInterpreterResetLogTestInterpreter* base_interpreter =
      new LoggingFilterInterpreterResetLogTestInterpreter();
  LoggingFilterInterpreter interpreter(&prop_reg, base_interpreter);

  HardwareProperties hwprops = {
    0, 0, 100, 100,  // left, top, right, bottom
    10,  // x res (pixels/mm)
    10,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    1, 0, 0  //t5r2, semi, button pad
  };

  interpreter.SetHardwareProperties(hwprops);
  FingerState finger_state = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    0, 0, 0, 0, 10, 0, 50, 50, 1, 0
  };
  HardwareState hardware_state = {
    // time, buttons, finger count, touch count, finger states pointer
    200000, 0, 1, 1, &finger_state
  };
  stime_t timeout = -1.0;
  interpreter.SyncInterpret(&hardware_state, &timeout);
  EXPECT_EQ(interpreter.log_.size(), 1);

  interpreter.SyncInterpret(&hardware_state, &timeout);
  EXPECT_EQ(interpreter.log_.size(), 2);

  // Assume the ResetLog property is set.
  interpreter.logging_reset_.HandleGesturesPropWritten();
  EXPECT_EQ(interpreter.log_.size(), 0);

  interpreter.SyncInterpret(&hardware_state, &timeout);
  EXPECT_EQ(interpreter.log_.size(), 1);
}
}  // namespace gestures
