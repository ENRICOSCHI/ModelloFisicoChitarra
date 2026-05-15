#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StringComponent.h"
#include "KnobStyle.h"

//==============================================================================
class StringUIdemoAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit StringUIdemoAudioProcessorEditor(StringUIdemoAudioProcessor&);
    ~StringUIdemoAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Colori delle corde (verde scalare, basso → alto)
    static juce::Colour stringColour(int index)
    {
        const juce::Colour colours[6] = {
            juce::Colour(0xFF008000),  // E2
            juce::Colour(0xFF00CF00),  // A2
            juce::Colour(0xFF1FFF1F),  // D3
            juce::Colour(0xFF6FFF6F),  // G3
            juce::Colour(0xFF96FF96),  // B3
            juce::Colour(0xFFCCFFCC)   // E4
        };
        return colours[index % 6];
    }

private:

	// --- Manopole e relative label (drive + gain) ---
	KnobStyle stilePomello;
	juce::Label titoloManopoloEffetoDrive;
	juce::Label titoloManopoloEffetoGain;
	juce::Slider manopolaEffettoDrive;
	juce::Slider manopolaEffettoGain;

	// --- Attachment per collegamenti con APVTS (Componente UI <-> Parametro) ---
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

	// --- Callback del Timer --- (Per interazione Audio Thread -> UI Thread per la MIDI)
	void timerCallback() override;
    /* Permette di controllare periodicamente 
    se l'Audio Thread ha flaggato una corda come suonata.*/

    // --- Mouse ---
    void mouseDown(const juce::MouseEvent& e) override { handleMouseEvent(e); }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseEvent(e); }
    void mouseUp(const juce::MouseEvent& e) override { oldPosFret = -1; oldMidiNote = -1; }
    void handleMouseEvent(const juce::MouseEvent& e);

    // --- Paint helpers ---
    void SetTitle(juce::Graphics&);
    void SetLineaSeparatrice(juce::Graphics&);
    void SetStrings(juce::Graphics&);
    void SetSeparationFret(juce::Graphics&);

    // --- Tuning helpers ---
    // Aggiorna la label di tuning per la corda i (mostra nome nota + delta semitoni)
    void updateTuningLabel(int stringIndex);

    // Aggiorna tutte le label di tuning
    void updateAllTuningLabels();

    // --- Dati ---
    StringUIdemoAudioProcessor& audioProcessor;

    juce::OwnedArray<StringComponent> stringComponents;

    // Controlli tuning: per ogni corda un pulsante "-", una label, un pulsante "+"
    juce::OwnedArray<juce::TextButton> tuningDownButtons;  // [−]
    juce::OwnedArray<juce::TextButton> tuningUpButtons;    // [+]
    juce::OwnedArray<juce::Label>      tuningLabels;       // "E2 (+0)"

    // Pulsante reset accordatura
    juce::TextButton resetTuningButton;

    // Label nota suonata corrente
    juce::Label notaSuonataLabel;

    // Costanti layout
    const int numFret = 12;
    const int numCorde = 6;

    // Larghezza della colonna tuning a sinistra delle corde
    // Layout: [−](22) [Label nota(50)] [+](22) gap(8) → totale 102
    static constexpr int tuningPanelWidth = 110;

    // Stato mouse (evita retriggering sulla stessa posizione)
    int oldPosFret = -1;
    int oldMidiNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringUIdemoAudioProcessorEditor)
};