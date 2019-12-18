#include "MicrophoneController.h"

//
// IIR Filters
//

//
// Equalizer IIR filters to flatten microphone frequency response
// See respective .m file for filter design. Fs = 48Khz.
//

// TDK/InvenSense INMP441
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/INMP441.pdf
const double INMP441_B[] = {1.00198, -1.99085, 0.98892};
const double INMP441_A[] = {1.0, -1.99518, 0.99518};
IIR_FILTER INMP441(INMP441_B, INMP441_A);

//
// A-weighting 6th order IIR Filter, Fs = 48KHz
// (By Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
//
const double A_weighting_B[] = {0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003};
const double A_weighting_A[] = {1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968};
IIR_FILTER A_weighting(A_weighting_B, A_weighting_A);

// Data we push to 'samples_queue'
struct samples_queue_t
{
    // Sum of squares of mic samples, after Equalizer filter
    IIR_ACCU_T sum_sqr_SPL;
    // Sum of squares of weighted mic samples
    IIR_ACCU_T sum_sqr_weighted;
};
int32_t samples[SAMPLES_SHORT];
QueueHandle_t samples_queue;

MicrophoneController::MicrophoneController()
{
    pinMode(MIC_1_CHANEL, OUTPUT);
    pinMode(MIC_2_CHANEL, OUTPUT);
    delay(500);

    MicrophoneController::mic_i2s_install();
    MicrophoneController::mic_i2s_set_pin();

    Serial.println("Microphones I2S setup DONE");
}

void MicrophoneController::setData()
{
    // MIC 1
    digitalWrite(MIC_1_CHANEL, LOW);
    digitalWrite(MIC_2_CHANEL, HIGH);

    delay(500);
    i2s_start(I2S_PORT);
    delay(500);

    Serial.print("First MIC: ");
    setSensorData(&sensorA);
    i2s_stop(I2S_PORT);

    // MIC 2
    digitalWrite(MIC_1_CHANEL, HIGH);
    digitalWrite(MIC_2_CHANEL, LOW);

    delay(500);
    i2s_start(I2S_PORT);
    delay(500);

    Serial.print("Second MIC: ");
    setSensorData(&sensorB);
    i2s_stop(I2S_PORT);
}

// Rationale for separate task reading I2S is that IIR filter
// processing cam be scheduled to different core on the ESP32
// while i.e. the display is beeing updated...
void MicrophoneController::setSensorData(MicrophoneData *data)
{
    //Discard first block, microphone may have startup time(i.e.INMP441 up to 83ms)
    size_t bytes_read = 0;
    i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    uint32_t Leq_cnt = 0;
    IIR_ACCU_T Leq_sum_sqr = 0;

    int count = 0;

    while (true)
    {
        //Serial.print(".");
        IIR_ACCU_T sum_sqr_SPL = 0;
        IIR_ACCU_T sum_sqr_weighted = 0;
        samples_queue_t q;
        int32_t sample;
        IIR_BASE_T s;

        i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        for (int i = 0; i < SAMPLES_SHORT; i++)
        {
            sample = SAMPLE_CONVERT(samples[i]);
            s = sample;
            s = MIC_EQUALIZER.filter(s);
            ACCU_SQR(sum_sqr_SPL, s);
            s = WEIGHTING.filter(s);
            ACCU_SQR(sum_sqr_weighted, s);
        }

        q.sum_sqr_SPL = sum_sqr_SPL;
        q.sum_sqr_weighted = sum_sqr_weighted;

        // Calculate dB values relative to MIC_REF_AMPL and adjust for reference dB
        double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT) / MIC_REF_AMPL);

        // In case of acoustic overload, report infinty Leq value
        if (short_SPL_dB > MIC_OVERLOAD_DB)
            Leq_sum_sqr = INFINITY;

        Leq_sum_sqr += q.sum_sqr_weighted;
        Leq_cnt += SAMPLES_SHORT;
        if (Leq_cnt >= SAMPLE_RATE * LEQ_PERIOD)
        {
            data->leq = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(Leq_sum_sqr) / Leq_cnt) / MIC_REF_AMPL);
            Leq_sum_sqr = 0;
            Leq_cnt = 0;
            printf("%s(%ds) : %.1f\n", LEQ_UNITS, LEQ_PERIOD, data->leq);
        }

        count++;

        if (count > 50)
        {
            break;
        }
    }
}

void MicrophoneController::mic_i2s_install()
{
    // Setup I2S to sample mono channel for SAMPLE_RATE * SAMPLE_BITS
    // NOTE: Recent update to Arduino_esp32 (1.0.2 -> 1.0.3)
    //       seems to have swapped ONLY_LEFT and ONLY_RIGHT channels
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BANKS,
        .dma_buf_len = DMA_BANK_SIZE,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void MicrophoneController::mic_i2s_set_pin()
{
    // I2S pin mapping
    const i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_1_I2S_SCK,
        .ws_io_num = MIC_1_I2S_WS,
        .data_out_num = -1, // not used
        .data_in_num = MIC_1_I2S_SD};

    i2s_set_pin(I2S_PORT, &pin_config);

    //FIXME: There is a known issue with esp-idf and sampling rates, see:
    //       https://github.com/espressif/esp-idf/issues/2634
    //       Update (when available) to the latest versions of esp-idf *should* fix it
    //       In the meantime, the below line seems to set sampling rate at ~47999.992Hz
    //       fifs_req=24576000, sdm0=149, sdm1=212, sdm2=5, odir=2 -> fifs_reached=24575996
    // rtc_clk_apll_enable(1, 149, 212, 5, 2);
}