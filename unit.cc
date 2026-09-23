/**
 *  @file unit.cc
 *  @brief drumlogue SDK unit interface for Lirah-4 synth
 *
 *  Copyright (c) 2026.
 */

#include <cstddef>
#include <cstdint>

#include "unit.h"   // Drumlogue synth callback declarations.
#include "synth.h"  // Lirah-4 voice and envelope engine.

static Synth s_synth_instance;
static unit_runtime_desc_t s_runtime_desc;
// Host-facing snapshot of parameter values.
// The drumlogue host reads values from this cache, which also restores state
// after resets.
static int32_t s_cached_values[UNIT_MAX_PARAM_COUNT];

// ---- Callback entry points from drumlogue runtime ----------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc)
    return k_unit_err_undef;

  if (desc->target != unit_header.target)
    return k_unit_err_target;

  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;

  s_runtime_desc = *desc;

  // Start from header defaults so first boot is deterministic.
  for (uint32_t i = 0; i < UNIT_MAX_PARAM_COUNT; ++i) {
    s_cached_values[i] = unit_header.params[i].init;
  }

  const int8_t ret = s_synth_instance.Init(desc);
  if (ret != k_unit_err_none)
    return ret;

  // Push cached values into the synth engine.
  for (uint8_t i = 0; i < unit_header.num_params; ++i) {
    s_synth_instance.setParameter(i, s_cached_values[i]);
  }

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
  s_synth_instance.Teardown();
}

__unit_callback void unit_reset() {
  s_synth_instance.Reset();
  // After DSP reset, restore the last host-visible parameter state.
  for (uint8_t i = 0; i < unit_header.num_params; ++i) {
    s_synth_instance.setParameter(i, s_cached_values[i]);
  }
}

__unit_callback void unit_resume() {
  s_synth_instance.Resume();
}

__unit_callback void unit_suspend() {
  s_synth_instance.Suspend();
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;
  s_synth_instance.Render(out, frames);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  if (id >= unit_header.num_params)
    return;

  const unit_param_t & p = unit_header.params[id];
  if (value < p.min)
    value = p.min;
  if (value > p.max)
    value = p.max;

  // Keep host and DSP state in lockstep.
  s_cached_values[id] = value;
  s_synth_instance.setParameter(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  if (id >= unit_header.num_params)
    return 0;
  return s_cached_values[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  if (id >= unit_header.num_params)
    return nullptr;

  const unit_param_t & p = unit_header.params[id];
  if (value < p.min)
    value = p.min;
  if (value > p.max)
    value = p.max;

  return s_synth_instance.getParameterStrValue(id, value);
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
  if (id >= unit_header.num_params)
    return nullptr;

  const unit_param_t & p = unit_header.params[id];
  if (value < p.min)
    value = p.min;
  if (value > p.max)
    value = p.max;

  return s_synth_instance.getParameterBmpValue(id, value);
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  const float bpm = (tempo >> 16) + (tempo & 0xFFFF) / static_cast<float>(0x10000);
  s_synth_instance.SetTempo(bpm);
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  s_synth_instance.NoteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  s_synth_instance.NoteOff(note);
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  s_synth_instance.GateOn(velocity);
}

__unit_callback void unit_gate_off() {
  s_synth_instance.GateOff();
}

__unit_callback void unit_all_note_off() {
  s_synth_instance.AllNoteOff();
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  s_synth_instance.PitchBend(bend);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  s_synth_instance.ChannelPressure(pressure);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  s_synth_instance.Aftertouch(note, aftertouch);
}

__unit_callback void unit_load_preset(uint8_t idx) {
  // Loading a preset updates DSP first, then mirrors values into host cache.
  s_synth_instance.LoadPreset(idx);
  for (uint8_t i = 0; i < unit_header.num_params; ++i) {
    s_cached_values[i] = s_synth_instance.getParameterValue(i);
  }
}

__unit_callback uint8_t unit_get_preset_index() {
  return s_synth_instance.getPresetIndex();
}

__unit_callback const char * unit_get_preset_name(uint8_t idx) {
  return Synth::getPresetName(idx);
}
