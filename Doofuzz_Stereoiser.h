#pragma once

#include "iir1/Iir.h"

class Stereoiser {
public:

  Stereoiser() {
    reset(48000.0);
  }

  inline double reset(const double _sampleRate) {

    m_HighPass.setup(_sampleRate, kHighPassF);

    double notchFreq = 20000.0;

    for (int n = 0; n < kNumNotches; n++) {
      for (int ch = 0; ch < 2; ch++) {
        m_Notch[ch][n].setup(_sampleRate, notchFreq /= kSqrtPhi, kNotchQ);
      }
    };

    return _sampleRate;
  }

  inline double setWidth(double _width) {
    return m_Width = _width;
  }

  inline void  processFrame(const sample  inputL,
                            const sample  inputR,
                            sample*       outputL,
                            sample*       outputR) {

    if (m_Width != 0.0) {

      double processed[2] = { inputL, inputR };

      for (int n = 0; n < kNumNotches; n++) {
        for (int ch = 0; ch < 2; ch++) {
          processed[ch] = m_Notch[ch][n].filter(processed[ch]);
        }
      }

      double processedCombinedAndFiltered = m_Width * m_HighPass.filter(processed[0] - processed[1]);

      *outputL  = inputL + processedCombinedAndFiltered; // inputL;
      *outputR  = inputR - processedCombinedAndFiltered; // inputR;

    } else {

      *outputL  = inputL;
      *outputR  = inputR;

    }

  }

private:

  static const  inline  double  kHighPassF  = 250.0;
  static const  inline  int     kNumNotches = 10;
  static const  inline  double  kNotchQ     = 7.5; // 10.0;
  static const  inline  double  kPhi        = (sqrt(5.0) + 1.0) / 2.0;
  static const  inline  double  kSqrtPhi    = sqrt(kPhi);

  double                        m_Width     = 1.0;

  Iir::RBJ::IIRNotch            m_Notch[2][kNumNotches];  // channels, notches
  Iir::Butterworth::HighPass<1> m_HighPass;

};
