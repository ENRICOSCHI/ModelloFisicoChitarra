#pragma once

#include <JuceHeader.h>
#include "StringSynthesiser.h"
#include <atomic>

//==============================================================================
class StringUIdemoAudioProcessor : public juce::AudioProcessor
{
public:
    static const int numStrings = 6;

    // Accordatura di default (E2 A2 D3 G3 B3 E4) - usata come riferimento iniziale
    static const int defaultMidiNotes[numStrings];

    // Range MIDI consentito per il tuning (±12 semitoni rispetto al default)
    static const int tuningRangeSemitones = 12;

    #pragma region Variabile APVTS per controllo Effettistica (UI)

    juce::AudioProcessorValueTreeState apvts;

    #pragma endregion

    #pragma region Variabili Atomic

	// Atomic per gestire in maniera thread-safe le modifiche alla UI da parte del processBlock
    std::atomic<bool> uiStringWasPlucked[numStrings];
	std::atomic<float> uiPluckPosition[numStrings];

    #pragma endregion



    StringUIdemoAudioProcessor();
    ~StringUIdemoAudioProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // Puntatore all'oscilloscopio, usato per visualizzare le onde sonore
    juce::AudioVisualiserComponent* puntatoreOscilloscopio = nullptr;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // Pizzica la corda (la frequenza viene calcolata da currentMidiNotes + fret)
    void pluckString(int stringIndex, float position);

    // Tuning: imposta la nota MIDI base per la corda stringIndex
    void setStringMidiNote(int stringIndex, int newMidiNote);

    // Restituisce la nota MIDI base corrente per la corda stringIndex
    int getStringMidiNote(int stringIndex) const;

    // Reimposta tutte le corde all'accordatura di default
    void resetTuning();

private:

    #pragma region Parametri per Effettistica (UI)

    // Funzione per creare i parametri da gestire con l'APVTS
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

	// Puntatori atomici (thread-safe) per i parametri di controllo dell'effettistica
	std::atomic<float>* driveParameter = nullptr;
    std::atomic<float>* gainParameter = nullptr;

    #pragma endregion


    // Nota MIDI base per ciascuna corda (modificabile dal tuning UI)
    int currentMidiNotes[numStrings];

    juce::OwnedArray<StringSynthesiser> stringSynths;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringUIdemoAudioProcessor)
};