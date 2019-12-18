// #include <Arduino.h>

// // Connect to ESP32:
// // INMP441 >> ESP32:
// // SCK >> GPIO14
// // SD >> GPIO32
// // WS >> GPIO15
// // GND >> GND
// // VDD >> VDD3.3

// // TENTO JE GOOOD https://hackaday.io/project/166867-esp32-i2s-slm & https://github.com/ikostoski/esp32-i2s-slm

// // dalsi...:
// // nevim : https://community.hiveeyes.org/t/first-steps-digital-sound-recording/306
// // vzor https://github.com/maspetsberger/esp32-i2s-mems

// #include <driver/i2s.h>
// #include <soc/rtc.h>
// #include "iir-filter.h"

// //
// // Configuration
// //

// #define LEQ_PERIOD 5 // second(s)
// #define WEIGHTING A_weighting
// #define LEQ_UNITS "LAeq"
// #define DB_UNITS "dB(A)"

// #define MIC_EQUALIZER INMP441 // See below for defined IIR filters
// #define MIC_OFFSET_DB 3.0103  // Default offset (sine-wave RMS vs. dBFS)
// #define MIC_REF_DB 94.0       // dB(SPL)
// #define MIC_BITS 24           // valid bits in I2S data
// #define MIC_REF_AMPL 420426   // Amplitude at 94dB(SPL) (-26dBFS from datasheet, i.e. (2^23-1)*10^(-26/20) )
// #define MIC_OVERLOAD_DB 116.0 // dB - Acoustic overload point*/
// #define MIC_NOISE_DB 30       // dB - Noise floor*/

// //
// // I2S pins
// //

// // MIC 1
// #define MIC_1_I2S_WS 12
// #define MIC_1_I2S_SD 14  //- OK da se menit OK: 32, 35, 33
// #define MIC_1_I2S_SCK 13 // OK 13, 14

// #define MIC_1_CHANEL 18
// #define MIC_2_CHANEL 19

// //
// // Sampling
// //
// #define SAMPLE_RATE 48000 // Hz, fixed to design of IIR filters
// #define SAMPLE_BITS 32    // bits
// #define SAMPLE_T int32_t
// #define SAMPLE_CONVERT(s) (s >> (SAMPLE_BITS - MIC_BITS))
// #define SAMPLES_SHORT (SAMPLE_RATE / 8) // ~125ms
// #define SAMPLES_LEQ (SAMPLE_RATE * LEQ_PERIOD)
// #define DMA_BANK_SIZE (SAMPLES_SHORT / 16)
// #define DMA_BANKS 32

// //
// // IIR Filters
// //

// //
// // Equalizer IIR filters to flatten microphone frequency response
// // See respective .m file for filter design. Fs = 48Khz.
// //

// // TDK/InvenSense INMP441
// // Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/INMP441.pdf
// const double INMP441_B[] = {1.00198, -1.99085, 0.98892};
// const double INMP441_A[] = {1.0, -1.99518, 0.99518};
// IIR_FILTER INMP441(INMP441_B, INMP441_A);

// //
// // A-weighting 6th order IIR Filter, Fs = 48KHz
// // (By Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
// //
// const double A_weighting_B[] = {0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003};
// const double A_weighting_A[] = {1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968};
// IIR_FILTER A_weighting(A_weighting_B, A_weighting_A);

// // Data we push to 'samples_queue'
// struct samples_queue_t
// {
//   // Sum of squares of mic samples, after Equalizer filter
//   IIR_ACCU_T sum_sqr_SPL;
//   // Sum of squares of weighted mic samples
//   IIR_ACCU_T sum_sqr_weighted;
// };
// int32_t samples[SAMPLES_SHORT];
// QueueHandle_t samples_queue;

// //
// // I2S Setup
// //
// #define I2S_TASK_PRI 4
// #define I2S_TASK_STACK 2048
// #define I2S_PORT I2S_NUM_0

// void mic_i2s_install()
// {
//   // Setup I2S to sample mono channel for SAMPLE_RATE * SAMPLE_BITS
//   // NOTE: Recent update to Arduino_esp32 (1.0.2 -> 1.0.3)
//   //       seems to have swapped ONLY_LEFT and ONLY_RIGHT channels
//   const i2s_config_t i2s_config = {
//       .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
//       .sample_rate = SAMPLE_RATE,
//       .bits_per_sample = i2s_bits_per_sample_t(SAMPLE_BITS),
//       .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//       .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
//       .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//       .dma_buf_count = DMA_BANKS,
//       .dma_buf_len = DMA_BANK_SIZE,
//       .use_apll = true,
//       .tx_desc_auto_clear = false,
//       .fixed_mclk = 0};

//   i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
// }

// void mic_i2s_set_pin()
// {
//   // I2S pin mapping
//   const i2s_pin_config_t pin_config = {
//       .bck_io_num = MIC_1_I2S_SCK,
//       .ws_io_num = MIC_1_I2S_WS,
//       .data_out_num = -1, // not used
//       .data_in_num = MIC_1_I2S_SD};

//   i2s_set_pin(I2S_PORT, &pin_config);

//   //FIXME: There is a known issue with esp-idf and sampling rates, see:
//   //       https://github.com/espressif/esp-idf/issues/2634
//   //       Update (when available) to the latest versions of esp-idf *should* fix it
//   //       In the meantime, the below line seems to set sampling rate at ~47999.992Hz
//   //       fifs_req=24576000, sdm0=149, sdm1=212, sdm2=5, odir=2 -> fifs_reached=24575996
//   // rtc_clk_apll_enable(1, 149, 212, 5, 2);
// }

// //
// // I2S Reader Task
// //
// // Rationale for separate task reading I2S is that IIR filter
// // processing cam be scheduled to different core on the ESP32
// // while i.e. the display is beeing updated...
// //
// void mic_i2s_reader_task(void *parameter)
// {

//   //mic_i2s_init();

//   //Discard first block, microphone may have startup time(i.e.INMP441 up to 83ms)
//   size_t bytes_read = 0;
//   i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

//   while (true)
//   {
//     //Serial.print(".");
//     IIR_ACCU_T sum_sqr_SPL = 0;
//     IIR_ACCU_T sum_sqr_weighted = 0;
//     samples_queue_t q;
//     int32_t sample;
//     IIR_BASE_T s;

//     i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);
//     for (int i = 0; i < SAMPLES_SHORT; i++)
//     {
//       sample = SAMPLE_CONVERT(samples[i]);
//       s = sample;
//       s = MIC_EQUALIZER.filter(s);
//       ACCU_SQR(sum_sqr_SPL, s);
//       s = WEIGHTING.filter(s);
//       ACCU_SQR(sum_sqr_weighted, s);
//     }

//     q.sum_sqr_SPL = sum_sqr_SPL;
//     q.sum_sqr_weighted = sum_sqr_weighted;
//     xQueueSend(samples_queue, &q, portMAX_DELAY);
//   }
// }

// double Leq_dB = 0;

// void setup()
// {
//   Serial.begin(112500);
//   pinMode(MIC_1_CHANEL, OUTPUT);
//   pinMode(MIC_2_CHANEL, OUTPUT);
//   delay(1000);

//   mic_i2s_install();
//   mic_i2s_set_pin();

//   Serial.println("GOOOO\n\n");
// }

// bool first = true;

// void loop()
// {
//   Serial.println(first ? "FIRST" : "SECOND");
//   digitalWrite(MIC_1_CHANEL, first ? LOW : HIGH);
//   digitalWrite(MIC_2_CHANEL, first ? HIGH : LOW);
//   delay(500);

//   i2s_start(I2S_PORT);

//   delay(500);

//   //Discard first block, microphone may have startup time(i.e.INMP441 up to 83ms)
//   size_t bytes_read = 0;
//   i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

//   uint32_t Leq_cnt = 0;
//   IIR_ACCU_T Leq_sum_sqr = 0;

//   int count = 0;

//   while (true)
//   {
//     //Serial.print(".");
//     IIR_ACCU_T sum_sqr_SPL = 0;
//     IIR_ACCU_T sum_sqr_weighted = 0;
//     samples_queue_t q;
//     int32_t sample;
//     IIR_BASE_T s;

//     i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);
//     for (int i = 0; i < SAMPLES_SHORT; i++)
//     {
//       sample = SAMPLE_CONVERT(samples[i]);
//       s = sample;
//       s = MIC_EQUALIZER.filter(s);
//       ACCU_SQR(sum_sqr_SPL, s);
//       s = WEIGHTING.filter(s);
//       ACCU_SQR(sum_sqr_weighted, s);
//     }

//     q.sum_sqr_SPL = sum_sqr_SPL;
//     q.sum_sqr_weighted = sum_sqr_weighted;

//     // Calculate dB values relative to MIC_REF_AMPL and adjust for reference dB
//     double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT) / MIC_REF_AMPL);

//     // In case of acoustic overload, report infinty Leq value
//     if (short_SPL_dB > MIC_OVERLOAD_DB)
//       Leq_sum_sqr = INFINITY;

//     Leq_sum_sqr += q.sum_sqr_weighted;
//     Leq_cnt += SAMPLES_SHORT;
//     if (Leq_cnt >= SAMPLE_RATE * LEQ_PERIOD)
//     {
//       Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(Leq_sum_sqr) / Leq_cnt) / MIC_REF_AMPL);
//       Leq_sum_sqr = 0;
//       Leq_cnt = 0;
//       printf("%s(%ds) : %.1f\n", LEQ_UNITS, LEQ_PERIOD, Leq_dB);
//     }

//     count++;

//     if (count > 50)
//     {
//       break;
//     }
//   }

//   i2s_stop(I2S_PORT);
//   first = !first;
// }