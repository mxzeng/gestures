// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/interpreter.h"

#include <cxxabi.h>
#include <string>

#include <base/json/json_writer.h>
#include <base/values.h>

#include "gestures/include/activity_log.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"
#include "gestures/include/tracer.h"

namespace gestures {

Interpreter::Interpreter(PropRegistry* prop_reg,
                         Tracer* tracer,
                         bool force_logging)
    : log_(NULL),
      name_(NULL),
      tracer_(tracer) {
#ifdef DEEP_LOGS
  bool logging_enabled = true;
#else
  bool logging_enabled = force_logging;
#endif
  if (logging_enabled)
    log_.reset(new ActivityLog(prop_reg));
}

Interpreter::~Interpreter() {
  if (name_)
    free(const_cast<char*>(name_));
}

void Interpreter::Trace(const char* message, const char* name) {
  if (tracer_)
    tracer_->Trace(message, name);
}

Gesture* Interpreter::SyncInterpret(HardwareState* hwstate,
                                    stime_t* timeout) {
  if (log_.get() && hwstate) {
    Trace("log: start: ", "LogHardwareState");
    log_->LogHardwareState(*hwstate);
    Trace("log: end: ", "LogHardwareState");
  }
  Trace("SyncInterpret: start: ", name());
  Gesture* result = SyncInterpretImpl(hwstate, timeout);
  Trace("SyncInterpret: end: ", name());
  LogOutputs(result, timeout, "SyncLogOutputs");
  return result;
}

Gesture* Interpreter::HandleTimer(stime_t now, stime_t* timeout) {
  if (log_.get()) {
    Trace("log: start: ", "LogTimerCallback");
    log_->LogTimerCallback(now);
    Trace("log: end: ", "LogTimerCallback");
  }
  Trace("HandleTimer: start: ", name());
  Gesture* result = HandleTimerImpl(now, timeout);
  Trace("HandleTimer: end: ", name());
  LogOutputs(result, timeout, "TimerLogOutputs");
  return result;
}

void Interpreter::SetHardwareProperties(const HardwareProperties& hwprops) {
  if (log_.get()) {
    Trace("log: start: ", "SetHardwareProperties");
    log_->SetHardwareProperties(hwprops);
    Trace("log: end: ", "SetHardwareProperties");
  }
  SetHardwarePropertiesImpl(hwprops);
}

DictionaryValue* Interpreter::EncodeCommonInfo() {
  DictionaryValue* root = log_.get() ?
      log_->EncodeCommonInfo() : new DictionaryValue;
  root->Set(ActivityLog::kKeyInterpreterName,
            new StringValue(std::string(name())));
  return root;
}

std::string Interpreter::Encode() {
  DictionaryValue *root;
  root = EncodeCommonInfo();
  if (log_.get())
    root = log_->AddEncodeInfo(root);

  std::string out;
  base::JSONWriter::Write(root, true, &out);
  delete root;
  return out;
}

void Interpreter::InitName() {
  if (!name_) {
    int status;
    char* full_name = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
    if (full_name == NULL) {
      if (status == -1)
        Err("Memory allocation failed");
      else if (status == -2)
        Err("Mangled_name is not a valid name");
      else if (status == -3)
        Err("One of the arguments is invalid");
      return;
    }
    // the return value of abi::__cxa_demangle(...) is gestures::XXXInterpreter
    char* last_colon = strrchr(full_name, ':');
    char* class_name;
    if (last_colon)
      class_name = last_colon + 1;
    else
      class_name = full_name;
    name_ = strdup(class_name);
    free(full_name);
  }
}

void Interpreter::LogOutputs(Gesture* result,
                             stime_t* timeout,
                             const char* action) {
  if (!log_.get())
    return;
  Trace("log: start: ", action);
  if (result)
    log_->LogGesture(*result);
  if (timeout && *timeout >= 0.0)
    log_->LogCallbackRequest(*timeout);
  Trace("log: end: ", action);
}
}  // namespace gestures
