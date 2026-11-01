/*
 * graphicProcessing.h
 *
 *  Created on: 5 wrz 2019
 *      Author: zieje
 */

#ifndef INTERFACE_GRAPHICPROCESSING_H_
#define INTERFACE_GRAPHICPROCESSING_H_

#include "interfaceDefs.h"
#include "spectrumControl.h"

#include <stdint.h>

class cGraphicProcessing
{
  public:
    void spectrumResetZoom(uint16_t position, uint32_t sampleLength, strZoomParams* zoom);
    void spectrumAutoZoomIn(uint16_t position,
                            uint16_t zoomWidth,
                            uint32_t sampleLength,
                            strZoomParams* zoom);
    void spectrumChangeZoom(int16_t value, uint32_t sampleLength, strZoomParams* zoom);

    void processSpectrum(strSpectrumParams* params,
                         strZoomParams* zoom,
                         strTrackerSpectrum* spectrum);
    void processSpectrum(strSampleMemory* sampleMemory,
                         strInstrument* instrument,
                         strZoomParams* zoom,
                         strTrackerSpectrum* spectrum,
                         strSampleMemory::Channel channel = strSampleMemory::Channel::kLeft,
                         uint32_t wt_pos = 0);
    void processSpectrum(int16_t* address,
                         uint32_t length,
                         strZoomParams* zoom,
                         strTrackerSpectrum* spectrum);
};

extern cGraphicProcessing GP;

#endif /* INTERFACE_GRAPHICPROCESSING_H_ */
