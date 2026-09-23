#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "unit.h"

// ============================================================================
//  Lirah-4 — four-voice FM+wavefold synthesizer for drumlogue
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
  // 10 oscillator controls + LFO3 depth + 4 spread controls + 4 envelope controls
  static constexpr uint8_t k_num_params  = 19;
  static constexpr uint8_t k_num_presets = 8;

  // Output is divided by voice count to keep full-chord level consistent
  static constexpr float k_voice_gain = 0.25f;

  // ---- Parameter index enumeration ----------------------------------------

  enum ParameterId : uint8_t {
    // ---- Core oscillator parameters ----------------------------------------
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
    k_param_lfo3_depth,         // LFO3 modulation depth (0-100)
    // ---- Voice spread parameters -------------------------------------------
    k_param_fm_spread,          // FM depth spread across voices (0-100)
    k_param_fold_spread,        // Wavefold spread across voices (0-100)
    k_param_feedback_spread,    // Feedback spread across voices (0-100)
    k_param_tune_spread,        // Pitch detune spread in semitones (0-100)
    // ---- Envelope parameters -----------------------------------------------
    k_param_env_type,           // Envelope mode: AR / ADSR / AHR
    k_param_env_speed,          // Envelope speed range: FAST / MED / SLOW
    k_param_env_attack,         // Envelope attack amount (0-100)
    k_param_env_release,        // Envelope release amount (0-100)
  };

  enum EnvelopeType : uint8_t {
    k_env_type_ar = 0,   // Attack then Release (simple synth envelope)
    k_env_type_adsr,     // Attack, Decay, Sustain, Release
    k_env_type_ahr,      // Attack, Hold, Release
    k_env_type_loop_ar,  // Repeats Attack->Release while gate is held
    k_env_type_open,     // Envelope bypass; voices remain fully open
  };

  enum EnvelopeSpeed : uint8_t {
    k_env_speed_fast = 0,
    k_env_speed_med,
    k_env_speed_slow,
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
    float lfo3Depth;        // LFO3 modulation depth (0-1)
    // Per-voice spread amounts — the physical offset applied per voice step.
    // Voice positions are -1.5, -0.5, +0.5, +1.5 so the mean is always 0.
    float fmSpread;         // FM depth delta per voice step (native units)
    float foldSpread;       // Wavefold delta per voice step
    float feedbackSpread;   // Feedback delta per voice step
    float tuneSpread;       // Detune in semitones per voice step
    uint8_t envType;        // Envelope type selector
    uint8_t envSpeed;       // Envelope speed range selector
    float envAttackNorm;    // Raw attack control normalized 0-1
    float envReleaseNorm;   // Raw release control normalized 0-1
    float envAttackSec;     // Attack duration in seconds (speed-scaled)
    float envReleaseSec;    // Release duration in seconds (speed-scaled)

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
      lfo3Depth      = 0.f;
      fmSpread       = 0.f;
      foldSpread     = 0.f;
      feedbackSpread = 0.f;
      tuneSpread     = 0.f;
      envType        = k_env_type_ar;
      envSpeed       = k_env_speed_med;
      envAttackNorm  = 0.2f;
      envReleaseNorm = 0.35f;
      envAttackSec   = 0.03f;
      envReleaseSec  = 0.2f;
    }
  };

  // ---- Per-voice oscillator and envelope state -----------------------------
  //
  // Each voice owns independent phase accumulators for its carrier, modulator,
  // and both hyper-LFOs.  This means held chords have independent hyper-gate
  // rhythms, producing the chorusing / "washy" effect when voices diverge.

  struct Voice {
    enum EnvStage : uint8_t {
      k_env_stage_off = 0,
      k_env_stage_attack,
      k_env_stage_decay,
      k_env_stage_hold,
      k_env_stage_sustain,
      k_env_stage_release,
    };

    float carrierPhase;   // FM carrier phase accumulator (0-1)
    float modPhase;       // FM modulator phase accumulator (0-1)
    float lfo1Phase;      // Hyper-LFO 1 phase accumulator (0-1)
    float lfo2Phase;      // Hyper-LFO 2 phase accumulator (0-1)
    float prevSample;     // Last output sample, used for feedback path
    float baseW0;         // Normalized note frequency: Hz / sampleRate
    float velocityAmp;    // Amplitude scaled from MIDI velocity
    float aftertouchMod;  // Per-note aftertouch amount (0-1)
    float ampEnv;         // Running amplitude envelope (0-velocityAmp)
    float envStageStart;  // Envelope value at stage entry
    uint32_t envStagePos; // Number of samples elapsed in current stage
    uint32_t envStageDur; // Duration in samples for the current stage
    bool  gateOn;         // True while MIDI note is held
    uint8_t envStage;     // Current envelope stage
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
      aftertouchMod = 0.f;
      ampEnv       = 0.f;
      envStageStart = 0.f;
      envStagePos   = 0;
      envStageDur   = 0;
      gateOn       = false;
      envStage     = k_env_stage_off;
      note         = 0xFF; // sentinel: slot is unoccupied
      age          = 0;
    }
  };

  // ---- Lifecycle -----------------------------------------------------------

  Synth(void) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    if (desc->samplerate != k_sample_rate_hz)
      return k_unit_err_samplerate;
    if (desc->output_channels != 2)
      return k_unit_err_geometry;

    presetIndex_ = 0U;
    Reset();
    return k_unit_err_none;
  }

  inline void Teardown() {}

  inline void Reset() {
    params_.reset();

    // Clear raw parameter storage before assigning defaults below.
    for (uint8_t i = 0; i < k_num_params; ++i)
      rawParams_[i] = 0;
    rawParams_[k_param_lfo1_rate] = 10;
    rawParams_[k_param_lfo2_rate] = 20;
    rawParams_[k_param_lfo3_rate] = 10;
    rawParams_[k_param_lfo3_depth] = 0;
    rawParams_[k_param_env_type] = k_env_type_ar;
    rawParams_[k_param_env_speed] = k_env_speed_med;
    rawParams_[k_param_env_attack] = 20;
    rawParams_[k_param_env_release] = 35;

    // Reset all voice slots to inactive
    for (uint8_t i = 0; i < k_num_voices; ++i)
      voices_[i].reset();

    lfo3Phase_    = 0.f;
    pitchBendMul_ = 1.f;
    channelPressureMod_ = 0.f;
    tempoBpm_     = 120.f;
    voiceAge_     = 0;
    lastNote_     = 60; // default to middle C for gate-only triggers
    openSeedVoice_ = false;
  }

  inline void Resume()  {}
  inline void Suspend() {}

  // ---- Render --------------------------------------------------------------

  // renderVoiceSample — advances one voice by exactly one audio sample.
  //
  // Parameters:
  //   v          — voice state (modified in place)
  //   baseW0     — per-voice detuned normalized frequency computed per block
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

  inline float renderVoiceSample(Voice & v, float baseW0,
                                  float fmDepth,  float hyperDepth,
                                  float hyperRate1, float hyperRate2,
                                  float fold,     float modTune,
                                  float oscTune,  float feedback,
                                  const Params & p) {
    // Advance this voice's hyper-LFOs independently so held chords diverge
    v.lfo1Phase = wrapPhase(v.lfo1Phase + hyperRate1 * k_sample_rate_recip);
    v.lfo2Phase = wrapPhase(v.lfo2Phase + hyperRate2 * k_sample_rate_recip);

    // Hyper gate: both LFOs positive at the same time = 3x frequency boost burst
    const float hyperGate =
        (v.lfo1Phase < 0.5f && v.lfo2Phase < 0.5f) ? 3.f : 0.f;
    const float hyperMod  = 1.f + hyperGate * hyperDepth;

    // Channel pressure affects every voice. Per-note aftertouch affects only
    // its matching voice; whichever source is stronger sets the FM response.
    const float pressure = channelPressureMod_ > v.aftertouchMod
                               ? channelPressureMod_
                               : v.aftertouchMod;
    const float fmScale = fmDepth * (0.5f + 0.5f * pressure);

    const float modFreqMul = 1.f + modTune * 0.01f;
    const float oscFreqMul = 1.f + oscTune * 0.01f;
    const float w0 = baseW0 * pitchBendMul_;

    // FM modulator: produces the modulating sine for the carrier's phase
    const float fmSig = std::sin(k_two_pi * v.modPhase);
    v.modPhase = wrapPhase(v.modPhase + w0 * modFreqMul * hyperMod);

    // FM carrier: pitch is shifted by the modulator signal each sample
    const float carrW0  = w0 * oscFreqMul * hyperMod + fmSig * fmScale * k_sample_rate_recip;
    const float carrier = 0.5f * std::sin(k_two_pi * v.carrierPhase);
    v.carrierPhase = wrapPhase(v.carrierPhase + carrW0);

    // Wavefold with per-voice feedback path
    const float foldDrive   = 1.f + fold;
    const float feedbackMix = 1.f + v.prevSample * feedback;
    const float mainOsc     = carrier * foldDrive * feedbackMix;

    // Repeated triangle folding keeps extreme fold/feedback settings bounded.
    const float folded = triangleFold(mainOsc);

    v.prevSample = clampf(mainOsc, -1.f, 1.f);

    updateEnvelope(v, p);

    return folded * v.ampEnv;
  }

  inline void Render(float * out, size_t frames) {
    // Snapshot params once per block to avoid mid-block parameter tearing
    const Params p      = params_;
    const float lfo3W0  = p.lfo3Rate * k_sample_rate_recip;

    // Note callbacks cannot occur during this render call, so active-voice
    // layout and tune-spread ratios only need to be calculated once per block.
    uint8_t activeVoices[k_num_voices];
    float spreadPositions[k_num_voices];
    float tuneMultipliers[k_num_voices];
    uint8_t activeCount = 0U;
    for (uint8_t vi = 0U; vi < k_num_voices; ++vi) {
      if (voices_[vi].envStage == Voice::k_env_stage_off)
        continue;

      activeVoices[activeCount++] = vi;
    }

    for (uint8_t slot = 0U; slot < activeCount; ++slot) {
      const float spreadPosition =
          static_cast<float>(slot) -
          0.5f * (static_cast<float>(activeCount) - 1.f);
      spreadPositions[slot] = spreadPosition;
      tuneMultipliers[slot] =
          std::pow(2.f, p.tuneSpread * spreadPosition * (1.f / 12.f));
    }

    float * out_p = out;
    for (size_t i = 0; i < frames; ++i, out_p += 2) {
      // Advance the global LFO3 once per sample (shared scene modulator)
      lfo3Phase_ = wrapPhase(lfo3Phase_ + lfo3W0);
      const float lfo3 =
          (p.lfo3Target != k_lfo3_target_off && p.lfo3Depth > 0.f)
              ? std::sin(k_two_pi * lfo3Phase_) * p.lfo3Depth
              : 0.f;

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
      for (uint8_t slot = 0U; slot < activeCount; ++slot) {
        const uint8_t vi = activeVoices[slot];
        Voice & v = voices_[vi];

        if (v.envStage == Voice::k_env_stage_off)
          continue;

        // Active voices are always laid out around 0:
        // N=1: {0}, N=2: {-0.5,+0.5}, N=3: {-1,0,+1}, N=4: {-1.5,-0.5,+0.5,+1.5}
        const float spreadPos = spreadPositions[slot];

        const float fmDepth  = clampf(sceneFmDepth   + p.fmSpread       * spreadPos, 0.f, 2046.f);
        const float fold     = clampf(sceneFold       + p.foldSpread     * spreadPos, 0.f, 10.f);
        const float feedback = clampf(sceneFeedback   + p.feedbackSpread * spreadPos, 0.f, 2.f);

        // Apply centered tune spread for this active-voice slot.
        const float voiceBaseW0 = v.baseW0 * tuneMultipliers[slot];

        mix += renderVoiceSample(v, voiceBaseW0, fmDepth, sceneHyperDepth,
                                 sceneHyperRate1, sceneHyperRate2,
                                 fold, sceneModTune, sceneOscTune, feedback, p);
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
      case k_param_lfo3_depth:
        params_.lfo3Depth = clampf(value * 0.01f, 0.f, 1.f);
        break;

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

      case k_param_env_type:
        if (value < k_env_type_ar)     value = k_env_type_ar;
        if (value > k_env_type_open)   value = k_env_type_open;
        {
          const uint8_t previousType = params_.envType;
          params_.envType = static_cast<uint8_t>(value);
          if (params_.envType == k_env_type_open && previousType != k_env_type_open)
            enterOpenMode();
          else if (previousType == k_env_type_open && params_.envType != k_env_type_open)
            leaveOpenMode();
        }
        break;
      case k_param_env_speed:
        if (value < k_env_speed_fast)  value = k_env_speed_fast;
        if (value > k_env_speed_slow)  value = k_env_speed_slow;
        params_.envSpeed = static_cast<uint8_t>(value);
        updateEnvelopeTimes();
        break;
      case k_param_env_attack:
        params_.envAttackNorm = clampf(value * 0.01f, 0.f, 1.f);
        updateEnvelopeTimes();
        break;
      case k_param_env_release:
        params_.envReleaseNorm = clampf(value * 0.01f, 0.f, 1.f);
        updateEnvelopeTimes();
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

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    static const char * targetNames[] = {"OFF",   "FMDEP", "HDEP", "HR1", "HR2",
                                        "FOLD",  "FMTUN", "OTUN", "FDBK"};
    static const char * envTypeNames[] = {"AR", "ADSR", "AHR", "LOOP", "OPEN"};
    static const char * envSpeedNames[] = {"FAST", "MED", "SLOW"};

    if (index == k_param_lfo3_target) {
      if (value < k_lfo3_target_off)       value = k_lfo3_target_off;
      if (value > k_lfo3_target_feedback)  value = k_lfo3_target_feedback;
      return targetNames[value];
    }

    if (index == k_param_env_type) {
      if (value < k_env_type_ar)    value = k_env_type_ar;
      if (value > k_env_type_open)  value = k_env_type_open;
      return envTypeNames[value];
    }

    if (index == k_param_env_speed) {
      if (value < k_env_speed_fast) value = k_env_speed_fast;
      if (value > k_env_speed_slow) value = k_env_speed_slow;
      return envSpeedNames[value];
    }

    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t, int32_t) const { return nullptr; }

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
      const Voice & v = voices_[i];
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
    if (velocity == 0U) {
      NoteOff(note);
      return;
    }

    if (note > 127U)
      note = 127U;

    lastNote_ = note;
    // Replace the automatic OPEN-mode seed instead of layering the first
    // played note over its temporary middle-C pitch.
    const uint8_t vi = (params_.envType == k_env_type_open && openSeedVoice_)
                           ? 0U
                           : allocVoice();
    openSeedVoice_ = false;
    Voice & v = voices_[vi];

    v.note        = note;
    v.baseW0      = midiNoteToW0(static_cast<float>(note));
    v.velocityAmp = params_.envType == k_env_type_open ? 1.f : velocityToAmp(velocity);
    v.aftertouchMod = 0.f;
    v.gateOn      = true;
    v.age         = voiceAge_++;

    // Retrigger phases so the triggered note starts with a clean attack transient.
    v.carrierPhase = 0.f;
    v.modPhase     = 0.f;
    v.lfo1Phase    = 0.f;
    v.lfo2Phase    = 0.f;
    v.prevSample   = 0.f;
    if (params_.envType == k_env_type_open) {
      v.ampEnv = 1.f;
      v.envStageStart = 1.f;
      v.envStagePos = 0U;
      v.envStageDur = 0U;
      v.envStage = Voice::k_env_stage_sustain;
    } else {
      startEnvelopeStage(v, Voice::k_env_stage_attack, params_.envAttackSec);
    }
  }

  inline void NoteOff(uint8_t note) {
    // OPEN is a VCA bypass. Notes set pitch, but note-off does not close it.
    if (params_.envType == k_env_type_open)
      return;

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
    if (params_.envType == k_env_type_open) {
      // Preserve an explicit host panic as the way to silence an open drone.
      for (uint8_t i = 0; i < k_num_voices; ++i)
        voices_[i].reset();
      openSeedVoice_ = false;
      return;
    }

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
    if (pressure > 127U)
      pressure = 127U;
    channelPressureMod_ = pressure * (1.f / 127.f);
  }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    if (aftertouch > 127U)
      aftertouch = 127U;

    const float amount = aftertouch * (1.f / 127.f);
    for (uint8_t i = 0U; i < k_num_voices; ++i) {
      Voice & v = voices_[i];
      if (v.note == note && v.envStage != Voice::k_env_stage_off)
        v.aftertouchMod = amount;
    }
  }

  inline void SetTempo(float tempoBpm) { tempoBpm_ = tempoBpm; }

  // ---- Presets -------------------------------------------------------------

  inline void LoadPreset(uint8_t idx) {
    if (idx >= k_num_presets)
      idx = 0;
    const Preset & preset = presets()[idx];
    for (uint8_t i = 0; i < k_num_params; ++i)
      setParameter(i, preset.values[i]);
    presetIndex_ = idx;
  }

  inline uint8_t getPresetIndex() const { return presetIndex_; }

  static inline const char * getPresetName(uint8_t idx) {
    if (idx >= k_num_presets)
      return nullptr;
    return presets()[idx].name;
  }

 private:

  // ---- Preset table --------------------------------------------------------

  struct Preset {
    const char * name;
    // 19 values: [fmDep, hLfo, lfo1, lfo2, fold, fmTune, pitch, fdbk,
    //             lfo3Tgt, lfo3Rate, lfo3Depth,
    //             fmSprd, foldSprd, fdbkSprd, tuneSprd,
    //             envType, envSpd, atk, rel]
    int32_t values[k_num_params];
  };

  static inline const Preset * presets() {
    static const Preset k_presets[k_num_presets] = {
      // Init uses zero voice spread for a centered unison state.
      {"Init",    {  0,   0, 10, 20,  0,  0,  0,  0, 0, 10,  0,  0,  0,  0,  0, 0, 1, 20, 35}},
      // Presets with spread values chosen to accent each preset's character:
      {"HyperFM", {700, 480, 40, 32, 22, 18,  0,  8, 1, 55, 100, 20,  5,  5,  8, 0, 0, 15, 18}},
      {"Glass",   {560, 260, 22, 35, 45, 30,  8, 16, 5, 48, 100, 10, 15,  3, 12, 1, 1, 22, 32}},
      {"AcidFM",  {850, 120, 55, 48, 18, 36, 12, 24, 8, 62, 100, 30,  5,  8,  5, 0, 0,  8, 12}},
      {"WashPad", {340, 780,  6,  8, 12, 20,  0,  5, 2, 38, 100, 15, 20, 10, 20, 1, 2, 40, 70}},
      {"Pluck",   {500, 150, 28, 42, 28, 15,  4, 12, 7, 52, 100, 10,  8,  5, 10, 2, 1, 10, 24}},
      {"Drone",   {430, 900,  3,  5, 35, 44, 18, 20, 4, 25, 100, 25, 25, 15, 30, 2, 2, 55, 65}},
      {"Chaos",   {980, 650, 65, 61, 72, 62, 25, 34, 3, 70, 100, 50, 40, 20, 25, 0, 1, 18, 22}},
    };
    return k_presets;
  }

  // ---- DSP constants -------------------------------------------------------

  static constexpr float k_sample_rate_hz    = 48000.f;
  static constexpr float k_sample_rate_recip = 1.f / k_sample_rate_hz;
  static constexpr float k_two_pi            = 6.2831853071795864769f;
  static constexpr float k_fm_depth_scale    = 2.f;
  static constexpr float k_env_sustain       = 0.68f;

  // ---- Utility helpers -----------------------------------------------------

  static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  static inline float wrapPhase(float phase) {
    phase -= static_cast<int32_t>(phase);
    return phase < 0.f ? phase + 1.f : phase;
  }

  static inline float triangleFold(float sample) {
    const float cycles = (sample + 0.5f) * 0.5f;
    int32_t cycle = static_cast<int32_t>(cycles);
    if (static_cast<float>(cycle) > cycles)
      --cycle;

    const float wrapped = sample - 2.f * static_cast<float>(cycle);
    return wrapped > 0.5f ? 1.f - wrapped : wrapped;
  }

  static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
  }

  static inline uint32_t secondsToSamples(float sec) {
    const float clampedSec = sec < (1.f / k_sample_rate_hz) ? (1.f / k_sample_rate_hz) : sec;
    return static_cast<uint32_t>(clampedSec * k_sample_rate_hz);
  }

  // Maps 0..1 knob input into time in seconds using exponential scaling.
  // Exponential mapping keeps the short-time area usable and musical.
  static inline float mapEnvTimeSeconds(uint8_t speed, float norm, bool isAttack) {
    float minSec = 0.001f;
    float maxSec = 0.1f;

    if (isAttack) {
      if (speed == k_env_speed_fast) {
        // Extra-snappy range for transient-heavy sounds.
        minSec = 0.0003f;
        maxSec = 0.10f;
      } else if (speed == k_env_speed_med) {
        minSec = 0.003f;
        maxSec = 0.60f;
      } else {
        minSec = 0.02f;
        maxSec = 4.5f;
      }
    } else {
      if (speed == k_env_speed_fast) {
        minSec = 0.0015f;
        maxSec = 0.50f;
      } else if (speed == k_env_speed_med) {
        minSec = 0.015f;
        maxSec = 2.0f;
      } else {
        minSec = 0.08f;
        maxSec = 8.0f;
      }
    }

    const float t = clampf(norm, 0.f, 1.f);
    return minSec * std::pow(maxSec / minSec, t);
  }

  inline void updateEnvelopeTimes() {
    // Recompute absolute times whenever ENV RANGE / ATTACK / RELEASE changes.
    params_.envAttackSec = mapEnvTimeSeconds(params_.envSpeed, params_.envAttackNorm, true);
    params_.envReleaseSec = mapEnvTimeSeconds(params_.envSpeed, params_.envReleaseNorm, false);
  }

  static inline void startEnvelopeStage(Voice & v, uint8_t stage, float durationSec) {
    v.envStage = stage;
    v.envStageStart = v.ampEnv;
    v.envStagePos = 0;
    v.envStageDur = secondsToSamples(durationSec);
  }

  inline void enterOpenMode() {
    bool hasVoice = false;
    for (uint8_t i = 0; i < k_num_voices; ++i) {
      Voice & v = voices_[i];
      if (v.note == 0xFF)
        continue;

      hasVoice = true;
      v.velocityAmp = 1.f;
      v.ampEnv = 1.f;
      v.envStageStart = 1.f;
      v.envStagePos = 0U;
      v.envStageDur = 0U;
      v.gateOn = true;
      v.envStage = Voice::k_env_stage_sustain;
    }

    if (hasVoice) {
      openSeedVoice_ = false;
      return;
    }

    // OPEN must make sound without waiting for another trigger. Start one
    // full-level voice at the most recently received pitch (middle C at boot).
    Voice & v = voices_[0];
    v.reset();
    v.note = lastNote_;
    v.baseW0 = midiNoteToW0(static_cast<float>(lastNote_));
    v.velocityAmp = 1.f;
    v.ampEnv = 1.f;
    v.envStageStart = 1.f;
    v.gateOn = true;
    v.envStage = Voice::k_env_stage_sustain;
    v.age = voiceAge_++;
    openSeedVoice_ = true;
  }

  inline void leaveOpenMode() {
    // Return persistent OPEN voices to the normal envelope lifecycle.
    openSeedVoice_ = false;
    for (uint8_t i = 0; i < k_num_voices; ++i) {
      Voice & v = voices_[i];
      if (v.note == 0xFF)
        continue;
      v.gateOn = false;
      startEnvelopeStage(v, Voice::k_env_stage_release, params_.envReleaseSec);
    }
  }

  inline void updateEnvelope(Voice & v, const Params & p) {
    // OPEN mode bypasses the normal note-controlled envelope lifecycle.
    if (p.envType != k_env_type_open && !v.gateOn && v.envStage != Voice::k_env_stage_release && v.envStage != Voice::k_env_stage_off)
      startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);

    // Move stages that do not exist in the selected envelope mode to a
    // valid stage. This keeps live mode changes and preset changes responsive.
    if (v.gateOn) {
      if (p.envType == k_env_type_ar &&
          (v.envStage == Voice::k_env_stage_decay ||
           v.envStage == Voice::k_env_stage_hold)) {
        v.envStage = Voice::k_env_stage_sustain;
      } else if (p.envType == k_env_type_adsr &&
                 v.envStage == Voice::k_env_stage_hold) {
        v.envStage = Voice::k_env_stage_sustain;
      } else if (p.envType == k_env_type_ahr &&
                 (v.envStage == Voice::k_env_stage_decay ||
                  v.envStage == Voice::k_env_stage_sustain)) {
        const float holdSec = clampf(p.envAttackSec * 0.5f, 0.004f, 1.2f);
        startEnvelopeStage(v, Voice::k_env_stage_hold, holdSec);
      } else if (p.envType == k_env_type_loop_ar &&
                 (v.envStage == Voice::k_env_stage_decay ||
                  v.envStage == Voice::k_env_stage_hold ||
                  v.envStage == Voice::k_env_stage_sustain)) {
        startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
      }
    }

    switch (p.envType) {
      case k_env_type_ar:
        if (v.envStage == Voice::k_env_stage_attack) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, v.velocityAmp, t);
          if (++v.envStagePos >= v.envStageDur) {
            if (v.gateOn) {
              v.envStage = Voice::k_env_stage_sustain;
            } else {
              startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
            }
          }
        } else if (v.envStage == Voice::k_env_stage_sustain) {
          v.ampEnv = v.velocityAmp;
          if (!v.gateOn)
            startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
        } else if (v.envStage == Voice::k_env_stage_release) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, 0.f, t);
          if (++v.envStagePos >= v.envStageDur)
            v.envStage = Voice::k_env_stage_off;
        }
        break;

      case k_env_type_adsr: {
        const float decaySec = clampf(p.envAttackSec * 0.75f, 0.006f, 1.8f);
        const float sustainLevel = v.velocityAmp * k_env_sustain;

        if (v.envStage == Voice::k_env_stage_attack) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, v.velocityAmp, t);
          if (++v.envStagePos >= v.envStageDur)
            startEnvelopeStage(v, Voice::k_env_stage_decay, decaySec);
        } else if (v.envStage == Voice::k_env_stage_decay) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, sustainLevel, t);
          if (++v.envStagePos >= v.envStageDur)
            v.envStage = Voice::k_env_stage_sustain;
        } else if (v.envStage == Voice::k_env_stage_sustain) {
          v.ampEnv = sustainLevel;
          if (!v.gateOn)
            startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
        } else if (v.envStage == Voice::k_env_stage_release) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, 0.f, t);
          if (++v.envStagePos >= v.envStageDur)
            v.envStage = Voice::k_env_stage_off;
        }
      } break;

      case k_env_type_ahr: {
        const float holdSec = clampf(p.envAttackSec * 0.5f, 0.004f, 1.2f);

        if (v.envStage == Voice::k_env_stage_attack) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, v.velocityAmp, t);
          if (++v.envStagePos >= v.envStageDur)
            startEnvelopeStage(v, Voice::k_env_stage_hold, holdSec);
        } else if (v.envStage == Voice::k_env_stage_hold) {
          v.ampEnv = v.velocityAmp;
          if (++v.envStagePos >= v.envStageDur || !v.gateOn)
            startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
        } else if (v.envStage == Voice::k_env_stage_release) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, 0.f, t);
          if (++v.envStagePos >= v.envStageDur)
            v.envStage = Voice::k_env_stage_off;
        }
      } break;

      case k_env_type_loop_ar:
        // LOOP AR: while gate is on, keep cycling attack -> release.
        if (v.envStage == Voice::k_env_stage_attack) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, v.velocityAmp, t);
          if (++v.envStagePos >= v.envStageDur)
            startEnvelopeStage(v, Voice::k_env_stage_release, p.envReleaseSec);
        } else if (v.envStage == Voice::k_env_stage_release) {
          const float t = clampf((v.envStagePos + 1.f) / static_cast<float>(v.envStageDur), 0.f, 1.f);
          v.ampEnv = lerp(v.envStageStart, 0.f, t);
          if (++v.envStagePos >= v.envStageDur) {
            if (v.gateOn) {
              startEnvelopeStage(v, Voice::k_env_stage_attack, p.envAttackSec);
            } else {
              v.envStage = Voice::k_env_stage_off;
            }
          }
        }
        break;

      case k_env_type_open:
        v.velocityAmp = 1.f;
        v.ampEnv = 1.f;
        v.gateOn = true;
        v.envStage = Voice::k_env_stage_sustain;
        break;

      default:
        break;
    }

    if (v.envStage == Voice::k_env_stage_off) {
      v.ampEnv = 0.f;
      v.note = 0xFF;
      v.gateOn = false;
    }
  }

  static inline float midiNoteToW0(float note) {
    // Convert MIDI note number to normalized frequency (Hz / sampleRate)
    return 440.f * std::pow(2.f, (note - 69.f) * (1.f / 12.f)) * k_sample_rate_recip;
  }

  static inline float velocityToAmp(uint8_t velocity) {
    // Map velocity 0-127 to amplitude 0.15-1.0 (minimum touch still audible)
    if (velocity > 127U)
      velocity = 127U;
    return 0.15f + 0.85f * (velocity * (1.f / 127.f));
  }

  // Apply LFO3 scene-level modulation to the shared parameter set.
  // Results are passed to each voice per sample as the "scene" baseline.
  static inline void applyLfo3Modulation(uint8_t target, float mod,
                                          float & fmDepth,    float & hyperDepth,
                                          float & hyperRate1, float & hyperRate2,
                                          float & fold,       float & modTune,
                                          float & oscTune,    float & feedback) {
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
  float   channelPressureMod_; // Global channel-pressure modulation depth (0-1)
  float   tempoBpm_;     // Host tempo (available for tempo-sync features)
  uint8_t presetIndex_;  // Currently loaded preset index
  uint8_t lastNote_;     // Most recent MIDI note number (for GateOn)
  uint32_t voiceAge_;    // Monotonic counter incremented on every NoteOn
  bool openSeedVoice_;   // True until the first note replaces OPEN's startup voice

  int32_t rawParams_[k_num_params]; // Raw integer values as seen by the host
};
