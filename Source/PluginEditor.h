#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StringComponent.h"

//==================================================================
class StringUIdemoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StringUIdemoAudioProcessorEditor(StringUIdemoAudioProcessor&);
    ~StringUIdemoAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Colori delle corde (pubblico per usarlo nel paint)
    static juce::Colour stringColour(int index)
    {
        // E2=oro, A2=argento, D3=bronzo, G3=azzurro, B3=verde, E4=rosso
        const juce::Colour colours[6] = {
            juce::Colour(0xFFD4A017),
            juce::Colour(0xFFC0C0C0),
            juce::Colour(0xFFCD7F32),
            juce::Colour(0xFF4FC3F7),
            juce::Colour(0xFF81C784),
            juce::Colour(0xFFEF5350)
        };
        return colours[index % 6];
    }

private:

    int oldPosFret = -1;
    int oldMidiNote = -1;
 
    void mouseDown(const juce::MouseEvent& e) override { handleMouseEvent(e); }
    
    void mouseDrag(const juce::MouseEvent& e) override {
        handleMouseEvent(e);
    }

    void mouseUp(const juce::MouseEvent& e) override {
        
        //al rilascio del mouse azzero i parametri (-1 perchè 0 è una pos. del fret)
        oldPosFret = -1; 
        oldMidiNote = -1;
    }
    void handleMouseEvent(const juce::MouseEvent& e);

    StringUIdemoAudioProcessor& audioProcessor;

    juce::OwnedArray<StringComponent> stringComponents;

    //numero tasti del manico della chitarra
    const int numFret = 12;

    juce::Label notaSuonataLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringUIdemoAudioProcessorEditor)
};