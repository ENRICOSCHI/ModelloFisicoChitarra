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
    // Inizializzazioni utili...
	for (int i = 0; i < numStrings; ++i) 
    {
        // Inizializzo il tuning corrente con i valori di default
		currentMidiNotes[i] = defaultMidiNotes[i];

        // Inizializzo gli atomic per la UI
		uiStringWasPlucked[i].store(false);
		uiPluckPosition[i].store(0.0f);
	}

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

/// <summary>
/// Imposto la stringa accordata a fino a un max e min di 12.
/// Fa tre cose:
/// 1) Controllo indice valido.
/// 2) Limito il tuning.
/// 3) Aggiorno frequenze nel StringSynthesiser.h
/// </summary>
/// <param name="stringIndex"></param>
/// <param name="newMidiNote"></param>
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
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Gestione dei messaggi MIDI in arrivo
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int midiNote = message.getNoteNumber();
            float velocity = message.getFloatVelocity(); // Forza della plettrata

            // Troviamo quale corda deve suonare questa nota.
            // Una logica semplice: la prima corda che può coprire questa nota entro 12 tasti.
            for (int i = 0; i < numStrings; ++i)
            {
                int openStringNote = currentMidiNotes[i];
                int fret = midiNote - openStringNote;

                if (fret >= 0 && fret <= 12) // Se la nota è suonabile su questa corda
                {
                    // Calcoliamo la posizione "virtuale" del tasto (0.0 a 1.0)
                    // per farla digerire alla tua funzione pluckString
                    float position = (float)fret / 12.0f;

                    pluckString(i, position);

                    // Comunico tramite le variabili atomic (maniera thread-safe) alla UI
					// che la corda i è stata pizzicata e qual è la posizione del tasto.
                    uiPluckPosition[i].store(position);
					uiStringWasPlucked[i].store(true);

                    // Se vogliamo che una nota MIDI attivi solo una corda, usiamo break.
                    // Altrimenti, se la nota è presente su più corde (es. Mi), suonerebbero tutte.
                    break;
                }
            }
        }
        else if (message.isNoteOff())
        {
            // Opzionale: gestire il rilascio se il tuo StringSynthesiser lo supporta.
            // Molti modelli fisici di corde "lasciano suonare" finché l'energia non decade.
        }
    }

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