#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "unit.h"

class Synth {
 public:
  enum ParameterId : uint8_t {
    k_param_fm_depth = 0,
    k_param_hyper_lfo_depth,
    k_param_lfo1_rate,
    k_param_lfo2_rate,
    k_param_fold,
    k_param_fm_tune,
    k_param_pitch,
    k_param_feedback,
    k_param_lfo3_target,
    k_param_lfo3_rate,
  };

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

  struct Params {
    float fmDepth;
    float lfoDepth;
    float lfoRate1;
    float lfoRate2;
    float waveFold;
    int32_t modTune;
    int32_t oscTune;
    float feedback;
    uint8_t lfo3Target;
    float lfo3Rate;

    inline void reset() {
      fmDepth = 0.f;
      lfoDepth = 0.f;
      lfoRate1 = 1.f;
      lfoRate2 = 2.f;
      waveFold = 0.f;
      modTune = 0;
      oscTune = 0;
      feedback = 0.f;
      lfo3Target = k_lfo3_target_off;
      lfo3Rate = 0.f;
    }
  };

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
    carrierPhase_ = 0.f;
    modPhase_ = 0.f;
    lfo1Phase_ = 0.f;
    lfo2Phase_ = 0.f;
    lfo3Phase_ = 0.f;
    prevSample_ = 0.f;
    baseW0_ = midiNoteToW0(60.f);
    pitchBendMul_ = 1.f;
    pressureMod_ = 0.f;
    gateOn_ = false;
    activeNote_ = 60;
    velocityAmp_ = 1.f;
    ampEnv_ = 0.f;
    tempoBpm_ = 120.f;
  }

  inline void Resume() {}

  inline void Suspend() {}

  inline void Render(float *out, size_t frames) {
    const Params p = params_;
    const float lfo3W0 = p.lfo3Rate * k_sample_rate_recip;

    float *out_p = out;
    for (size_t i = 0; i < frames; ++i, out_p += 2) {
      float fmDepthNow = p.fmDepth;
      float hyperDepthNow = p.lfoDepth;
      float hyperRate1Now = p.lfoRate1;
      float hyperRate2Now = p.lfoRate2;
      float foldNow = p.waveFold;
      float modTuneNow = static_cast<float>(p.modTune);
      float oscTuneNow = static_cast<float>(p.oscTune);
      float feedbackNow = p.feedback;

      lfo3Phase_ += lfo3W0;
      lfo3Phase_ -= static_cast<uint32_t>(lfo3Phase_);
      const float lfo3 = std::sinf(k_two_pi * lfo3Phase_);

      applyLfo3Modulation(p.lfo3Target, lfo3, fmDepthNow, hyperDepthNow, hyperRate1Now,
                          hyperRate2Now, foldNow, modTuneNow, oscTuneNow, feedbackNow);

      const float lfo1W0Now = hyperRate1Now * k_sample_rate_recip;
      const float lfo2W0Now = hyperRate2Now * k_sample_rate_recip;

      lfo1Phase_ += lfo1W0Now;
      lfo1Phase_ -= static_cast<uint32_t>(lfo1Phase_);

      lfo2Phase_ += lfo2W0Now;
      lfo2Phase_ -= static_cast<uint32_t>(lfo2Phase_);

      const float lfo1Out = std::sinf(k_two_pi * lfo1Phase_);
      const float lfo2Out = std::sinf(k_two_pi * lfo2Phase_);

      const float hyperGate = (lfo1Out >= 0.f && lfo2Out >= 0.f) ? 3.f : 0.f;
      const float hyperMod = 1.f + hyperGate * hyperDepthNow;
      const float fmScale = fmDepthNow * (0.5f + 0.5f * pressureMod_);

      const float modFreqMul = 1.f + modTuneNow * 0.01f;
      const float oscFreqMul = 1.f + oscTuneNow * 0.01f;
      const float w0 = baseW0_ * pitchBendMul_;
      const float modW0 = w0 * modFreqMul * hyperMod;

      const float fmSig = std::sinf(k_two_pi * modPhase_);
      modPhase_ += modW0;
      modPhase_ -= static_cast<uint32_t>(modPhase_);

      const float carrW0 = w0 * oscFreqMul * hyperMod + fmSig * fmScale * k_sample_rate_recip;

      const float carrier = 0.5f * std::sinf(k_two_pi * carrierPhase_);
      const float foldDrive = 1.f + foldNow;
      const float feedbackMix = 1.f + prevSample_ * feedbackNow;
      const float mainOsc = carrier * foldDrive * feedbackMix;

      carrierPhase_ += carrW0;
      carrierPhase_ -= static_cast<uint32_t>(carrierPhase_);

      const float folded = (mainOsc < -0.5f) ? (-1.f - mainOsc)
                           : (mainOsc > 0.5f) ? (1.f - mainOsc)
                                              : mainOsc;

      prevSample_ = clampf(mainOsc, -1.f, 1.f);

      const float targetAmp = gateOn_ ? velocityAmp_ : 0.f;
      const float glide = gateOn_ ? k_amp_attack : k_amp_release;
      ampEnv_ += (targetAmp - ampEnv_) * glide;

      const float outSample = folded * ampEnv_;
      out_p[0] = outSample;
      out_p[1] = outSample;
    }
  }

  inline void setParameter(uint8_t index, int32_t value) {
    switch (index) {
      case k_param_fm_depth:
        params_.fmDepth = value * 2.f;
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
        if (value < k_lfo3_target_off)
          value = k_lfo3_target_off;
        if (value > k_lfo3_target_feedback)
          value = k_lfo3_target_feedback;
        params_.lfo3Target = static_cast<uint8_t>(value);
        break;
      case k_param_lfo3_rate: {
        const float norm = clampf(value * 0.01f, 0.f, 1.f);
        if (norm <= 0.5f) {
          params_.lfo3Rate = norm * 2.f;
        } else {
          params_.lfo3Rate = 1.f + (norm - 0.5f) * 48.f;
        }
      } break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t) const { return 0; }

  inline const char *getParameterStrValue(uint8_t index, int32_t value) const {
    static const char *targetNames[] = {"OFF", "FMDEP", "HDEP", "HR1", "HR2",
                                        "FOLD", "FMTUN", "OTUN", "FDBK"};

    if (index != k_param_lfo3_target)
      return nullptr;

    value = static_cast<int32_t>(clampf(static_cast<float>(value), k_lfo3_target_off,
                                        k_lfo3_target_feedback));
    return targetNames[value];
  }

  inline const uint8_t *getParameterBmpValue(uint8_t, int32_t) const { return nullptr; }

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    activeNote_ = note;
    baseW0_ = midiNoteToW0(static_cast<float>(note));
    velocityAmp_ = 0.15f + 0.85f * (velocity * (1.f / 127.f));
    gateOn_ = true;
  }

  inline void NoteOff(uint8_t note) {
    if (note == activeNote_)
      gateOn_ = false;
  }

  inline void GateOn(uint8_t velocity) {
    velocityAmp_ = 0.15f + 0.85f * (velocity * (1.f / 127.f));
    gateOn_ = true;
  }

  inline void GateOff() { gateOn_ = false; }

  inline void AllNoteOff() { gateOn_ = false; }

  inline void PitchBend(uint16_t bend) {
    const float norm = (static_cast<float>(bend) - 8192.f) * (1.f / 8192.f);
    const float semitones = clampf(norm, -1.f, 1.f) * 2.f;
    pitchBendMul_ = std::pow(2.f, semitones * (1.f / 12.f));
  }

  inline void ChannelPressure(uint8_t pressure) { pressureMod_ = pressure * (1.f / 127.f); }

  inline void Aftertouch(uint8_t, uint8_t aftertouch) { pressureMod_ = aftertouch * (1.f / 127.f); }

  inline void SetTempo(float tempoBpm) { tempoBpm_ = tempoBpm; }

  inline void LoadPreset(uint8_t idx) { (void)idx; }

  inline uint8_t getPresetIndex() const { return 0; }

  static inline const char *getPresetName(uint8_t idx) {
    (void)idx;
    return nullptr;
  }

 private:
  static constexpr float k_sample_rate_hz = 48000.f;
  static constexpr float k_sample_rate_recip = 1.f / k_sample_rate_hz;
  static constexpr float k_two_pi = 6.2831853071795864769f;
  static constexpr float k_amp_attack = 0.08f;
  static constexpr float k_amp_release = 0.0025f;

  static inline float clampf(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
  }

  static inline float midiNoteToW0(float note) {
    const float hz = 440.f * std::pow(2.f, (note - 69.f) * (1.f / 12.f));
    return hz * k_sample_rate_recip;
  }

  static inline void applyLfo3Modulation(uint8_t target, float mod, float &fmDepth,
                                         float &hyperDepth, float &hyperRate1,
                                         float &hyperRate2, float &fold, float &modTune,
                                         float &oscTune, float &feedback) {
    switch (target) {
      case k_lfo3_target_fm_depth:
        fmDepth = clampf(fmDepth + mod * 350.f, 0.f, 2046.f);
        break;
      case k_lfo3_target_hyper_depth:
        hyperDepth = clampf(hyperDepth + mod * 0.25f, 0.f, 1.f);
        break;
      case k_lfo3_target_hyper_rate1:
        hyperRate1 = clampf(hyperRate1 + mod * 2.f, 0.f, 10.f);
        break;
      case k_lfo3_target_hyper_rate2:
        hyperRate2 = clampf(hyperRate2 + mod * 2.f, 0.f, 10.f);
        break;
      case k_lfo3_target_fold:
        fold = clampf(fold + mod * 2.f, 0.f, 10.f);
        break;
      case k_lfo3_target_mod_tune:
        modTune = clampf(modTune + mod * 20.f, 0.f, 100.f);
        break;
      case k_lfo3_target_osc_tune:
        oscTune = clampf(oscTune + mod * 20.f, 0.f, 100.f);
        break;
      case k_lfo3_target_feedback:
        feedback = clampf(feedback + mod * 0.2f, 0.f, 2.f);
        break;
      default:
        break;
    }
  }

  Params params_;
  float carrierPhase_;
  float modPhase_;
  float lfo1Phase_;
  float lfo2Phase_;
  float lfo3Phase_;
  float prevSample_;
  float baseW0_;
  float pitchBendMul_;
  float pressureMod_;
  bool gateOn_;
  uint8_t activeNote_;
  float velocityAmp_;
  float ampEnv_;
  float tempoBpm_;
};
