#pragma once

// TO DO:
// - Web link
// - mono / stereo / simulated stereo?

#include "IPlug_include_in_plug_hdr.h"
#include "iir1/Iir.h"
#include "Oversampler.h"
#include "Doofuzz_ParamSmoother.h"
#include "Doofuzz_WaveShaper.h"

using namespace iplug;
using namespace igraphics;

const int     kNumPresets       = 1;
const int     kMaxNumChannels   = 2;
const double  kSmoothingTimeMs  = 20.0; // Parameter smoothing in milliseconds

const double  kDCBlockFreq      =    40.0;
const double  kScoopFreq        =   432.0; // Joke...
const double  kScoop_dB         =   -24.0;
const double  kScoopBandwidth   =     2.4;

//const double  kEnvCutoff        =  4320.0;

enum EParams {
  // Main parameters (the big knobs):
  kParamDrive = 0,
  kParamRip,
  kParamTone,
  kParamOutput,
  kParamActive,
  kParamOversampling,
  ///////////////////
  kNumParams
};

char* paramNames[kNumParams] = {
  "Drive",
  "Rip",
  "Tone",
  "Output",
  "Active",
  "Oversampling",
};

char* paramLabels[kNumParams] = {
  "Drive",
  "Rip",
  "Tone",
  "Output",
  "Active",
  "OS",
};

char* paramToolTips[kNumParams] = {
  "Drive (dB):\nControls the distortion level",
  "Rip:\nControls the starvation of the transistors",
  "Tone:\nControls the brightness",
  "Output (dB):\nControls the final output volume",
  "Active:\nSwitches the plugin on or off",
  "Oversampling:\nSwitches between 16x oversampling, or none.\nLack of oversampling will lead to aliasing, especially at higher Drive settings",
};

class VALUES {
public:
  VALUES(double _def, double _min, double _max, double _step) {
    def   = _def;
    min   = _min;
    max   = _max;
    step  = _step;
  };
  VALUES(bool _def) {
    def   = _def;
    min   = false;
    max   = true;
    step  = 1.0;
  };
  double def;
  double min;
  double max;
  double step;
};

VALUES paramValues[kNumParams] = {  // (default, minimum, maximum, step)

  // Main (big) knobs:
  VALUES(  48.0,   0.0,   +96.0, 0.01), // Drive, in dB
  VALUES(   0.5,   0.0,     1.0, 0.01), // Rip
  VALUES(4000.0, 800.0, 20000.0, 0.01), // Tone
  VALUES( -18.0, -54.0,   +18.0, 0.01), // Output, in dB

  // Switches:
  VALUES(true),                         // Active
  VALUES(true),                         // Oversampling
};

IRECT controlCoordinates[kNumParams] = {
  IRECT(60 + 0*84, 100, 123 + 0*84, 215), // Drive
  IRECT(60 + 1*84, 100, 123 + 1*84, 215), // Rip
  IRECT(60 + 2*84, 100, 123 + 2*84, 215), // Tone
  IRECT(60 + 3*84, 100, 123 + 3*84, 215), // Output

  IRECT(60 + 4*84, 100, 123 + 4*84, 150), // Active
  IRECT(60 + 4*84, 165, 123 + 4*84, 215), // Oversampling

 };

/////////////////////////////////////////

class Doofuzz final: public Plugin {
private:

  // Smoothed parameter values ////////////////////////////////////////////////

  // Main knobs:
  double  m_Drive_Real      = DBToAmp(paramValues[kParamDrive       ].def); // Input gain in real terms, from dB
  double  m_Rip             =         paramValues[kParamRip         ].def;
  double  m_Tone            =         paramValues[kParamTone        ].def;
  double  m_Output_Real     = DBToAmp(paramValues[kParamOutput      ].def); // Output gain in real terms, from dB
  double  m_Active          =         paramValues[kParamActive      ].def;  // 0.0..1.0
  double  m_Oversampling    =         paramValues[kParamOversampling].def;

  /////////////////////////////////////////////////////////////////////////////

  ParameterSmoother               smoother = ParameterSmoother(kNumParams);

  // Filters etc:

  Iir::Butterworth::HighPass<1>   m_DCBlockBefore [kMaxNumChannels];
  Iir::Butterworth::HighPass<1>   m_DCBlockAfter  [kMaxNumChannels];

  Iir::RBJ::BandShelf             m_Scoop         [kMaxNumChannels];

  Iir::Butterworth::LowPass<1>    m_HighCut       [kMaxNumChannels];

  WaveShaperDoofuzz          m_Waveshaper[kMaxNumChannels];

  OverSampler<sample>             m_Oversampler [kMaxNumChannels] = {
                                    OverSampler(EFactor::k16x, false),
                                    OverSampler(EFactor::k16x, false),
                                  };

  inline void updateKnobs();
  inline void AdjustOversampling();
  inline void updateStages(bool _resetting);

public:
  Doofuzz(const InstanceInfo& info);
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;

};
