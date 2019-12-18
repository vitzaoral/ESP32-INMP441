#ifndef __MicrophoneController_H
#define __MicrophoneController_H
#include <Arduino.h>

#include <driver/i2s.h>
#include <soc/rtc.h>
#include "iir-filter.h"

//
// Configuration
//

#define LEQ_PERIOD 5 // second(s)
#define WEIGHTING A_weighting
#define LEQ_UNITS "LAeq"
#define DB_UNITS "dB(A)"

#define MIC_EQUALIZER INMP441 // See below for defined IIR filters
#define MIC_OFFSET_DB 3.0103  // Default offset (sine-wave RMS vs. dBFS)
#define MIC_REF_DB 94.0       // dB(SPL)
#define MIC_BITS 24           // valid bits in I2S data
#define MIC_REF_AMPL 420426   // Amplitude at 94dB(SPL) (-26dBFS from datasheet, i.e. (2^23-1)*10^(-26/20) )
#define MIC_OVERLOAD_DB 116.0 // dB - Acoustic overload point*/
#define MIC_NOISE_DB 30       // dB - Noise floor*/

//
// I2S Setup
//
#define I2S_TASK_PRI 4
#define I2S_TASK_STACK 2048
#define I2S_PORT I2S_NUM_0

//
// I2S pins
//

// MIC 1
#define MIC_1_I2S_WS 12
#define MIC_1_I2S_SD 14  //- OK da se menit OK: 32, 35, 33
#define MIC_1_I2S_SCK 13 // OK 13, 14

#define MIC_1_CHANEL 18
#define MIC_2_CHANEL 19

//
// Sampling
//
#define SAMPLE_RATE 48000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS 32    // bits
#define SAMPLE_T int32_t
#define SAMPLE_CONVERT(s) (s >> (SAMPLE_BITS - MIC_BITS))
#define SAMPLES_SHORT (SAMPLE_RATE / 8) // ~125ms
#define SAMPLES_LEQ (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE (SAMPLES_SHORT / 16)
#define DMA_BANKS 32

struct MicrophoneData
{
    double leq;
};

class MicrophoneController
{
public:
    MicrophoneController();
    MicrophoneData sensorA;
    MicrophoneData sensorB;
    MicrophoneData sensorC;
    void setData();

private:
    void setSensorData(MicrophoneData *data);
    void mic_i2s_install();
    void mic_i2s_set_pin();
};

#endif