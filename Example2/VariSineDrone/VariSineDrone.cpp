#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;
Oscillator osc;

// Frequency range on Knob 1
static const float kFreqMin = 20.0f;
static const float kFreqMax = 2000.0f;

// The waveforms we want to cycle through, in order
// These constants come from the Oscillator class in DaisySP
static const uint8_t kWaveforms[] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_SAW,
    Oscillator::WAVE_RAMP,
    Oscillator::WAVE_SQUARE,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
};

// How many waveforms we have — calculated automatically so you can
// add/remove entries from the array above without changing anything else
static const uint8_t kNumWaveforms = sizeof(kWaveforms) / sizeof(kWaveforms[0]);

// LED colors for each waveform, as {Red, Green, Blue} pairs (0.0 to 1.0)
// One entry per waveform, in the same order as kWaveforms[]
static const float kColors[7][3] = {
    {0.0f, 0.0f, 1.0f},  // SIN          → blue
    {0.0f, 1.0f, 1.0f},  // TRI          → cyan
    {0.0f, 1.0f, 0.0f},  // SAW          → green
    {1.0f, 0.5f, 0.0f},  // RAMP         → orange
    {1.0f, 0.0f, 0.0f},  // SQUARE       → red
    {1.0f, 0.0f, 1.0f},  // POLYBLEP SAW → magenta
    {1.0f, 1.0f, 0.0f},  // POLYBLEP SQR → yellow
};

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // --- Knob 1: Frequency (exponential mapping) ---
    float knob1 = hw.knob1.Process();
    float freq  = kFreqMin * powf(kFreqMax / kFreqMin, knob1);
    osc.SetFreq(freq);

    // --- Knob 2: Waveform selector ---
    // knob2 returns 0.0 to 1.0. We multiply by kNumWaveforms to spread
    // the waveforms evenly across the knob's range, then cast to int
    // to get a discrete index (0, 1, 2, ..., kNumWaveforms-1).
    float knob2      = hw.knob2.Process();
    uint8_t waveIdx  = (uint8_t)(knob2 * kNumWaveforms);

    // Clamp to valid range — when knob is fully at 1.0,
    // the multiplication gives exactly kNumWaveforms which is out of bounds
    if(waveIdx >= kNumWaveforms)
        waveIdx = kNumWaveforms - 1;

    osc.SetWaveform(kWaveforms[waveIdx]);

    // Update LED color to reflect the current waveform
    hw.led1.Set(kColors[waveIdx][0], kColors[waveIdx][1], kColors[waveIdx][2]);
    hw.led2.Set(kColors[waveIdx][0], kColors[waveIdx][1], kColors[waveIdx][2]);
    hw.UpdateLeds();

    // --- Fill the audio buffer ---
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
    hw.SetAudioBlockSize(4);

    float sample_rate = hw.AudioSampleRate();

    osc.Init(sample_rate);
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetFreq(220.0f);
    osc.SetAmp(0.7f);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true) {}
}
