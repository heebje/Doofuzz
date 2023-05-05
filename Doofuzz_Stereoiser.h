#pragma once

#include <limits>
#include "iir1/Iir.h"

class Stereoiser {
public:

  Stereoiser() {
    reset(48000.0);
  }

  inline double reset(const double _sampleRate) {

    m_HighPass.setup(_sampleRate, kHighPassF);
    m_LowPass .setup(_sampleRate, kLowPassF );

    double notchFreq = 4000.0 * kSqrtPhi;

    for (int n = 0; n < kNumNotches; n++) {
      for (int ch = 0; ch < 2; ch++) {
        m_Notch[ch][n].setup(_sampleRate, notchFreq /= kSqrtPhi, kNotchQ);
      }
    };

    return _sampleRate;
  }

  inline double setWidth(double _width) {                   // 0.0..1.0; the stored and used
    m_WidthSquared = std::clamp(_width * _width, 0.0, 1.0); // value is actually the square,
    return _width;                                          // in order to make the control
  }                                                         // feel smoother.


  inline void  processFrame(const sample  inputL,
                            const sample  inputR,
                            sample*       outputL,
                            sample*       outputR) {

 
    if (m_WidthSquared < kEpsilon) {

      *outputL  = inputL;
      *outputR  = inputR;

    } else {

      double processed[2] = { inputL, inputR };

      for (int n = 0; n < kNumNotches; n++) {
        for (int ch = 0; ch < 2; ch++) {
          processed[ch] = m_Notch[ch][n].filter(processed[ch]);
        }
      }

      double processedCombinedAndFiltered = kWidthMultiplier *
                                              m_WidthSquared *
                                                m_LowPass.filter(
                                                  m_HighPass.filter(
                                                    processed[0] - processed[1]));

      *outputL  = inputL + processedCombinedAndFiltered; // inputL;
      *outputR  = inputR - processedCombinedAndFiltered; // inputR;

    }

  }

private:

  static  const inline  double  kEpsilon          = std::numeric_limits<double>::epsilon();

  static  const inline  double  kHighPassF        =  250.0;
  static  const inline  double  kLowPassF         = 4000.0;

  static  const inline  int     kNumNotches       =  7;
  static  const inline  double  kNotchQ           = 10.0;
  static  const inline  double  kPhi              = (sqrt(5.0) + 1.0) / 2.0;
  static  const inline  double  kSqrtPhi          = sqrt(kPhi);

  static  const inline  double  kWidthMultiplier  = 2.0;

  double                        m_WidthSquared    = 1.0;

  Iir::RBJ::IIRNotch            m_Notch[2][kNumNotches];  // channels, notches
  Iir::Butterworth::HighPass<1> m_HighPass;
  Iir::Butterworth::LowPass <1> m_LowPass;

};
