// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/gestures.h"

#include <sys/time.h>

#include <base/stringprintf.h>

#include "gestures/include/accel_filter_interpreter.h"
#include "gestures/include/box_filter_interpreter.h"
#include "gestures/include/click_wiggle_filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/fling_stop_filter_interpreter.h"
#include "gestures/include/iir_filter_interpreter.h"
#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/integral_gesture_filter_interpreter.h"
#include "gestures/include/logging.h"
#include "gestures/include/logging_filter_interpreter.h"
#include "gestures/include/lookahead_filter_interpreter.h"
#include "gestures/include/palm_classifying_filter_interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/scaling_filter_interpreter.h"
#include "gestures/include/cr48_profile_sensor_filter_interpreter.h"
#include "gestures/include/sensor_jump_filter_interpreter.h"
#include "gestures/include/split_correcting_filter_interpreter.h"
#include "gestures/include/stuck_button_inhibitor_filter_interpreter.h"
#include "gestures/include/t5r2_correcting_filter_interpreter.h"
#include "gestures/include/trace_marker.h"
#include "gestures/include/tracer.h"
#include "gestures/include/util.h"

using std::string;

// C API:

static const int kMinSupportedVersion = 1;
static const int kMaxSupportedVersion = 1;

stime_t StimeFromTimeval(const struct timeval* tv) {
  return static_cast<stime_t>(tv->tv_sec) +
      static_cast<stime_t>(tv->tv_usec) / 1000000.0;
}

stime_t StimeFromTimespec(const struct timespec* ts) {
  return static_cast<stime_t>(ts->tv_sec) +
      static_cast<stime_t>(ts->tv_nsec) / 1000000000.0;
}

std::string HardwareProperties::String() const {
  return StringPrintf("%f,  // left edge\n"
                      "%f,  // top edge\n"
                      "%f,  // right edge\n"
                      "%f,  // bottom edge\n"
                      "%f,  // x pixels/TP width\n"
                      "%f,  // y pixels/TP height\n"
                      "%f,  // x screen DPI\n"
                      "%f,  // y screen DPI\n"
                      "%f,  // orientation minimum\n"
                      "%f,  // orientation maximum\n"
                      "%u,  // max fingers\n"
                      "%u,  // max touch\n"
                      "%u,  // t5r2\n"
                      "%u,  // semi-mt\n"
                      "%u   // is button pad\n",
                      left, top, right, bottom,
                      res_x,
                      res_y,
                      screen_x_dpi,
                      screen_y_dpi,
                      orientation_minimum,
                      orientation_maximum,
                      max_finger_cnt,
                      max_touch_cnt,
                      supports_t5r2,
                      support_semi_mt,
                      is_button_pad);
}

string FingerState::String() const {
  return StringPrintf("{ %f, %f, %f, %f, %f, %f, %f, %f, %d, %d }",
                      touch_major, touch_minor,
                      width_major, width_minor,
                      pressure,
                      orientation,
                      position_x,
                      position_y,
                      tracking_id,
                      flags);
}

FingerState* HardwareState::GetFingerState(short tracking_id) {
  return const_cast<FingerState*>(
      const_cast<const HardwareState*>(this)->GetFingerState(tracking_id));
}

const FingerState* HardwareState::GetFingerState(short tracking_id) const {
  for (short i = 0; i < finger_cnt; i++) {
    if (fingers[i].tracking_id == tracking_id)
      return &fingers[i];
  }
  return NULL;
}

string HardwareState::String() const {
  string ret = StringPrintf("{ %f, %d, %d, %d, {",
                            timestamp,
                            buttons_down,
                            finger_cnt,
                            touch_cnt);
  for (size_t i = 0; i < finger_cnt; i++) {
    if (i != 0)
      ret += ",";
    ret += " ";
    ret += fingers[i].String();
  }
  if (finger_cnt > 0)
    ret += " ";
  ret += "} }";
  return ret;
}

bool HardwareState::SameFingersAs(const HardwareState& that) const {
  if (finger_cnt != that.finger_cnt || touch_cnt != that.touch_cnt)
    return false;
  // For now, require fingers to be in the same slots
  for (size_t i = 0; i < finger_cnt; i++)
    if (fingers[i].tracking_id != that.fingers[i].tracking_id)
      return false;
  return true;
}

string Gesture::String() const {
  switch (type) {
    case kGestureTypeNull:
      return "(Gesture type: null)";
    case kGestureTypeContactInitiated:
      return StringPrintf("(Gesture type: contactInitiated "
                          "start: %f stop: %f)", start_time, end_time);
    case kGestureTypeMove:
      return StringPrintf("(Gesture type: move start: %f stop: %f "
                          "dx: %f dy: %f)", start_time, end_time,
                          details.move.dx, details.move.dy);
    case kGestureTypeScroll:
      return StringPrintf("(Gesture type: scroll start: %f stop: %f "
                          "dx: %f dy: %f)", start_time, end_time,
                          details.scroll.dx, details.scroll.dy);
    case kGestureTypePinch:
      return StringPrintf("(Gesture type: pinch start: %f stop: %f "
                          "dz: %f)", start_time, end_time, details.pinch.dz);
    case kGestureTypeButtonsChange:
      return StringPrintf("(Gesture type: buttons start: %f stop: "
                          "%f down: %d up: %d)", start_time, end_time,
                          details.buttons.down, details.buttons.up);
    case kGestureTypeFling:
      return StringPrintf("(Gesture type: fling start: %f stop: "
                          "%f vx: %f vy: %f state: %s)", start_time, end_time,
                          details.fling.vx, details.fling.vy,
                          details.fling.fling_state == GESTURES_FLING_START ?
                          "start" : "tapdown");
    case kGestureTypeSwipe:
      return StringPrintf("(Gesture type: swipe start: %f stop: %f "
                          "dx: %f)", start_time, end_time, details.swipe.dx);
  }
  return "(Gesture type: unknown)";
}

bool Gesture::operator==(const Gesture& that) const {
  if (type != that.type)
    return false;
  switch (type) {
    case kGestureTypeNull:  // fall through
    case kGestureTypeContactInitiated:
      return true;
    case kGestureTypeMove:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          gestures::FloatEq(details.move.dx, that.details.move.dx) &&
          gestures::FloatEq(details.move.dy, that.details.move.dy);
    case kGestureTypeScroll:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          gestures::FloatEq(details.scroll.dx, that.details.scroll.dx) &&
          gestures::FloatEq(details.scroll.dy, that.details.scroll.dy);
    case kGestureTypePinch:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          gestures::FloatEq(details.pinch.dz, that.details.pinch.dz);
    case kGestureTypeButtonsChange:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          details.buttons.down == that.details.buttons.down &&
          details.buttons.up == that.details.buttons.up;
    case kGestureTypeFling:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          gestures::FloatEq(details.fling.vx, that.details.fling.vx) &&
          gestures::FloatEq(details.fling.vy, that.details.fling.vy);
    case kGestureTypeSwipe:
      return gestures::DoubleEq(start_time, that.start_time) &&
          gestures::DoubleEq(end_time, that.end_time) &&
          gestures::FloatEq(details.swipe.dx, that.details.swipe.dx);
  }
  return true;
}

GestureInterpreter* NewGestureInterpreterImpl(int version) {
  if (version < kMinSupportedVersion) {
    Err("Client too old. It's using version %d"
        ", but library has min supported version %d",
        version,
        kMinSupportedVersion);
    return NULL;
  }
  if (version > kMaxSupportedVersion) {
    Err("Client too new. It's using version %d"
        ", but library has max supported version %d",
        version,
        kMaxSupportedVersion);
    return NULL;
  }
  return new gestures::GestureInterpreter(version);
}

void DeleteGestureInterpreter(GestureInterpreter* obj) {
  delete obj;
}

void GestureInterpreterPushHardwareState(GestureInterpreter* obj,
                                         struct HardwareState* hwstate) {
  obj->PushHardwareState(hwstate);
}

void GestureInterpreterSetHardwareProperties(
    GestureInterpreter* obj,
    const struct HardwareProperties* hwprops) {
  obj->SetHardwareProperties(*hwprops);
}

void GestureInterpreterSetCallback(GestureInterpreter* obj,
                                   GestureReadyFunction fn,
                                   void* user_data) {
  obj->set_callback(fn, user_data);
}

void GestureInterpreterSetTimerProvider(GestureInterpreter* obj,
                                        GesturesTimerProvider* tp,
                                        void* data) {
  obj->SetTimerProvider(tp, data);
}

void GestureInterpreterSetPropProvider(GestureInterpreter* obj,
                                       GesturesPropProvider* pp,
                                       void* data) {
  obj->SetPropProvider(pp, data);
}

void GestureInterpreterInitialize(GestureInterpreter* obj) {
  obj->Initialize();
}

// C++ API:

GestureInterpreter::GestureInterpreter(int version)
    : callback_(NULL),
      callback_data_(NULL),
      timer_provider_(NULL),
      timer_provider_data_(NULL),
      interpret_timer_(NULL),
      loggingFilter_(NULL) {
  prop_reg_.reset(new PropRegistry);
  tracer_.reset(new Tracer(prop_reg_.get(), TraceMarker::StaticTraceWrite));
  finger_metrics_.reset(new FingerMetrics(prop_reg_.get()));
  TraceMarker::CreateTraceMarker();
}

GestureInterpreter::~GestureInterpreter() {
  SetTimerProvider(NULL, NULL);
  SetPropProvider(NULL, NULL);
  TraceMarker::DeleteTraceMarker();
}

namespace {
stime_t InternalTimerCallback(stime_t now, void* callback_data) {
  Log("TimerCallback called");
  GestureInterpreter* gi = reinterpret_cast<GestureInterpreter*>(callback_data);
  stime_t next = -1.0;
  gi->TimerCallback(now, &next);
  return next;
}
}

void GestureInterpreter::PushHardwareState(HardwareState* hwstate) {
  if (!interpreter_.get()) {
    Err("Filters are not composed yet!");
    return;
  }
  stime_t timeout = -1.0;
  Gesture* gs = interpreter_->SyncInterpret(hwstate, &timeout);
  if (gs && callback_)
    (*callback_)(callback_data_, gs);
  if (timer_provider_ && interpret_timer_) {
    if (timeout <= 0.0) {
      timer_provider_->cancel_fn(timer_provider_data_, interpret_timer_);
    } else {
      timer_provider_->set_fn(timer_provider_data_,
                              interpret_timer_,
                              timeout,
                              InternalTimerCallback,
                              this);
      Log("Setting timer for %f s out.", timeout);
    }
  } else {
    Err("No timer!");
  }
}

void GestureInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  if (!interpreter_.get()) {
    Err("Filters are not composed yet!");
    return;
  }
  interpreter_->SetHardwareProperties(hwprops);
}

void GestureInterpreter::TimerCallback(stime_t now, stime_t* timeout) {
  if (!interpreter_.get()) {
    Err("Filters are not composed yet!");
    return;
  }
  Gesture* gs = interpreter_->HandleTimer(now, timeout);
  if (gs && callback_)
    (*callback_)(callback_data_, gs);
}

void GestureInterpreter::SetTimerProvider(GesturesTimerProvider* tp,
                                          void* data) {
  if (timer_provider_ == tp && timer_provider_data_ == data)
    return;
  if (timer_provider_ && interpret_timer_) {
    timer_provider_->free_fn(timer_provider_data_, interpret_timer_);
    interpret_timer_ = NULL;
  }
  if (interpret_timer_)
    Log("How was interpret_timer_ not NULL?!");
  timer_provider_ = tp;
  timer_provider_data_ = data;
  if (timer_provider_)
    interpret_timer_ = timer_provider_->create_fn(timer_provider_data_);
}

void GestureInterpreter::SetPropProvider(GesturesPropProvider* pp,
                                         void* data) {
  prop_reg_->SetPropProvider(pp, data);
}

void GestureInterpreter::Initialize(void) {
  Interpreter* temp = new ImmediateInterpreter(prop_reg_.get(),
                                               finger_metrics_.get(),
                                               tracer_.get());
  temp = new FlingStopFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new ClickWiggleFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new PalmClassifyingFilterInterpreter(prop_reg_.get(), temp,
                                              finger_metrics_.get(),
                                              tracer_.get());
  temp = new IirFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new LookaheadFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new BoxFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new SensorJumpFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new AccelFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new SplitCorrectingFilterInterpreter(prop_reg_.get(), temp,
                                              tracer_.get());
  temp = new ScalingFilterInterpreter(prop_reg_.get(), temp, tracer_.get());
  temp = new IntegralGestureFilterInterpreter(temp, tracer_.get());
  temp = new StuckButtonInhibitorFilterInterpreter(temp, tracer_.get());
  temp = new T5R2CorrectingFilterInterpreter(prop_reg_.get(), temp,
                                             tracer_.get());
  temp = new Cr48ProfileSensorFilterInterpreter(prop_reg_.get(), temp,
                                                tracer_.get());
  temp = loggingFilter_ = new LoggingFilterInterpreter(prop_reg_.get(), temp,
                                                       tracer_.get());
  interpreter_.reset(temp);
  temp = NULL;
}

std::string GestureInterpreter::EncodeActivityLog() {
  return loggingFilter_->EncodeActivityLog();
}

const GestureMove kGestureMove = { 0, 0 };
const GestureScroll kGestureScroll = { 0, 0, 0 };
const GestureButtonsChange kGestureButtonsChange = { 0, 0 };
