// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>

#include "gestures/include/gestures.h"
#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/set.h"

#ifndef GESTURES_SPLIT_CORRECTING_FILTER_INTERPRETER_H_
#define GESTURES_SPLIT_CORRECTING_FILTER_INTERPRETER_H_

namespace gestures {

// This interepreter corrects problems that can occur with some touchpads.
// Currently, it corrects for the case where a large finger can erroneously
// "split" into two contacts. It works around this by looking for a contact
// to seemingly split into two, and fakes that the split didn't occur.

// This struct tracks an unmerged contact. By default the output and input
// IDs will be the same value, however after merge cycles, that may no longer
// be the case.
struct UnmergedContact {
  UnmergedContact() : input_id(-1) {}
  bool Valid() const { return input_id >= 0; }
  void Invalidate() { input_id = -1; }
  short input_id;
  short output_id;
  float position_x;
  float position_y;
};

// Tracks two input contacts that are being combined into one output contact
// because we believe they are actually two parts of the same real contact.
struct MergedContact {
  MergedContact() : output_id(-1) {}
  bool Valid() const { return output_id >= 0; }
  void Invalidate() { output_id = -1; }
  short input_ids[2];
  short output_id;
};

class SplitCorrectingFilterInterpreter : public Interpreter {
 public:
  // Takes ownership of |next|:
  SplitCorrectingFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~SplitCorrectingFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  void RemoveMissingUnmergedContacts(const HardwareState& hwstate);
  void MergeFingers(const HardwareState& hwstate);
  void UnmergeFingers(const HardwareState& hwstate);
  void UpdateUnmergedLocations(const HardwareState& hwstate);

  // Based on merged_ and unmeged_, updates the current hwstate.
  void UpdateHwState(HardwareState* hwstate) const;
  // Tests to see if new_contact, when paired w/ existing_contact
  // are a good match for the unmerged contact, merge_recipient.
  // new_contact is the current state of the finger in merge_recipient.
  // Returns < 0 if this is not a good match, or an error value if it's good.
  // The smaller the error, the better.
  float AreMergePair(const FingerState& existing_contact,
                     const FingerState& new_contact,
                     const UnmergedContact& merge_recipient) const;

  void AppendMergedContact(short input_id_a, short input_id_b, short output_id);
  void AppendUnmergedContact(const FingerState& fs, short output_id);

  const UnmergedContact* FindUnmerged(short input_id) const;
  const MergedContact* FindMerged(short input_id) const;

  static void JoinFingerState(FingerState* in_out,
                              const FingerState& newfinger);
  static void RemoveFingerStateFromHardwareState(HardwareState* hs,
                                                 FingerState* fs);

  // Sets last_tracking_ids_ to the ids in the passed hwstate.
  void SetLastTrackingIds(const HardwareState& hwstate);

  // Dumps internal state and hwstate.
  void Dump(const HardwareState& hwstate) const;

  scoped_ptr<Interpreter> next_;
  set<short, kMaxFingers> last_tracking_ids_;
  UnmergedContact unmerged_[kMaxFingers];
  MergedContact merged_[kMaxFingers / 2 + 1];

  // Contacts must be separated by less than this amount to be considered for
  // merging.
  DoubleProperty merge_max_separation_;
};

}  // namespace gestures

#endif  // GESTURES_SPLIT_CORRECTING_FILTER_INTERPRETER_H_