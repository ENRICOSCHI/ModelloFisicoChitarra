#include "PluginProcessor.h"
#include "PluginEditor.h"

// Accordatura standard: E2=40, A2=45, D3=50, G3=55, B3=59, E4=64
const int StringUIdemoAudioProcessor::defaultMidiNotes[StringUIdemoAudioProcessor::numStrings]
= { 40, 45, 50, 55, 59, 64 };

//==============================================================================
StringUIdemoAudioProcessor::StringUIdemoAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    // Inizializza il tuning corrente con i valori di default
    for (int i = 0; i < numStrings; ++i)
        currentMidiNotes[i] = defaultMidiNotes[i];
}

StringUIdemoAudioProcessor::~StringUIdemoAudioProcessor() {}

//==============================================================================
void StringUIdemoAudioProcessor::pluckString(int stringIndex, float position)
{
    if (stringIndex < 0 || stringIndex >= stringSynths.size())
        return;

    const int numFrets = 12;
    int fret = juce::jlimit(0, numFrets, (int)(position * (float)numFrets));
    int midiNote = currentMidiNotes[stringIndex] + fret;

    double freqHz = juce::MidiMessage::getMidiNoteInHertz(midiNote);

    auto* synth = stringSynths.getUnchecked(stringIndex);
    synth->setFrequency(freqHz);
    synth->stringPlucked(position);
}

//==============================================================================
void StringUIdemoAudioProcessor::setStringMidiNote(int stringIndex, int newMidiNote)
{
    if (stringIndex < 0 || stringIndex >= numStrings)
        return;

    // Limita il tuning a ±tuningRangeSemitones rispetto al default
    int minNote = defaultMidiNotes[stringIndex] - tuningRangeSemitones;
    int maxNote = defaultMidiNotes[stringIndex] + tuningRangeSemitones;
    currentMidiNotes[stringIndex] = juce::jlimit(minNote, maxNote, newMidiNote);

    // Aggiorna subito la frequenza base del synth (senza pizzicare)
    if (stringIndex < stringSynths.size())
    {
        double freqHz = juce::MidiMessage::getMidiNoteInHertz(currentMidiNotes[stringIndex]);
        stringSynths.getUnchecked(stringIndex)->setFrequency(freqHz);
    }
}

int StringUIdemoAudioProcessor::getStringMidiNote(int stringIndex) const
{
    if (stringIndex >= 0 && stringIndex < numStrings)
        return currentMidiNotes[stringIndex];
    return 0;
}

void StringUIdemoAudioProcessor::resetTuning()
{
    for (int i = 0; i < numStrings; ++i)
        setStringMidiNote(i, defaultMidiNotes[i]);
}

//==============================================================================
const juce::String StringUIdemoAudioProcessor::getName() const { return JucePlugin_Name; }

bool StringUIdemoAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool StringUIdemoAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool StringUIdemoAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double StringUIdemoAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int StringUIdemoAudioProcessor::getNumPrograms() { return 1; }
int StringUIdemoAudioProcessor::getCurrentProgram() { return 0; }
void StringUIdemoAudioProcessor::setCurrentProgram(int) {}
const juce::String StringUIdemoAudioProcessor::getProgramName(int) { return {}; }
void StringUIdemoAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void StringUIdemoAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    stringSynths.clear();

    for (int i = 0; i < numStrings; ++i)
    {
        double freq = juce::MidiMessage::getMidiNoteInHertz(currentMidiNotes[i]);
        stringSynths.add(new StringSynthesiser(sampleRate, freq));
    }
}

void StringUIdemoAudioProcessor::releaseResources()
{
    stringSynths.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StringUIdemoAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}
#endif

void StringUIdemoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    float* channelData = buffer.getWritePointer(0);

    for (int i = 0; i < stringSynths.size(); ++i)
        stringSynths.getUnchecked(i)->generateAndAddData(channelData, buffer.getNumSamples());

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
bool StringUIdemoAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* StringUIdemoAudioProcessor::createEditor()
{
    return new StringUIdemoAudioProcessorEditor(*this);
}

void StringUIdemoAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void StringUIdemoAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StringUIdemoAudioProcessor();
}