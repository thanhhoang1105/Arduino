#include "valve_control.h"
#include "../config.h"
#include "../storage/preferences.h"
#include "../hardware/hardware.h"

void setValveState(uint8_t index, bool state) {
  if (index < 10) {
    setWorkingValveState(index, state);
    updateOutputsFromWorking();
  }
}

void setPumpState(uint8_t index, bool state) {
  if (index < 2) {
    setWorkingPumpState(index, state);
    updateOutputsFromWorking();
  }
}

void syncValvesWithSaved() {
  for (uint8_t i = 0; i < 10; i++) {
    setWorkingValveState(i, getSavedValveState(i));
  }
  updateOutputsFromWorking();
}

void syncPumpsWithSaved() {
  for (uint8_t j = 0; j < 2; j++) {
    setWorkingPumpState(j, getSavedPumpState(j));
  }
  updateOutputsFromWorking();
}