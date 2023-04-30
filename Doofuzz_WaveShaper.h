#pragma once

#include  "Doofuzz_Common.h"
#include  "iir1/Iir.h"
#include  "iir1/iir/Butterworth.h"

using namespace Doofuzz_Common;

class WaveShaperDoofuzz {
  public:

  inline double reset(const double _sampleRate) {
    for (int i = 0; i < kNumEnvFollowers; i++) {
      envelopeFollower[i].setup(_sampleRate, envFollowerFreq[i]);
    }
    return _sampleRate;
  }

  inline double setRip(const double _rip) {
    return m_Rip = _rip;
  }

  inline double processAudioSample(double _sample) {

    double sample2 = _sample * _sample;

    // Calculate minimum envelope level:
    double env = envelopeFollower[0].filter(sample2);
    for (int i = 1; i < kNumEnvFollowers; i++) {
      env = std::min(env, envelopeFollower[i].filter(sample2));
    }

    _sample += m_Rip * sqrt(2.0 * kRippingAmount * env);

    return tanh(_sample * (1.0 + _sample * _sample / 3.0));
  }

private:

  static const inline double  kRippingAmount    = 1.25;
  static const inline int     kNumEnvFollowers  = 4;

  double envFollowerFreq[kNumEnvFollowers] = {
    40.0,
    // 80.0,
    160.0,
    // 320.0,
    640.0,
    // 1280.0,
    2560.0,
    // 5120.0,
  };

  double                        m_SampleRate  = 48000.0;
  double                        m_Rip         =     0.5;
  Iir::Butterworth::LowPass<1>  envelopeFollower[kNumEnvFollowers];

};
