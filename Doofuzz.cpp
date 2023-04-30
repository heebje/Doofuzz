#include "Doofuzz.h"

#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include "Doofuzz_CornerResizers.h"
#include "Doofuzz_Common.h"

using namespace Doofuzz_Common;

Doofuzz::Doofuzz(const InstanceInfo& info): iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets)) {

  for (int p = 0; p < kNumParams; p++) {

    if (paramNames[p] == NULL) {
      FAIL('Parameter name not definied');
    }

    switch (p) {

      // dBs:
      case kParamDrive:
      case kParamOutput: {
        GetParam(p)->InitGain(paramNames [p],
                              paramValues[p].def,
                              paramValues[p].min,
                              paramValues[p].max,
                              paramValues[p].step);
        break;
      }

      // Double:
      case kParamWidth:
      case kParamRip: {
        GetParam(p)->InitDouble(paramNames [p],
                                paramValues[p].def,
                                paramValues[p].min,
                                paramValues[p].max,
                                paramValues[p].step);
        break;
      }

      // Frequency:
      case kParamTone: {
        GetParam(p)->InitFrequency(paramNames [p],
                                   paramValues[p].def,
                                   paramValues[p].min,
                                   paramValues[p].max,
                                   paramValues[p].step);
        break;
      }

      // Boolean:
      case kParamActive:
      case kParamOversampling: {
        GetParam(p)->InitBool(paramNames [p],
                              paramValues[p].def);
        break;
      }

      /*case kParamOversampling: {
        GetParam(p)->InitEnum(paramNames[p],
                              paramValues[p].def,
                              { NAM_OVERSAMPLING_FACTORS_VA_LIST });
        break;
      }*/

      default: {
        FAIL("Parameter missing");
        break;
      }

    }

  }

  smoother.reset(this, kSmoothingTimeMs);

  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {

    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);

    pGraphics->AttachCornerResizer(new NAMCornerResizer(IRECT(0, 0, PLUG_WIDTH, PLUG_HEIGHT),
                                                        24.0));
    pGraphics->AttachControl(new NAMCornerShrinker(24.0));

    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    pGraphics->AttachControl(new IVLabelControl(IRECT(30, 30, PLUG_WIDTH-30, 80),
                                                PLUG_NAME,
                                                DEFAULT_STYLE
                                                  .WithDrawFrame(false)
                                                  .WithDrawShadows(false)
                                                  .WithValueText(IText(30))));
    
    pGraphics->AttachControl(new IURLControl(IRECT((PLUG_WIDTH - 60.0) / 2.0,
                                                   30.0,
                                                   (PLUG_WIDTH + 60.0) / 2.0,
                                                   80.0),
                                             "",
                                             PLUG_URL_STR));

    for (int p = 0; p < kNumParams; p++) {

      switch(p) {

        // Big knobs: /////////////////////////////////////////////////////////
        case kParamWidth:
        case kParamDrive:
        case kParamRip:
        case kParamTone:
        case kParamOutput: {
          pGraphics->AttachControl(new IVKnobControl(controlCoordinates[p],
                                                     p,
                                                     paramLabels[p],
                                                     DEFAULT_STYLE
                                                       .WithShowLabel(true)
                                                       .WithShowValue(true)))->SetTooltip(paramToolTips[p]);
          break;
        }

        case kParamActive: {
          // On/Off switch:
          pGraphics->AttachControl(new IVToggleControl(controlCoordinates[p],
                                                       p,
                                                       paramLabels[p],
                                                       DEFAULT_STYLE.WithShowLabel(false),
                                                       "off",
                                                       "on"))->SetTooltip(paramToolTips[p]);
          break;
        }

        case kParamOversampling: {
          // Oversampling switch:
          pGraphics->AttachControl(new IVToggleControl(controlCoordinates[p],
                                                       p,
                                                       paramLabels[p],
                                                       DEFAULT_STYLE.WithShowLabel(false),
                                                       "none",
                                                       "16x"))->SetTooltip(paramToolTips[p]);
          break;
        }

        default: {
          FAIL("Parameter missing");
          break;
        }
      }

    }

    updateKnobs();

  };

}

void Doofuzz::OnReset() {

  const double sr = GetSampleRate();

  // To prevent useless
  //   "IAudioProcessor::process (..) generates non silent output for silent input for tail above 0 samples"
  // error.
  //
  SetTailSize((int)(sr + 0.5));

  smoother.reset(this,
                 kSmoothingTimeMs);

  updateStages(true);

}

void Doofuzz::OnParamChange(int paramIdx) {

  /*if (paramIdx == xkParamOversampling) {
    m_Oversampling = EDoofuzzFactor(GetParam(paramIdx)->Value());

    if (GetUI() && m_Controls[paramIdx]) {
      m_Controls[paramIdx]->As<IVButtonControl>()->SetValueStr(OSFactorLabels[m_Oversampling]);
    }

  } else {*/

  // Smoothed parameters:
  smoother.change(paramIdx, GetParam(paramIdx)->Value());

  if ((paramIdx == kParamActive) && GetUI()) {
    // Reflect in knob appearances:
    updateKnobs();
  }

}

void Doofuzz::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {

  const int     nInChans  = std::min(kMaxNumChannels, NInChansConnected());
  const int     nOutChans = std::min(kMaxNumChannels, NOutChansConnected());
  const int     nMaxChans = std::max(nInChans, nOutChans);

  for (int s = 0; s < nFrames; s++) {

    updateStages(false);

    if (m_Active == 0.0) {

      for (int ch = 0; ch < nMaxChans; ch++) {
        outputs[std::min(ch, nOutChans-1)][s] = inputs[std::min(ch, nInChans-1)][s];
      }

    } else {

      // Active: //////////////////////////////////////////////////////////////

      sample x[kMaxNumChannels];  // Scratch area

      // Stereoise first:

      if (nInChans == kMaxNumChannels) {

        assert(nOutChans == kMaxNumChannels);   // Can't have a "2-1" situation, so this MUST be "2-2"
        m_Stereoiser.processFrame(inputs[0][s], inputs[1][s], &x[0], &x[1]);

      } else {

        m_Stereoiser.processFrame(inputs[0][s], inputs[0][s], &x[0], &x[1]);

      }

      ///////////////////////////////////////////////////////////////////////////

      for (int ch = 0; ch < nMaxChans; ch++) {

        x[ch] =
          m_Output_Real *
            -m_DCBlockAfter[ch].filter( // Minus, because this filter erroneously inverts polarity.
                                        // I reported this bug, but the developer denied there was a problem.
              m_HighCut[ch].filter(
                m_Scoop[ch].filter(

                  m_Oversampler[ch].Process(
                    m_Drive_Real *
                      -m_DCBlockBefore[ch].filter(  // Minus, because this filter erroneously inverts polarity.
                                                    // I reported this bug, but the developer denied there was a problem.
                        x[ch]), // Use earlier stereoised values
                    [&](sample input) {
                      return m_Waveshaper[ch].processAudioSample(input);
                    }
                  )
                )
              )
            );

        if (m_Active != 1.0) {

          // Transition: ////////////////////////////////////////////////////

          x[ch] =
            ((1.0 - m_Active) * inputs[std::min(ch, nInChans-1)][s]) +
            ((m_Active)       * x[ch]);

        }

        outputs[std::min(ch, nOutChans-1)][s] = x[ch];

      }

    }
  }
}

void Doofuzz::updateKnobs() {
  if (GetUI()) {

    float weight = float((1.0 + GetParam(kParamActive)->Value()) / 2.0f);

    auto pGraphics  = GetUI();
    auto nControls  = pGraphics->NControls();

    for (int c = 0; c < nControls; c++) {
      auto ctrl = pGraphics->GetControl(c);
      if (ctrl->GetParamIdx() != kParamActive) {
        ctrl->SetBlend(IBlend(EBlend::Default, weight));
        ctrl->SetDirty(false);
      }
    }
  }
}

inline void Doofuzz::AdjustOversampling() {
  for (int ch = 0; ch < kMaxNumChannels; ch++) {

    if (m_Oversampling >= 0.5) {
      m_Oversampler[ch].SetOverSampling(EFactor::k16x);
    } else {
      m_Oversampler[ch].SetOverSampling(EFactor::kNone);
    }

    /*m_Oversampler[ch][0].SetOverSampling(EFactor(std::min(int(m_Oversampling),
                                                          int(EFactor::k16x))));
    m_Oversampler[ch][1].SetOverSampling(EFactor(std::max(int(m_Oversampling) - int(EFactor::k16x),
                                                          int(EFactor::kNone))));*/
  }
}

inline void Doofuzz::updateStages(bool _resetting) {

  const double sr = GetSampleRate();

  // Non-parameter-related stages:
  if (_resetting) {

    m_Stereoiser.reset(sr);
    m_Stereoiser.setWidth(m_Width);

    for (int ch = 0; ch < kMaxNumChannels; ch++) {

      m_DCBlockBefore[ch].setup(sr, kDCBlockFreq);

      m_Oversampler[ch].Reset(GetBlockSize());

      m_Waveshaper[ch].reset(sr * m_Oversampler[ch].GetRate());
      // m_Waveshaper[ch].setEnvCutOffFreq(kEnvCutoff);
      m_Waveshaper[ch].setRip(m_Rip);

      m_Scoop[ch].setup(sr, kScoopFreq, kScoop_dB, kScoopBandwidth);

      m_HighCut[ch].setup(sr, m_Tone);

      m_DCBlockAfter [ch].setup(sr, kDCBlockFreq);

    }
  }

  // Parameter-related stages:
  for (int p = 0; p < kNumParams; p++) {

    switch (p) {

      case kParamWidth: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Width = v;
          m_Stereoiser.setWidth(v);
        }
        break;
      }

      case kParamDrive: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Drive_Real = DBToAmp(v);
        }
        break;
      }

      case kParamRip: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Rip = v;
          for (int ch = 0; ch < kMaxNumChannels; ch++) {
            m_Waveshaper[ch].setRip(v);
          }
        }
        break;
      }

      case kParamTone: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Tone = v;
          for (int ch = 0; ch < kMaxNumChannels; ch++) {
            m_HighCut[ch].setup(sr, v);
          }
        }
        break;
      }

      /*case kParamPolCutoffFreq: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_PolCutoff = v;
          for (int ch = 0; ch < xkMaxNumChannels; ch++) {
            m_Waveshaper[ch].setPolCutOffFreq(v);
          }
        }
        break;
      }*/

                              //case kParamMid: {
      //  double v;
      //  if (smoother.get(p, v) || _resetting) {
      //    m_Mid_dB = v;  // Knob setting == Value in dB
      //    AdjustMid();
      //  }
      //  break;
      //}

      //case kParamHigh: {
      //  double v;
      //  if (smoother.get(p, v) || _resetting) {
      //    m_HighPos = v;  // Knob setting
      //    AdjustMid();
      //    AdjustHigh();
      //  }
      //  break;
      //}

      case kParamOutput: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Output_Real = DBToAmp(v);
        }
        break;
      }

      case kParamActive: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Active = v;
        }
        break;
      }

      case kParamOversampling: {
        double v;
        if (smoother.get(p, v) || _resetting) {
          m_Oversampling = v;
          AdjustOversampling();

          for (int ch = 0; ch < kMaxNumChannels; ch++) {
            m_Waveshaper[ch].reset(sr * m_Oversampler[ch].GetRate());
          }

        }
        break;
        //if (m_Oversampling != m_PrevOversampling) {
        //  m_PrevOversampling = m_Oversampling;
        //  AdjustOversampling();
        //  // SetParameterValue(p, GetParam(p)->ToNormalized(m_Oversampling));

        //  // SendParameterValueFromAPI doesn't work
        //  // SendParameterValueFromDelegate doesn't work
        //  SendParameterValueFromUI(p, m_Oversampling, false);
        //  x;
        //    /*FromUI(kParamOversampling,
        //    GetParam(kParamOversampling)->ToNormalized(m_Oversampling));*/
        //}
        //break;
      }

      default: {
        FAIL("Parameter missing");
        break;
      }

    }
  }
}
