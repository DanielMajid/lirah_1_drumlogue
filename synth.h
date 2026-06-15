#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "unit.h"

// ============================================================================
//  Lirah-1 Poly — 4-voice polyphonic FM+wavefold synthesizer for drumlogue
//
//  Architecture overview:
//    - 4 independent voices, each running a full FM carrier+modulator pair
//      with its own hyper-LFO phases, wavefold, feedback, and amp envelope.
//    - Global state shared across all voices: LFO3 (slow scene modulator),
//      pitch bend, channel pressure, and the main Params block.
//    - Four "spread" parameters (FM, Fold, Feedback, Tune) let each voice
//      diverge from the global value by an amount proportional to its
//      position in the voice pool, creating automatic voice detuning and
//      timbral spread similar to unison stacking.
//    - Voice allocation: prefers free slots, then oldest releasing voice,
//      then oldest active voice (oldest-first steal).
// ============================================================================

class Synth {
 public:
  static constexpr uint8_t k_num_voices  = 4;
  // 10 original params + 4 spread params (FM, Fold, Feedback, Tune)
  static constexpr uint8_t k_num_params  = 14;
  static constexpr uint8_t k_num_presets = 8;

  // Output is divided by voice count to keep full-chord level consistent
  static constexpr float k_voice_gain = 0.25f;

  // ---- Parameter index enumeration ----------------------------------------

  enum ParameterId : uint8_t {
    // ---- Original 10 parameters (unchanged IDs) ----------------------------
    k_param_fm_depth = 0,       // FM modulation depth (0-1023)
    k_param_hyper_lfo_depth,    // Hyper-LFO gate amplitude (0-1023)
    k_param_lfo1_rate,          // Hyper-LFO 1 rate in tenths of Hz (0-100)
    k_param_lfo2_rate,          // Hyper-LFO 2 rate in tenths of Hz (0-100)
    k_param_fold,               // Wavefold drive in tenths (0-100)
    k_param_fm_tune,            // FM modulator ratio offset 0-100 = 0-100%
    k_param_pitch,              // Carrier pitch offset 0-100 = 0-100%
    k_param_feedback,           // Output-to-input feedback (0-100)
    k_param_lfo3_target,        // Which param LFO3 modulates (enum string)
    k_param_lfo3_rate,          // LFO3 rate (0-100, logarithmic mapping)
    // ---- Voice spread parameters (params 10-13) ----------------------------
    k_param_fm_spread,          // FM depth spread across voices (0-100)
    k_param_fold_spread,        // Wavefold spread across voices (0-100)
    k_param_feedback_spread,    // Feedback spread across voices (0-100)
    k_param_tune_spread,        // Pitch detune spread in semitones (0-100)
  };

  // Targets available for LFO3 scene modulation
  enum Lfo3Target : uint8_t {
    k_lfo3_target_off = 0,
    k_lfo3_target_fm_depth,
    k_lfo3_target_hyper_depth,
    k_lfo3_target_hyper_rate1,
    k_lfo3_target_hyper_rate2,
    k_lfo3_target_fold,
    k_lfo3_target_mod_tune,
    k_lfo3_target_osc_tune,
    k_lfo3_target_feedback,
  };

  // ---- Global parameter block (one copy, shared by all voices) -------------

  struct Params {
    float fmDepth;          // FM depth in native units (raw * k_fm_depth_scale)
    float lfoDepth;         // Hyper-LFO gate amplitude (0-1)
    float lfoRate1;         // Hyper-LFO 1 frequency in Hz
    float lfoRate2;         // Hyper-LFO 2 frequency in Hz
    float waveFold;         // Wavefold drive amount (0-10)
    int32_t modTune;        // FM modulator ratio offset (0-100)
    int32_t oscTune;        // Carrier frequency offset (0-100)
    float feedback;         // Feedback coefficient
    uint8_t lfo3Target;     // Which param LFO3 sweeps
    float lfo3Rate;         // LFO3 frequency in Hz
    // Per-voice spread amounts — the physical offset applied per voice step.
    // Voice positions are -1.5, -0.5, +0.5, +1.5 so the mean is always 0.
    float fmSpread;         // FM depth delta per voice step (native units)
    float foldSpread;       // Wavefold delta per voice step
    float feedbackSpread;   // Feedback delta per voice step
    float tuneSpread;       // Detune in semitones per voice step

    inline void reset() {
      fmDepth        = 0.f;
      lfoDepth       = 0.f;
      lfoRate1       = 1.f;
      lfoRate2       = 2.f;
      waveFold       = 0.f;
      modTune        = 0;
      oscTune        = 0;
      feedback       = 0.f;
      lfo3Target     = k_lfo3_target_off;
      lfo3Rate       = 0.f;
      fmSpread       = 0.f;
      foldSpread     = 0.f;
      feedbackSpread = 0.f;
      tuneSpread     = 0.f;
    }
  };

  // ---- Per-voice oscillator and envelope state -----------------------------
  //
  // Each voice owns independent phase accumulators for its carrier, modulator,
  // and both hyper-LFOs.  This means held chords have independent hyper-gate
  // rhythms, producing the chorusing / "washy" effect when voices diverge.

  struct Voice {
    float carrierPhase;   // FM carrier phase accumulator (0-1)
    float modPhase;       // FM modulator phase accumulator (0-1)
    float lfo1Phase;      // Hyper-LFO 1 phase accumulator (0-1)
    float lfo2Phase;      // Hyper-LFO 2 phase accumulator (0-1)
    float prevSample;     // Last output sample, used for feedback path
    float baseW0;         // Normalized note frequency: Hz / sampleRate
    float velocityAmp;    // Amplitude scaled from MIDI velocity
    float ampEnv;         // Running amplitude envelope (0-velocityAmp)
    bool  gateOn;         // True while MIDI note is held
    uint8_t note;         // MIDI note number; 0xFF = slot is free
    uint32_t age;         // Monotonic counter value at trigger time (for steal)

    inline void reset() {
      carrierPhase = 0.f;
      modPhase     = 0.f;
      lfo1Phase    = 0.f;
      lfo2Phase    = 0.f;
      prevSample   = 0.f;
      baseW0       = 0.f;
      velocityAmp  = 0.f;
      ampEnv       = 0.f;
      gateOn       = false;
      note         = 0xFF; // sentinel: slot is unoccupied
      age          = 0;
    }
  };

  // ---- Lifecycle -----------------------------------------------------------

  inline int8_t Init(const unit_runtime_desc_t *desc) {
    if (desc->samplerate != k_sample_rate_hz)
      return k_unit_err_samplerate;
    if (desc->output_channels != 2)
      return k_unit_err_geometry;

    Reset();
    return k_unit_err_none;
  }

  inline void Teardown() {}

  inline void Reset() {
    params_.reset();

    // Clear all raw parameter storage (preserves LFO rate defaults below)
    for (uint8_t i = 0; i < k_num_params; ++i)
      rawParams_[i] = 0;
    rawParams_[k_param_lfo1_rate] = 10;
    rawParams_[k_param_lfo2_rate] = 20;
    rawParams_[k_param_lfo3_rate] = 10;

    // Reset all voice slots to inactive
    for (uint8_t i = 0; i < k_num_voices; ++i)
      voices_[i].reset();

    lfo3Phase_    = 0.f;
    pitchBendMul_ = 1.f;
    pressureMod_  = 0.f;
    tempoBpm_     = 120.f;
    presetIndex_  = 0;
    voiceAge_     = 0;
    lastNote_     = 60; // default to middle C for gate-only triggers
  }

  inline void Resume()  {}
  inline void Suspend() {}

  // ---- Render --------------------------------------------------------------

  // renderVoiceSample — advances one voice by exactly one audio sample.
  //
  // Parameters:
  //   v          — voice state (modified in place)
  //   baseW0     — per-voice detuned normalized frequency (pre-computed above)
  //   fmDepth    — per-voice FM depth (global + voice spread offset)
  //   hyperDepth — hyper-LFO gate amplitude (global, post-LFO3)
  //   hyperRate1 — hyper-LFO 1 Hz (global, post-LFO3)
  //   hyperRate2 — hyper-LFO 2 Hz (global, post-LFO3)
  //   fold       — per-voice wavefold drive
  //   modTune    — FM modulator ratio offset (global)
  //   oscTune    — carrier pitch offset (global)
  //   feedback   — per-voice feedback coefficient
  //
  // Returns the folded, envelope-scaled output sample for this voice.

  inline float renderVoiceSample(Voice &v, float baseW0,
                                  float fmDepth,  float hyperDepth,
                                  float hyperRate1, float hyperRate2,
                                  float fold,     float modTune,
                                  float oscTune,  float feedback) {
    // Advance this voice's hyper-LFOs independently so held chords diverge
    v.lfo1Phase += hyperRate1 * k_sample_rate_recip;
    v.lfo1Phase -= static_cast<uint32_t>(v.lfo1Phase);
    v.lfo2Phase += hyperRate2 * k_sample_rate_recip;
    v.lfo2Phase -= static_cast<uint32_t>(v.lfo2Phase);

    const float lfo1Out = std::sin(k_two_pi * v.lfo1Phase);
    const float lfo2Out = std::sin(k_two_pi * v.lfo2Phase);

    // Hyper gate: both LFOs positive at the same time = 3x frequency boost burst
    const float hyperGate = (lfo1Out >= 0.f && lfo2Out >= 0.f) ? 3.f : 0.f;
    const float hyperMod  = 1.f + hyperGate * hyperDepth;

    // Pressure deepens FM (channel pressure / poly AT modulates timbre)
    const float fmScale = fmDepth * (0.5f + 0.5f * pressureMod_);

    const float modFreqMul = 1.f + modTune * 0.01f;
    const float oscFreqMul = 1.f + oscTune * 0.01f;
    const float w0 = baseW0 * pitchBendMul_;

    // FM modulator: produces the modulating sine for the carrier's phase
    const float fmSig = std::sin(k_two_pi * v.modPhase);
    v.modPhase += w0 * modFreqMul * hyperMod;
    v.modPhase -= static_cast<uint32_t>(v.modPhase);

    // FM carrier: pitch is shifted by the modulator signal each sample
    const float carrW0  = w0 * oscFreqMul * hyperMod + fmSig * fmScale * k_sample_rate_recip;
    const float carrier = 0.5f * std::sin(k_two_pi * v.carrierPhase);
    v.carrierPhase += carrW0;
    v.carrierPhase -= static_cast<uint32_t>(v.carrierPhase);

    // Wavefold with per-voice feedback path
    const float foldDrive   = 1.f + fold;
    const float feedbackMix = 1.f + v.prevSample * feedback;
    const float mainOsc     = carrier * foldDrive * feedbackMix;

    // Triangle-wave folder: reflects the signal at ±0.5, keeping output bounded
    const float folded = (mainOsc < -0.5f) ? (-1.f - mainOsc)
                       : (mainOsc >  0.5f) ? ( 1.f - mainOsc)
                                           :          mainOsc;

    v.prevSample = clampf(mainOsc, -1.f, 1.f);

    // Exponential amplitude envelope — glide toward target using one-pole filter
    const float targetAmp = v.gateOn ? v.velocityAmp : 0.f;
    const float envCoeff  = v.gateOn ? k_amp_attack   : k_amp_release;
    v.ampEnv += (targetAmp - v.ampEnv) * envCoeff;

    // Once a released voice fully decays, mark its slot free for reuse
    if (!v.gateOn && v.ampEnv < 1e-5f)
      v.note = 0xFF;

    return folded * v.ampEnv;
  }

  inline void Render(float *out, size_t frames) {
    // Snapshot params once per block to avoid mid-block parameter tearing
    const Params p      = params_;
    const float lfo3W0  = p.lfo3Rate * k_sample_rate_recip;

    // Pre-compute per-voice pitch-detune multipliers once per render block.
    // Each voice sits at position {-1.5, -0.5, +0.5, +1.5} so the mean
    // detune across all voices is always 0 — the root pitch is preserved.
    // std::pow is called only 4 times per block, never inside the sample loop.
    float voiceW0Mul[k_num_voices];
    for (uint8_t vi = 0; vi < k_num_voices; ++vi) {
      const float spreadPos = static_cast<float>(vi) - 1.5f;
      const float semitones = p.tuneSpread * spreadPos;
      voiceW0Mul[vi] = std::pow(2.f, semitones * (1.f / 12.f));
    }

    float *out_p = out;
    for (size_t i = 0; i < frames; ++i, out_p += 2) {
      // Advance the global LFO3 once per sample (shared scene modulator)
      lfo3Phase_ += lfo3W0;
      lfo3Phase_ -= static_cast<uint32_t>(lfo3Phase_);
      const float lfo3 = std::sin(k_two_pi * lfo3Phase_);

      // Start from global param values and apply LFO3 modulation.
      // Each voice will then add its own spread offset on top of these.
      float sceneFmDepth    = p.fmDepth;
      float sceneHyperDepth = p.lfoDepth;
      float sceneHyperRate1 = p.lfoRate1;
      float sceneHyperRate2 = p.lfoRate2;
      float sceneFold       = p.waveFold;
      float sceneModTune    = static_cast<float>(p.modTune);
      float sceneOscTune    = static_cast<float>(p.oscTune);
      float sceneFeedback   = p.feedback;

      applyLfo3Modulation(p.lfo3Target, lfo3,
                          sceneFmDepth, sceneHyperDepth,
                          sceneHyperRate1, sceneHyperRate2,
                          sceneFold, sceneModTune, sceneOscTune, sceneFeedback);

      // Accumulate all voice outputs into a mono mix
      float mix = 0.f;
      for (uint8_t vi = 0; vi < k_num_voices; ++vi) {
        Voice &v = voices_[vi];

        // Skip slots that are both unassigned and fully silent
        if (v.note == 0xFF && v.ampEnv < 1e-5f)
          continue;

        // Voice spread: positions are -1.5, -0.5, +0.5, +1.5.
        // Adding p.*Spread * spreadPos shifts each voice's parameter by an
        // equal step in opposite directions, so the chord average equals
        // the unspread global value.
        const float spreadPos = static_cast<float>(vi) - 1.5f;

        const float fmDepth  = clampf(sceneFmDepth   + p.fmSpread       * spreadPos, 0.f, 2046.f);
        const float fold     = clampf(sceneFold       + p.foldSpread     * spreadPos, 0.f, 10.f);
        const float feedback = clampf(sceneFeedback   + p.feedbackSpread * spreadPos, 0.f, 2.f);

        // Apply pre-computed pitch detune for this voice position
        const float voiceBaseW0 = v.baseW0 * voiceW0Mul[vi];

        mix += renderVoiceSample(v, voiceBaseW0, fmDepth, sceneHyperDepth,
                                 sceneHyperRate1, sceneHyperRate2,
                                 fold, sceneModTune, sceneOscTune, feedback);
      }

      // Scale the summed output to prevent clipping with multiple active voices
      const float outSample = mix * k_voice_gain;
      out_p[0] = outSample;
      out_p[1] = outSample;
    }
  }

  // ---- Parameters ----------------------------------------------------------

  inline void setParameter(uint8_t index, int32_t value) {
    if (index < k_num_params)
      rawParams_[index] = value;

    switch (index) {
      case k_param_fm_depth:
        params_.fmDepth = value * k_fm_depth_scale;
        break;
      case k_param_hyper_lfo_depth:
        params_.lfoDepth = value * (1.f / 1023.f);
        break;
      case k_param_lfo1_rate:
        params_.lfoRate1 = value * 0.1f;
        break;
      case k_param_lfo2_rate:
        params_.lfoRate2 = value * 0.1f;
        break;
      case k_param_fold:
        params_.waveFold = value * 0.1f;
        break;
      case k_param_fm_tune:
        params_.modTune = value;
        break;
      case k_param_pitch:
        params_.oscTune = value;
        break;
      case k_param_feedback:
        params_.feedback = value * 0.02f;
        break;
      case k_param_lfo3_target:
        if (value < k_lfo3_target_off)      value = k_lfo3_target_off;
        if (value > k_lfo3_target_feedback)  value = k_lfo3_target_feedback;
        params_.lfo3Target = static_cast<uint8_t>(value);
        break;
      case k_param_lfo3_rate: {
        const float norm = clampf(value * 0.01f, 0.f, 1.f);
        // Lower half: 0-1 Hz (fine control); upper half: 1-25 Hz (fast LFO)
        params_.lfo3Rate = (norm <= 0.5f) ? norm * 2.f
                                          : 1.f + (norm - 0.5f) * 48.f;
      } break;

      // ---- Spread parameters (0-100 raw → physical spread per voice step) --

      case k_param_fm_spread:
        // 100 raw = 500 FM depth units per step; max voice spread = ±750 units
        params_.fmSpread = value * 5.f;
        break;
      case k_param_fold_spread:
        // 100 raw = 2.0 fold units per step; max spread = ±3 fold units
        params_.foldSpread = value * 0.02f;
        break;
      case k_param_feedback_spread:
        // 100 raw = 0.4 feedback units per step; max spread = ±0.6
        params_.feedbackSpread = value * 0.004f;
        break;
      case k_param_tune_spread:
        // 100 raw = 1.0 semitone per step; max spread = ±1.5 semitones (3 total)
        params_.tuneSpread = value * 0.01f;
        break;

      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    if (index >= k_num_params)
      return 0;
    return rawParams_[index];
  }

  inline const char *getParameterStrValue(uint8_t index, int32_t value) const {
    static const char *targetNames[] = {"OFF",   "FMDEP", "HDEP", "HR1", "HR2",
                                        "FOLD",  "FMTUN", "OTUN", "FDBK"};
    if (index != k_param_lfo3_target)
      return nullptr;
    if (value < k_lfo3_target_off)       value = k_lfo3_target_off;
    if (value > k_lfo3_target_feedback)  value = k_lfo3_target_feedback;
    return targetNames[value];
  }

  inline const uint8_t *getParameterBmpValue(uint8_t, int32_t) const { return nullptr; }

  // ---- Voice allocation ----------------------------------------------------

  // Selects a voice slot for an incoming note.
  // Priority order: (1) free slot, (2) oldest releasing voice, (3) oldest active voice.
  // "Oldest" is determined by the monotonic voiceAge_ counter stored at trigger time.
  inline uint8_t allocVoice() {
    uint8_t bestFree     = 0xFF;
    uint8_t bestRelease  = 0xFF;
    uint8_t bestActive   = 0xFF;
    uint32_t oldestRelAge = 0xFFFFFFFFU;
    uint32_t oldestActAge = 0xFFFFFFFFU;

    for (uint8_t i = 0; i < k_num_voices; ++i) {
      const Voice &v = voices_[i];
      if (v.note == 0xFF) {
        bestFree = i;
        break; // free slot: no need to search further
      }
      if (!v.gateOn) {
        if (v.age < oldestRelAge) { oldestRelAge = v.age; bestRelease = i; }
      } else {
        if (v.age < oldestActAge) { oldestActAge = v.age; bestActive  = i; }
      }
    }

    if (bestFree    != 0xFF) return bestFree;
    if (bestRelease != 0xFF) return bestRelease;
    return bestActive; // steal oldest active voice as last resort
  }

  // ---- MIDI callbacks ------------------------------------------------------

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    lastNote_ = note;
    const uint8_t vi = allocVoice();
    Voice &v = voices_[vi];

    v.note        = note;
    v.baseW0      = midiNoteToW0(static_cast<float>(note));
    v.velocityAmp = velocityToAmp(velocity);
    v.gateOn      = true;
    v.age         = voiceAge_++;

    // Retrigger phases so the new note starts with a clean attack transient
    v.carrierPhase = 0.f;
    v.modPhase     = 0.f;
    v.lfo1Phase    = 0.f;
    v.lfo2Phase    = 0.f;
    v.prevSample   = 0.f;
  }

  inline void NoteOff(uint8_t note) {
    // Release every voice currently holding this note (handles retrigger edge case)
    for (uint8_t i = 0; i < k_num_voices; ++i) {
      if (voices_[i].note == note && voices_[i].gateOn)
        voices_[i].gateOn = false;
    }
  }

  inline void GateOn(uint8_t velocity) {
    // Hardware gate trigger uses the most recently played MIDI note
    NoteOn(lastNote_, velocity);
  }

  inline void GateOff() {
    NoteOff(lastNote_);
  }

  inline void AllNoteOff() {
    for (uint8_t i = 0; i < k_num_voices; ++i)
      voices_[i].gateOn = false;
  }

  inline void PitchBend(uint16_t bend) {
    // Bend range: ±2 semitones (standard)
    const float norm      = (static_cast<float>(bend) - 8192.f) * (1.f / 8192.f);
    const float semitones = clampf(norm, -1.f, 1.f) * 2.f;
    pitchBendMul_ = std::pow(2.f, semitones * (1.f / 12.f));
  }

  inline void ChannelPressure(uint8_t pressure) {
    pressureMod_ = pressure * (1.f / 127.f);
  }

  inline void Aftertouch(uint8_t, uint8_t aftertouch) {
    pressureMod_ = aftertouch * (1.f / 127.f);
  }

  inline void SetTempo(float tempoBpm) { tempoBpm_ = tempoBpm; }

  // ---- Presets -------------------------------------------------------------

  inline void LoadPreset(uint8_t idx) {
    if (idx >= k_num_presets)
      idx = 0;
    const Preset &preset = presets()[idx];
    for (uint8_t i = 0; i < k_num_params; ++i)
      setParameter(i, preset.values[i]);
    presetIndex_ = idx;
  }

  inline uint8_t getPresetIndex() const { return presetIndex_; }

  static inline const char *getPresetName(uint8_t idx) {
    if (idx >= k_num_presets)
      return nullptr;
    return presets()[idx].name;
  }

 private:

  // ---- Preset table --------------------------------------------------------

  struct Preset {
    const char *name;
    // 14 values: [fmDep, hLfo, lfo1, lfo2, fold, fmTune, pitch, fdbk,
    //             lfo3Tgt, lfo3Rate, fmSprd, foldSprd, fdbkSprd, tuneSprd]
    int32_t values[k_num_params];
  };

  static inline const Preset *presets() {
    static const Preset k_presets[k_num_presets] = {
      // Spread params all zero in Init — behaves identically to monophonic original
      {"Init",    {  0,   0, 10, 20,  0,  0,  0,  0, 0, 10,  0,  0,  0,  0}},
      // Presets with spread values chosen to accent each preset's character:
      {"HyperFM", {700, 480, 40, 32, 22, 18,  0,  8, 1, 55, 20,  5,  5,  8}},
      {"Glass",   {560, 260, 22, 35, 45, 30,  8, 16, 5, 48, 10, 15,  3, 12}},
      {"AcidFM",  {850, 120, 55, 48, 18, 36, 12, 24, 8, 62, 30,  5,  8,  5}},
      {"WashPad", {340, 780,  6,  8, 12, 20,  0,  5, 2, 38, 15, 20, 10, 20}},
      {"Pluck",   {500, 150, 28, 42, 28, 15,  4, 12, 7, 52, 10,  8,  5, 10}},
      {"Drone",   {430, 900,  3,  5, 35, 44, 18, 20, 4, 25, 25, 25, 15, 30}},
      {"Chaos",   {980, 650, 65, 61, 72, 62, 25, 34, 3, 70, 50, 40, 20, 25}},
    };
    return k_presets;
  }

  // ---- DSP constants -------------------------------------------------------

  static constexpr float k_sample_rate_hz    = 48000.f;
  static constexpr float k_sample_rate_recip = 1.f / k_sample_rate_hz;
  static constexpr float k_two_pi            = 6.2831853071795864769f;
  static constexpr float k_fm_depth_scale    = 2.f;
  static constexpr float k_amp_attack        = 0.08f;   // one-pole attack coefficient
  static constexpr float k_amp_release       = 0.0025f; // one-pole release coefficient

  // ---- Utility helpers -----------------------------------------------------

  static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  static inline float midiNoteToW0(float note) {
    // Convert MIDI note number to normalized frequency (Hz / sampleRate)
    return 440.f * std::pow(2.f, (note - 69.f) * (1.f / 12.f)) * k_sample_rate_recip;
  }

  static inline float velocityToAmp(uint8_t velocity) {
    // Map velocity 0-127 to amplitude 0.15-1.0 (minimum touch still audible)
    return 0.15f + 0.85f * (velocity * (1.f / 127.f));
  }

  // Apply LFO3 scene-level modulation to the shared parameter set.
  // Results are passed to each voice per sample as the "scene" baseline.
  static inline void applyLfo3Modulation(uint8_t target, float mod,
                                          float &fmDepth,    float &hyperDepth,
                                          float &hyperRate1, float &hyperRate2,
                                          float &fold,       float &modTune,
                                          float &oscTune,    float &feedback) {
    switch (target) {
      case k_lfo3_target_fm_depth:
        fmDepth    = clampf(fmDepth    + mod * 350.f, 0.f, 2046.f); break;
      case k_lfo3_target_hyper_depth:
        hyperDepth = clampf(hyperDepth + mod * 0.25f,  0.f, 1.f);   break;
      case k_lfo3_target_hyper_rate1:
        hyperRate1 = clampf(hyperRate1 + mod * 2.f,    0.f, 10.f);  break;
      case k_lfo3_target_hyper_rate2:
        hyperRate2 = clampf(hyperRate2 + mod * 2.f,    0.f, 10.f);  break;
      case k_lfo3_target_fold:
        fold       = clampf(fold       + mod * 2.f,    0.f, 10.f);  break;
      case k_lfo3_target_mod_tune:
        modTune    = clampf(modTune    + mod * 20.f,   0.f, 100.f); break;
      case k_lfo3_target_osc_tune:
        oscTune    = clampf(oscTune    + mod * 20.f,   0.f, 100.f); break;
      case k_lfo3_target_feedback:
        feedback   = clampf(feedback   + mod * 0.2f,   0.f, 2.f);   break;
      default: break;
    }
  }

  // ---- Instance state ------------------------------------------------------

  Voice   voices_[k_num_voices]; // 4 independent voice slots
  Params  params_;                // Global parameter state (shared across voices)

  float   lfo3Phase_;    // Global LFO3 phase accumulator (0-1)
  float   pitchBendMul_; // Frequency multiplier from MIDI pitch bend
  float   pressureMod_;  // Channel pressure / aftertouch modulation depth (0-1)
  float   tempoBpm_;     // Host tempo (available for tempo-sync features)
  uint8_t presetIndex_;  // Currently loaded preset index
  uint8_t lastNote_;     // Most recent MIDI note number (for GateOn)
  uint32_t voiceAge_;    // Monotonic counter incremented on every NoteOn

  int32_t rawParams_[k_num_params]; // Raw integer values as seen by the host
};
