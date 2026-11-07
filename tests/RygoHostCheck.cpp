// Loads a built plugin the way a DAW does, through its real AU or VST3 wrapper, and plays it.
//   RygoHostCheck <path to .component or .vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! condition)
        ++failures;
}

double heardSeconds (juce::AudioPluginInstance& plugin, int note, double seconds, double rate)
{
    const auto blockSize = 512;
    const auto blocks = (int) (seconds * rate / blockSize);
    juce::AudioBuffer<float> buffer (juce::jmax (2, plugin.getTotalNumOutputChannels()), blockSize);
    double lastSound = -1.0;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
        if (block == blocks - 2)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        plugin.processBlock (buffer, midi);

        if (buffer.getMagnitude (0, blockSize) > 0.001f)
            lastSound = (block + 1) * blockSize / rate;
    }
    return lastSound;
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::cerr << "usage: RygoHostCheck <plugin path>" << std::endl;
        return 2;
    }

    const auto path = juce::String::fromUTF8 (argv[1]);
    std::cout << "\n" << path << std::endl;

    juce::AudioPluginFormatManager manager;
    juce::addDefaultFormatsToManager (manager);

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::String error;

    for (auto* format : manager.getFormats())
    {
        juce::OwnedArray<juce::PluginDescription> types;
        format->findAllTypesForFile (types, path);

        if (types.isEmpty())
            continue;

        std::cout << "  format " << format->getName() << ": " << types[0]->manufacturerName << " / " << types[0]->name
                  << ", instrument: " << (types[0]->isInstrument ? "yes" : "no") << std::endl;

        check (types[0]->manufacturerName == "ZHRI Records", "the developer is ZHRI Records");
        check (types[0]->name == "Rygostancija", "the plugin is called Rygostancija");
        check (types[0]->isInstrument, "it loads as an instrument");

        plugin = manager.createPluginInstance (*types[0], 48000.0, 512, error);
        break;
    }

    if (plugin == nullptr)
    {
        check (false, "plugin could not be loaded: " + error);
        return 1;
    }

    plugin->setNonRealtime (true);
    plugin->prepareToPlay (48000.0, 512);

    // This test runs far faster than real time, while the samples load on a background thread.
    // A hosted AU isn't always told it's offline, so give the load the moment a real session would have.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (3000);

    check (plugin->getParameters().size() >= 36, "parameters are visible to the host: " + juce::String (plugin->getParameters().size()));

    // Lowest key of the full bank: the longest sample. Highest: the shortest. This also proves
    // that the samples were found inside the installed bundle.
    const auto longest = heardSeconds (*plugin, 0, 7.0, 48000.0);
    const auto shortest = heardSeconds (*plugin, 120, 2.0, 48000.0);
    check (longest > 5.0, "lowest key plays the longest sample: " + juce::String (longest, 2) + " s");
    check (shortest > 0.0 && shortest < 0.3, "highest key plays the shortest sample: " + juce::String (shortest, 2) + " s");

    juce::AudioProcessorParameter* found = nullptr;
    for (auto* parameter : plugin->getParameters())
        if (parameter->getName (64) == "Sustain")
            found = parameter;

    check (found != nullptr, "the Sustain parameter is found by name");

    if (found != nullptr)
    {
        found->setValueNotifyingHost (0.5f);
        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        found->setValueNotifyingHost (1.0f);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        check (std::abs (found->getValue() - 0.5f) < 0.01f, "the host sees saved settings after restoring a project");
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
        check (editor != nullptr && editor->getWidth() > 600 && editor->getHeight() > 400,
               "the plugin window opens: " + (editor != nullptr ? juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) : juce::String ("none")));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    }

    plugin->releaseResources();
    plugin.reset();

    std::cout << (failures == 0 ? "  all good" : "  FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
