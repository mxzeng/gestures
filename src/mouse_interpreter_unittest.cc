// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/unittest_util.h"
#include "gestures/include/util.h"

namespace gestures {

HardwareProperties make_hwprops_for_mouse(
    unsigned has_wheel, unsigned wheel_is_hi_res) {
  return {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // touch-specific properties
    has_wheel,
    wheel_is_hi_res,
  };
}

class MouseInterpreterTest : public ::testing::Test {};

TEST(MouseInterpreterTest, SimpleTest) {
  HardwareProperties hwprops = make_hwprops_for_mouse(1, 0);
  MouseInterpreter mi(NULL, NULL);
  TestInterpreterWrapper wrapper(&mi, &hwprops);
  Gesture* gs;

  HardwareState hwstates[] = {
    { 200000, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0.0 },
    { 210000, 0, 0, 0, NULL, 9, -7, 0, 0, 0, 0.0 },
    { 220000, 1, 0, 0, NULL, 0, 0, 0, 0, 0, 0.0 },
    { 230000, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0.0 },
    { 240000, 0, 0, 0, NULL, 0, 0, -3, -360, 4, 0.0 },
  };

  gs = wrapper.SyncInterpret(&hwstates[0], NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), gs);

  gs = wrapper.SyncInterpret(&hwstates[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(9, gs->details.move.dx);
  EXPECT_EQ(-7, gs->details.move.dy);
  EXPECT_EQ(200000, gs->start_time);
  EXPECT_EQ(210000, gs->end_time);

  gs = wrapper.SyncInterpret(&hwstates[2], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeButtonsChange, gs->type);
  EXPECT_EQ(1, gs->details.buttons.down);
  EXPECT_EQ(0, gs->details.buttons.up);
  EXPECT_EQ(210000, gs->start_time);
  EXPECT_EQ(220000, gs->end_time);

  gs = wrapper.SyncInterpret(&hwstates[3], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeButtonsChange, gs->type);
  EXPECT_EQ(0, gs->details.buttons.down);
  EXPECT_EQ(1, gs->details.buttons.up);
  EXPECT_EQ(220000, gs->start_time);
  EXPECT_EQ(230000, gs->end_time);

  gs = wrapper.SyncInterpret(&hwstates[4], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  EXPECT_LT(-1, gs->details.scroll.dx);
  EXPECT_GT(1, gs->details.scroll.dy);
  EXPECT_EQ(240000, gs->start_time);
  EXPECT_EQ(240000, gs->end_time);
}

TEST(MouseInterpreterTest, HighResolutionVerticalScrollTest) {
  HardwareProperties hwprops = make_hwprops_for_mouse(1, 1);
  MouseInterpreter mi(NULL, NULL);
  TestInterpreterWrapper wrapper(&mi, &hwprops);
  Gesture* gs;

  HardwareState hwstates[] = {
    { 200000, 0, 0, 0, NULL, 0, 0,  0,   0, 0, 0.0 },
    { 210000, 0, 0, 0, NULL, 0, 0,  0, -15, 0, 0.0 },
    { 220000, 0, 0, 0, NULL, 0, 0, -1, -15, 0, 0.0 },
  };

  mi.hi_res_scrolling_.val_ = 1;

  gs = wrapper.SyncInterpret(&hwstates[0], NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), gs);

  gs = wrapper.SyncInterpret(&hwstates[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  EXPECT_EQ(0, gs->details.scroll.dx);
  float offset_of_8th_notch_scroll = gs->details.scroll.dy;
  EXPECT_LT(1, offset_of_8th_notch_scroll);

  gs = wrapper.SyncInterpret(&hwstates[2], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  EXPECT_EQ(0, gs->details.scroll.dx);
  // Having a low-res scroll event as well as the high-resolution one shouldn't
  // change the output value.
  EXPECT_NEAR(offset_of_8th_notch_scroll, gs->details.scroll.dy, 0.1);
}

}  // namespace gestures
