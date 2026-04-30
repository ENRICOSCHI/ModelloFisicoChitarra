/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StringUIdemoAudioProcessorEditor::StringUIdemoAudioProcessorEditor (StringUIdemoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
    {
        auto* sc = stringComponents.add (new StringComponent (stringColour (i)));
        addAndMakeVisible (sc);
    }


    addAndMakeVisible(notaSuonataLabel);
    notaSuonataLabel.setText("negro",juce::NotificationType::dontSendNotification);

    // Dimensioni editor: abbastanza larghe, con spazio sopra per futura UI
    setSize (700, 400);
}

StringUIdemoAudioProcessorEditor::~StringUIdemoAudioProcessorEditor() {}

//==============================================================================
void StringUIdemoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Sfondo scuro stile amplificatore
    g.fillAll (juce::Colour (0xFF1A1A2E));

    // Titolo
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("String UI Demo - pizzica le corde!",
                0, 10, getWidth(), 30,
                juce::Justification::centred);

    // Linea separatrice sopra le corde
    auto stringsAreaY = getHeight() - (StringUIdemoAudioProcessor::numStrings * 30 + 16);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (stringsAreaY - 4, 10.0f, (float) getWidth() - 10.0f);

    // Etichette corde (da sinistra: E2 A2 D3 G3 B3 E4, ordine chitarra basso→alto)
    const char* labels[] = { "E2", "A2", "D3", "G3", "B3", "E4" };
    g.setFont (juce::FontOptions (11.0f));

    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
    {
        auto* sc = stringComponents.getUnchecked (i);
        g.setColour (stringColour (i));
        g.drawText (labels[i],
                    sc->getX() - 30, sc->getY(),
                    28, sc->getHeight(),
                    juce::Justification::centredRight);
    }
}

void StringUIdemoAudioProcessorEditor::resized()
{
    //
    // Le 6 corde occupano la parte BASSA dell'editor, come i tasti di una pianola.
    // Tutte della stessa lunghezza (come su una chitarra fisica).
    //
    const int margin      = 40;  // margine sinistro per le etichette
    const int rightMargin = 10;
    const int stringH     = 24;  // altezza di ogni riga-corda
    const int gap         =  6;  // spazio tra una corda e l'altra
    const int totalStrings = StringUIdemoAudioProcessor::numStrings;

    const int stringsAreaH = totalStrings * stringH + (totalStrings - 1) * gap + 16;
    const int startY       = getHeight() - stringsAreaH;
    const int stringWidth  = getWidth() - margin - rightMargin;

    for (int i = 0; i < totalStrings; ++i)
    {
        int y = startY + i * (stringH + gap) + 8;
        stringComponents.getUnchecked (i)->setBounds (margin, y, stringWidth, stringH);
    }

    notaSuonataLabel.setBounds(350, 100, getWidth(), 100);
}

//==============================================================================
void StringUIdemoAudioProcessorEditor::handleMouseEvent (const juce::MouseEvent& e)
{
    for (int i = 0; i < stringComponents.size(); ++i)
    {
        auto* sc = stringComponents.getUnchecked (i);

        if (sc->getBounds().contains (e.getPosition()))
        {
            float relPos = (e.position.x - (float) sc->getX()) / (float) sc->getWidth();
            relPos = juce::jlimit (0.0f, 1.0f, relPos);

            /*Gestisco label nota*/
            int posFret = juce::jlimit(0, numFret, (int)(relPos * numFret));

            int midiNote = StringUIdemoAudioProcessor::guitarMidiNotes[i] + posFret;

            //controllo se la posione del fret e della nota precedente sono diversi
            //se almeno uno dei due è diverso posso suonare la nota
            if (oldPosFret != posFret || oldMidiNote != midiNote) {

                //aggiorno posizione del fret
                oldPosFret = posFret;

                //aggiorno la nota midi
                oldMidiNote = midiNote;

                juce::String nomeNota = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3);

                notaSuonataLabel.setText("Nota: " + nomeNota + " Tasto: " + (juce::String)posFret, juce::dontSendNotification);

                sc->stringPlucked(relPos);
                audioProcessor.pluckString(i, relPos);
                break;
            }
            
        }
    }
}