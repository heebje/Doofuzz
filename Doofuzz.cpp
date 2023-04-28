#include "Doofuzz.h"

#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include "NAM_Styling.h"
#include "Doofuzz_CornerResizers.h"
#include "Doofuzz_Common.h"


using namespace NAM_Styling;
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
                                                        24.0,
                                                        NAM_1,
                                                        NAM_3.WithOpacity(0.75),
                                                        NAM_2));
    pGraphics->AttachControl(new NAMCornerShrinker(24.0,
                                                   NAM_1,
                                                   NAM_3.WithOpacity(0.75)));

    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const auto  b           = pGraphics->GetBounds();
    const auto  mainArea    = b.GetPadded(-20);
    const auto  content     = mainArea.GetPadded(-10);
    const auto  titleHeight = 50.0;
    const auto  titleLabel  = content.GetFromTop(titleHeight);

    pGraphics->AttachControl(new IVPanelControl(mainArea,
                                                "",
                                                style.WithColor(kFG,
                                                                NAM_1)));

    // auto title = std::string(PLUG_NAME);

    pGraphics->AttachControl(new IVLabelControl(titleLabel,
                                                PLUG_NAME,
                                                style.WithDrawFrame(false).WithValueText({30,
                                                  EAlign::Center,
                                                  NAM_3})));

    pGraphics->AttachControl(new IURLControl(titleLabel.GetMidHPadded(60),
                                             "",
                                             PLUG_URL_STR));

    for (int p = 0; p < kNumParams; p++) {

      switch(p) {

        // Big knobs: /////////////////////////////////////////////////////////
        case kParamDrive:
        case kParamRip:
        case kParamTone:
        case kParamOutput: {
          pGraphics->AttachControl(new IVKnobControl(controlCoordinates[p],
                                                     p,
                                                     paramLabels[p],
                                                     style))->SetTooltip(paramToolTips[p]);
          break;
        }

        case kParamActive: {
          // On/Off switch:
          pGraphics->AttachControl(new IVToggleControl(controlCoordinates[p],
                                                       p,
                                                       paramLabels[p],
                                                       style.WithShowLabel(false),
                                                       "off",
                                                       "on"))->SetTooltip(paramToolTips[p]);
          break;
        }

        case kParamOversampling: {
          // Oversampling switch:
          pGraphics->AttachControl(new IVToggleControl(controlCoordinates[p],
                                                       p,
                                                       paramLabels[p],
                                                       style.WithShowLabel(false),
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

    /*
    // EQ Plot:

    auto plotPanel = IRECT(45,230,PLUG_WIDTH-45,PLUG_HEIGHT-45);

    pGraphics->AttachControl(new IPanelControl(plotPanel,
                                                IPattern(NAM_2.WithContrast(-0.75))
                                              ));

    m_Plot = pGraphics->AttachControl(new IVPlotControl(plotPanel,
                                                        {
                                                          { NAM_2,
                                                            [&](double x) -> double {
                                                              return m_PlotValues[int(x * (PLUG_WIDTH-1))];
                                                            }
                                                          }
                                                        },
                                                        pGraphics->Width()));
    m_Plot->SetBlend(IBlend(EBlend::Default, 0.75));*/

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

void Doofuzz::OnIdle() {
  /*if (m_PlotNeedsRecalc && m_Plot && GetUI()) {

    if (m_Active) {

      const double sr = GetSampleRate();

      m_LowCut    [kPlotChannel].setup(sr, getLowCutFreq());
      m_LowShelf  [kPlotChannel].setup(sr, getLowShelfFreq(),   getLowShelfBoost(),   kLowBoostSlope);

      m_MidBefore [kPlotChannel].setup(sr, getMidFreq(),        getMidBefore_dB(),    getMidBandwidth());
      m_MidAfter  [kPlotChannel].setup(sr, getMidFreq(),        getMidAfter_dB(),     getMidBandwidth());

      m_HighCut   [kPlotChannel].setup(sr, getHighCutFreq());
      m_HighShelf [kPlotChannel].setup(sr, getHighShelfFreq(),  getHighShelfBoost(),  kHighBoostSlope);

      m_DCBlock   [kPlotChannel].setup(sr, kDCBlockFreq);

      const double ln1000 = std::log(1000.0);

      const double  maxBoost = DBToAmp(std::max({ getLowShelfBoost(),
                                                  getMidBefore_dB(),
                                                  getHighShelfBoost() }));

      for (int s = 0; s < PLUG_WIDTH; s++) {
        double f = 20.0 * exp(ln1000 * s / (PLUG_WIDTH)) / sr;
        m_PlotValues[s] = std::log(abs(m_LowCut   [kPlotChannel].response(f))
                                 * abs(m_LowShelf [kPlotChannel].response(f))

                                 * abs(m_MidBefore[kPlotChannel].response(f))
                                 * abs(m_MidAfter [kPlotChannel].response(f))

                                 * abs(m_HighCut  [kPlotChannel].response(f))
                                 * abs(m_HighShelf[kPlotChannel].response(f))

                                 * abs(m_DCBlock  [kPlotChannel].response(f))

                                 / maxBoost
                                  ) * 0.5 + 0.5;

      }
    } else {
      for (int s = 0; s < PLUG_WIDTH; s++) {
        m_PlotValues[s] = 0.0;
      }
    }

    m_PlotNeedsRecalc = false;
    m_Plot->SetDirty(false);

  }*/
}

void Doofuzz::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {

  // const double  sr        = GetSampleRate();
  const int     nInChans  = NInChansConnected();
  const int     nOutChans = NOutChansConnected();
  const int     nMaxChans = std::max(nInChans, nOutChans);

  for (int s = 0; s < nFrames; s++) {

    updateStages(false);

    for (int ch = 0; ch < nMaxChans; ch++) {

      if ((nOutChans > nInChans) && (ch == 1)) { // The second output channel in the "1-2" situation
        outputs[1][s] = outputs[0][s]; // Simply copy

      } else {

        if (m_Active == 0.0) {

          // Bypassed: ////////////////////////////////////////////////////////

          outputs[ch][s] = inputs[ch][s];

        } else {

          outputs[ch][s] =
            m_Output_Real *
              -m_DCBlockAfter[ch].filter( // Minus, because this filter erroneously inverts polarity.
                                          // I reported this bug, but the developer denied there was a problem.
                m_HighCut[ch].filter(
                  m_Scoop[ch].filter(

                    m_Oversampler[ch].Process(
                      m_Drive_Real *
                        -m_DCBlockBefore[ch].filter(  // Minus, because this filter erroneously inverts polarity.
                                                      // I reported this bug, but the developer denied there was a problem.
                          inputs[ch][s]),
                      [&](sample input) {
                        return m_Waveshaper[ch].processAudioSample(input);
                      }
                    )
                  )
                )
              );

          if (m_Active != 1.0) {

            // Transition: ////////////////////////////////////////////////////

            outputs[ch][s] =
              ((1.0 - m_Active) * inputs [ch][s]) +
              ((m_Active)       * outputs[ch][s]);

          }
        }
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
          // m_PlotNeedsRecalc = true;
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
