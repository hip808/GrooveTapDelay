#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
#include <stdexcept>

static void check (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
}
static void set (GrooveTapDelayAudioProcessor& p, const juce::String& id, float v)
{
    auto* parameter = p.apvts.getParameter (id);
    check (parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (v));
}
static void initialise (GrooveTapDelayAudioProcessor& p, double sr = 48000)
{
    for (int b = 0; b < kNumBands; ++b)
        set (p, p.bandParamId (b, "enabled"), 0);
    set (p, "directLevel", 0);
    set (p, "effectLevel", 1);
    set (p, "output", 0);
    set (p, "topology", 0);
    set (p, "band1_time", 10);
    set (p, "band1_syncpct", 1);
    set (p, "band1_lfodepth", 0);
    set (p, "band1_feedback", 0);
    set (p, "band1_pan", -1);
    set (p, "band1_level", 1);
    set (p, "band1_lowcut", 0);
    set (p, "band1_highcut", 0);
    set (p, "band1_enabled", 1);
    p.prepareToPlay (sr, 512);
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        juce::MidiBuffer midi;
        {
            GrooveTapDelayAudioProcessor p;
            auto valueFromText = [&p] (const char* id, const char* text)
            {
                auto* param = p.apvts.getParameter (id);
                return param->convertFrom0to1 (param->getValueForText (text));
            };
            check (std::abs (valueFromText ("band1_lfospeed", "3.5") - 0.35f) < 1e-5f, "0-10 text entry uses wrong units");
            check (std::abs (valueFromText ("band1_pan", "L5") + 0.5f) < 1e-5f, "Pan text entry broken");
            check (std::abs (valueFromText ("band1_syncpct", "50 %") - 0.5f) < 1e-5f, "Tap percent text entry broken");
            check (std::abs (valueFromText ("band1_highcut", "1000 Hz") - 1.0f) < 1e-5f, "Filter text entry broken");
        }
        {
            GrooveTapDelayAudioProcessor p; initialise (p);
            set (p, "band1_feedback", 0.95f); set (p, "band1_time", 696);
            check (p.getTailLengthSeconds() > 90, "Host tail estimate truncates long feedback");
            juce::AudioBuffer<float> b (2, 512); b.clear(); b.setSample (0, 0, 1); p.processBlock (b, midi);
            p.reset();
            for (int k = 0; k < 100; ++k) { b.clear(); p.processBlock (b, midi); check (juce::exactlyEqual (b.getMagnitude (0, 512), 0.0f), "Reset leaves old delay data"); }
        }
        {
            GrooveTapDelayAudioProcessor p;
            auto layout = p.getBusesLayout();
            layout.inputBuses.set (0, juce::AudioChannelSet::mono());
            check (p.setBusesLayout (layout), "Mono layout rejected");
            initialise (p);
            set (p, "bypass", 1);
            juce::AudioBuffer<float> b (2, 128);
            for (int i = 0; i < 128; ++i) { b.setSample (0, i, 0.25f); b.setSample (1, i, -0.8f); }
            p.processBlock (b, midi);
            for (int i = 0; i < 128; ++i)
                check (juce::exactlyEqual (b.getSample (0, i), 0.25f) && juce::exactlyEqual (b.getSample (1, i), 0.25f), "Mono bypass reads output-only channel");
        }
        {
            GrooveTapDelayAudioProcessor p; initialise (p);
            set (p, "band1_time", 0);
            juce::AudioBuffer<float> b (2, 128); b.clear(); b.setSample (0, 0, 1);
            p.processBlock (b, midi);
            check (std::abs (b.getSample (0, 1) - 1) < 1e-6f, "Zero-delay must be causal one-sample delay");
            check (juce::exactlyEqual (b.getSample (0, 0), 0.0f), "Unexpected zero-delay output");
        }
        {
            GrooveTapDelayAudioProcessor p; initialise (p);
            set (p, "band1_feedback", 0.5f);
            juce::AudioBuffer<float> b (2, 1600); b.clear(); b.setSample (0, 0, 1);
            p.processBlock (b, midi);
            check (std::abs (b.getSample (0, 480) - 1) < 1e-3f, "First impulse time wrong");
            check (std::abs (b.getSample (0, 960) - 0.5f) < 1e-3f, "OFF filters color feedback");
            check (std::abs (b.getSample (0, 1440) - 0.25f) < 1e-3f, "Feedback decay wrong");
            for (int i = 0; i < 1600; ++i)
                if (i != 480 && i != 960 && i != 1440)
                    check (std::abs (b.getSample (0, i)) < 1e-3f, "OFF filters produce ringing");
        }
        {
            GrooveTapDelayAudioProcessor p; initialise (p);
            set (p, "band1_enabled", 0); set (p, "topology", 1);
            juce::AudioBuffer<float> b (2, 128);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) b.setSample (c, i, 0.5f);
            p.processBlock (b, midi);
            check (juce::exactlyEqual (b.getMagnitude (0, 128), 0.0f), "Disabled series chain leaks dry into wet");
        }
        {
            GrooveTapDelayAudioProcessor p; initialise (p);
            juce::AudioBuffer<float> b (2, 64); b.clear(); b.setSample (0, 0, 1); p.processBlock (b, midi);
            set (p, "band1_enabled", 0);
            for (int k = 0; k < 600; ++k) { b.clear(); p.processBlock (b, midi); }
            set (p, "band1_enabled", 1);
            for (int k = 0; k < 20; ++k) { b.clear(); p.processBlock (b, midi); check (juce::exactlyEqual (b.getMagnitude (0, 64), 0.0f), "Muted band replays frozen audio"); }
        }
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            GrooveTapDelayAudioProcessor p; initialise (p, sr);
            set (p, "band1_highcut", 0.03f); set (p, "band1_lowcut", 1); set (p, "band1_feedback", 0.95f);
            juce::AudioBuffer<float> b (2, 512); b.clear(); b.setSample (0, 0, 1);
            for (int k = 0; k < 100; ++k)
            {
                p.processBlock (b, midi);
                for (int i = 0; i < 512; ++i) check (std::isfinite (b.getSample (0, i)), "Unstable low-rate filters");
                b.clear();
            }
        }
        {
            GrooveTapDelayAudioProcessor p;
            juce::MemoryBlock original; p.getStateInformation (original);
            p.renamePatch (90, "Renamed without save");
            juce::MemoryBlock renamed; p.getStateInformation (renamed);
            GrooveTapDelayAudioProcessor restored;
            restored.setStateInformation (renamed.getData(), (int) renamed.getSize());
            check (restored.getPatchName (90) == "Renamed without save", "Renamed patch lost in session");
            restored.setStateInformation (original.getData(), (int) original.getSize());
            check (restored.getPatchName (90) == p.getPatchName (0), "Old patch bank leaks into loaded session");
            check (! p.isPatchEditable (999) && ! p.isPatchEditable (-1), "Invalid preset index accepted");
        }
        {
            GrooveTapDelayAudioProcessor p;
            juce::AudioBuffer<float> b (2, 128);
            for (int patch = 0; patch < kNumFactorySlots; ++patch)
            {
                p.loadPatch (patch); p.prepareToPlay (44100, 128);
                for (int block = 0; block < 120; ++block)
                {
                    b.clear();
                    if (block == 0) { b.setSample (0, 0, 0.5f); b.setSample (1, 0, 0.5f); }
                    p.processBlock (b, midi);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i)
                        check (std::isfinite (b.getSample (c, i)), "Factory preset produced non-finite output");
                }
            }
        }
        {
            GrooveTapDelayAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            auto before = p.apvts.getRawParameterValue ("band2_enabled")->load();
            juce::Button* power = nullptr;
            for (auto* child : editor->getChildren())
                if (child->getTitle() == "Band 2 on/off") power = dynamic_cast<juce::Button*> (child);
            check (power != nullptr, "Band power button missing");
            power->setToggleState (before < 0.5f, juce::sendNotification);
            check (! juce::exactlyEqual (p.apvts.getRawParameterValue ("band2_enabled")->load(), before), "Band toggle is not attached");
            if (argc > 1)
            {
                juce::File file (juce::String::fromUTF8 (argv[1]));
                auto stream = file.createOutputStream(); check (stream != nullptr, "Cannot write UI snapshot");
                stream->setPosition (0); stream->truncate();
                juce::PNGImageFormat png;
                check (png.writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds()), *stream), "Snapshot failed");
                stream.reset();
                // Exercise minimum-size layout and preserve a second review image.
                editor->setSize (420, 280);
                auto smallStream = file.getSiblingFile (file.getFileNameWithoutExtension() + "-small.png").createOutputStream();
                check (smallStream != nullptr, "Cannot write minimum-size snapshot");
                smallStream->setPosition (0); smallStream->truncate();
                check (png.writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds()), *smallStream), "Small snapshot failed");
            }
        }
        std::cout << "PASS: parameter text entry, reset/tail, mono bypass, zero delay, feedback/filter bypass, series mute, muted-band decay, five sample rates, preset persistence, all 90 factory smoke checks, UI attachment\n";
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    return 0;
}
