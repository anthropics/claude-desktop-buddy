#pragma once
#include <cstdint>

enum PersonaState {
  PS_SLEEP = 0, PS_IDLE, PS_BUSY, PS_ATTENTION, PS_CELEBRATE, PS_DIZZY, PS_HEART,
  PS_COUNT
};

struct PersonaInputs {
  bool          connected;
  int           running;
  unsigned long heartUntil;
  unsigned long now;
  bool          debugActive;
  PersonaState  debugState;
};

inline PersonaState derivePersonaState(const PersonaInputs& in) {
  if (in.debugActive)            return in.debugState;
  if (!in.connected)             return PS_SLEEP;
  if (in.now < in.heartUntil)    return PS_HEART;
  if (in.running > 0)            return PS_BUSY;
  return PS_IDLE;
}
