#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;
Oscillator osc;

// Frequency range mapped to Knob 1
static const float kFreqMin = 20.0f;
static const float kFreqMax = 2000.0f;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // Knob 1 (K1) controls frequency — logarithmic feel via exponential mapping
    float knob1 = hw.knob1.Process();
    float freq  = kFreqMin * powf(kFreqMax / kFreqMin, knob1);
    osc.SetFreq(freq);

    for(size_t i = 0; i < size; i++)
    {
        float sample = osc.Process();
        out[0][i]    = sample;
        out[1][i]    = sample;
    }
}

int main()
{
    hw.Init();
    hw.SetAudioBlockSize(4); // small block for low latency

    float sample_rate = hw.AudioSampleRate();

    osc.Init(sample_rate);
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetFreq(220.0f);  // default A3
    osc.SetAmp(0.7f);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true)
    {
        // Nothing needed in the main loop for a simple drone
    }
}
