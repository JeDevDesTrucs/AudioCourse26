#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;
Oscillator osc;

// ─── Frequency (Knob 1) ───────────────────────────────────────────────────────
static const float kFreqMin = 20.0f;
static const float kFreqMax = 2000.0f;

// ─── Waveforms (Knob 2) ──────────────────────────────────────────────────────
static const uint8_t kWaveforms[] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_SAW,
    Oscillator::WAVE_RAMP,
    Oscillator::WAVE_SQUARE,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
};
static const uint8_t kNumWaveforms = sizeof(kWaveforms) / sizeof(kWaveforms[0]);

static const float kWaveColors[7][3] = {
    {0.0f, 0.0f, 1.0f},  // SIN          → blue
    {0.0f, 1.0f, 1.0f},  // TRI          → cyan
    {0.0f, 1.0f, 0.0f},  // SAW          → green
    {1.0f, 0.5f, 0.0f},  // RAMP         → orange
    {1.0f, 0.0f, 0.0f},  // SQUARE       → red
    {1.0f, 0.0f, 1.0f},  // POLYBLEP SAW → magenta
    {1.0f, 1.0f, 0.0f},  // POLYBLEP SQR → yellow
};

// ─── Filter types (Encoder press cycles through these) ───────────────────────
// Svf (State Variable Filter) computes LP/HP/BP/Notch simultaneously.
// We use an enum to track which output we're reading.
enum FilterMode {
    FILTER_BYPASS = 0,  // no filter, raw oscillator signal
    FILTER_LOWPASS,     // cuts highs, keeps lows — warm, dark
    FILTER_HIGHPASS,    // cuts lows, keeps highs — thin, airy
    FILTER_BANDPASS,    // keeps a band in the middle — nasal, telephone
    FILTER_NOTCH,       // cuts a band in the middle — hollow, phasy
    FILTER_NUM_MODES    // not a real mode — just counts how many we have
};

// LED colors for each filter mode on LED 1 (LED 2 keeps waveform color)
static const float kFilterColors[FILTER_NUM_MODES][3] = {
    {1.0f, 1.0f, 1.0f},  // BYPASS    → white
    {0.0f, 0.0f, 1.0f},  // LOWPASS   → blue  (warm/dark = cool color)
    {1.0f, 0.5f, 0.0f},  // HIGHPASS  → orange (bright/sharp = warm color)
    {0.0f, 1.0f, 0.0f},  // BANDPASS  → green
    {1.0f, 0.0f, 0.5f},  // NOTCH     → pink
};

// ─── Filter range (Encoder controls cutoff) ──────────────────────────────────
static const float kCutoffMin = 20.0f;
static const float kCutoffMax = 18000.0f;
// Filter resonance is fixed here, try to change it and see what happen [0.0 - 1.0]
static const float kResonance = 0.4f;

// ─── State variable filter (SVF) — handles LP/HP/BP/Notch ───────────────────
Svf svf;

// ─── Global state ─────────────────────────────────────────────────────────────
// 'volatile' tells the compiler this can change outside normal program flow
// (e.g. from an interrupt). Not strictly needed here but good habit.
static FilterMode gFilterMode  = FILTER_BYPASS;
static float      gCutoffFreq  = 1000.0f;  // starting cutoff in Hz

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // ── Knob 1: Oscillator frequency ─────────────────────────────────────────
    float knob1 = hw.knob1.Process();
    float freq  = kFreqMin * powf(kFreqMax / kFreqMin, knob1);
    osc.SetFreq(freq);

    // ── Knob 2: Waveform selector ─────────────────────────────────────────────
    float   knob2   = hw.knob2.Process();
    uint8_t waveIdx = (uint8_t)(knob2 * kNumWaveforms);
    if(waveIdx >= kNumWaveforms) waveIdx = kNumWaveforms - 1;
    osc.SetWaveform(kWaveforms[waveIdx]);

    // ── Encoder rotation: Filter cutoff ──────────────────────────────────────
    // Encoder::Increment() returns:
    //   +1 when turned clockwise one detent
    //   -1 when turned counter-clockwise one detent
    //    0 when not moving
    // We scale by a step size (200 Hz per detent) and clamp to valid range.
    int32_t encMove = hw.encoder.Increment();
    gCutoffFreq += encMove * 200.0f;
    if(gCutoffFreq < kCutoffMin) gCutoffFreq = kCutoffMin;
    if(gCutoffFreq > kCutoffMax) gCutoffFreq = kCutoffMax;

    // Apply cutoff to the SVF
    svf.SetFreq(gCutoffFreq);
    svf.SetRes(kResonance);

    // ── Encoder button press: Cycle filter mode ───────────────────────────────
    // RisingEdge() returns true only on the exact moment the button is pressed
    // (not held). Without this, holding the button would cycle modes every
    // single audio callback — thousands of times per second.
    if(hw.encoder.RisingEdge())
    {
        // Cast to int to do arithmetic, then cast back to the enum type
        gFilterMode = (FilterMode)((gFilterMode + 1) % FILTER_NUM_MODES);
    }

    // ── Update LEDs ───────────────────────────────────────────────────────────
    // LED 1 = filter mode color
    // LED 2 = waveform color
    hw.led1.Set(kFilterColors[gFilterMode][0],
                kFilterColors[gFilterMode][1],
                kFilterColors[gFilterMode][2]);
    hw.led2.Set(kWaveColors[waveIdx][0],
                kWaveColors[waveIdx][1],
                kWaveColors[waveIdx][2]);
    hw.UpdateLeds();

    // ── Audio processing loop ─────────────────────────────────────────────────
    for(size_t i = 0; i < size; i++)
    {
        float dry = osc.Process();  // raw oscillator sample

        // Run the sample through the SVF regardless of mode —
        // the SVF always computes all outputs simultaneously.
        // We just choose which output to use based on the mode.
        svf.Process(dry);

        float wet;  // this will hold our final processed sample

        switch(gFilterMode)
        {
            case FILTER_LOWPASS:   wet = svf.Low();   break;
            case FILTER_HIGHPASS:  wet = svf.High();  break;
            case FILTER_BANDPASS:  wet = svf.Band();  break;
            case FILTER_NOTCH:     wet = svf.Notch(); break;
            case FILTER_BYPASS:
            default:               wet = dry;          break;
        }

        out[0][i] = wet;
        out[1][i] = wet;
    }
}

int main()
{
    hw.Init();
    hw.SetAudioBlockSize(4);

    float sample_rate = hw.AudioSampleRate();

    // Init oscillator
    osc.Init(sample_rate);
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetFreq(220.0f);
    osc.SetAmp(0.7f);

    // Init SVF filter
    svf.Init(sample_rate);
    svf.SetFreq(gCutoffFreq);
    svf.SetRes(kResonance);
    svf.SetDrive(0.0f);  // no extra drive/saturation

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true) {}
}
