#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StringUIdemoAudioProcessorEditor::StringUIdemoAudioProcessorEditor(StringUIdemoAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    
    // --- Corde visive ---
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
    {
        auto* sc = stringComponents.add(new StringComponent(stringColour(i)));
        addAndMakeVisible(sc);
    }

    // --- Controlli tuning (uno per corda) ---
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
    {
        // Pulsante [−]
        auto* btnDown = tuningDownButtons.add(new juce::TextButton("-"));
        // quando premo il pulsante...
        btnDown->onClick = [this, i]()
            {
                int current = audioProcessor.getStringMidiNote(i);
                audioProcessor.setStringMidiNote(i, current - 1);
                updateTuningLabel(i);
            };
        addAndMakeVisible(btnDown);

        // Label con nome nota + delta semitoni
        auto* lbl = tuningLabels.add(new juce::Label());
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        lbl->setColour(juce::Label::textColourId, stringColour(i));
        addAndMakeVisible(lbl);

        // Pulsante [+]
        auto* btnUp = tuningUpButtons.add(new juce::TextButton("+"));
        //quando premo il pulsante...
        btnUp->onClick = [this, i]()
            {
                int current = audioProcessor.getStringMidiNote(i);
                audioProcessor.setStringMidiNote(i, current + 1);
                updateTuningLabel(i);
            };
        addAndMakeVisible(btnUp);

    }  

    // --- Pulsante Reset ---
    resetTuningButton.setButtonText("Reset");
    resetTuningButton.onClick = [this]()
        {
            audioProcessor.resetTuning();
            updateAllTuningLabels();
        };
    addAndMakeVisible(resetTuningButton);

    // --- Label nota suonata ---
    addAndMakeVisible(notaSuonataLabel);
    notaSuonataLabel.setText("negro", juce::NotificationType::dontSendNotification);
    notaSuonataLabel.setFont(juce::FontOptions(13.0f));
    notaSuonataLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    //manopola
    manopolaEffetto.setSliderStyle((juce::Slider::Rotary)); //faccio diventare lo slide un cerchio
    manopolaEffetto.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20); //metto il testo sotto lo slider
    manopolaEffetto.setNumDecimalPlacesToDisplay(2);//imposto la visione del decimale fino al 0.00
    manopolaEffetto.setRange(0.0f, 1.0f); //range da 0 a 1
    manopolaEffetto.setValue(0.5f); //valore iniziale a 0.5
    manopolaEffetto.setLookAndFeel(&stilePomello); //imposto lo stile del pomello
    addAndMakeVisible(manopolaEffetto);
    //titolo manopola
    titoloManopoloEffeto.setText("Effetto Negro", juce::dontSendNotification);
    titoloManopoloEffeto.setJustificationType(juce::Justification::centred);
    titoloManopoloEffeto.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titoloManopoloEffeto);

    // Inizializza tutte le label di tuning con i valori correnti
    updateAllTuningLabels();

    setSize(750, 420);

	// Avvio il timer per controllare le interazioni Audio Thread -> UI Thread (per la MIDI)
	startTimerHz(60); // Timer che scade 60 volte al secondo (ogni ~16ms)
}

StringUIdemoAudioProcessorEditor::~StringUIdemoAudioProcessorEditor() 
{
	stopTimer(); // Ferma il timer quando l'editor viene distrutto | good practice.
}

//==============================================================================

// Override del Callback del timer come specificato in PluginEditor.h
void StringUIdemoAudioProcessorEditor::timerCallback() 
{
    // Controllo per ogni corda se la rispettiva flag è stata alzata dall'Audio Thread (processBlock)
	// Tutto tramite Polling dell'atomic <bool> uiStringWasPlucked[numStrings]
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i) 
    {
        // Check della flag, utilizzando il valore attuale e portandola a false
        if (audioProcessor.uiStringWasPlucked[i].exchange(false)) 
        {
            float position = audioProcessor.uiPluckPosition[i].load(); // Posizione del pizzico
			stringComponents.getUnchecked(i)->stringPlucked(position); // Aggiorna la visualizzazione della corda

            #pragma region Aggiornamento label nota suonata

            // Calcolo la fret della posizione relativa [0.0, 1.0]
			int fret = juce::jlimit(0, numFret, (int)(position * numFret));

			// Recupero la nota MIDI base della corda e vi sommo la fret per la nota MIDI effettiva suonata
			int midiNote = audioProcessor.getStringMidiNote(i) + fret;

            // Per il nome della nota riprendo la logica usata per esempio in HandleMouseEvent:
            // (sempre attraverso la utility di JUCE)
			juce::String nomeNota = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3);

            // Aggiorno la label di conseguenza
            notaSuonataLabel.setText("Nota: " + nomeNota + "  Tasto: " + juce::String(fret),
				juce::dontSendNotification);

            #pragma endregion
        }
    }

}

//==============================================================================
void StringUIdemoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF201513));

    SetTitle(g);
    SetLineaSeparatrice(g);
    SetStrings(g);
    SetSeparationFret(g);
}

void StringUIdemoAudioProcessorEditor::resized()
{
    // --- Dimensioni area corde ---
    const int stringH = 24;
    const int gap = 6;
    const int totalStrings = StringUIdemoAudioProcessor::numStrings;
    const int rightMargin = 10;

    const int stringsAreaH = totalStrings * stringH + (totalStrings - 1) * gap + 16;
    const int startY = getHeight() - stringsAreaH;
    const int stringWidth = getWidth() - tuningPanelWidth - rightMargin;

    for (int i = 0; i < totalStrings; ++i)
    {
        int y = startY + i * (stringH + gap) + 8;

        // Corda visiva: parte dopo il pannello tuning
        stringComponents.getUnchecked(i)->setBounds(tuningPanelWidth, y, stringWidth, stringH);

        // Layout pannello tuning per la corda i-esima:
        // [−](22) [Label(44)] [+](22) — con 4px di gap tra elementi, 4px padding sx
        int px = 4;
        int btnW = 22;
        int lblW = 44;
        int btnH = stringH - 2;
        int elemY = y + 1;

        tuningDownButtons.getUnchecked(i)->setBounds(px, elemY, btnW, btnH);
        tuningLabels.getUnchecked(i)->setBounds(px + btnW + 2, elemY, lblW, btnH);
        tuningUpButtons.getUnchecked(i)->setBounds(px + btnW + lblW + 4, elemY, btnW, btnH);
    }

    // Pulsante Reset: sopra il pannello tuning
    resetTuningButton.setBounds(4, startY - 28, tuningPanelWidth - 8, 22);

    // Label nota suonata: sopra la parte per suonare le corde
    notaSuonataLabel.setBounds(350,200, getWidth() - tuningPanelWidth - 20, 24);

    //titolo manopola
    titoloManopoloEffeto.setBounds(120, 35, 120, 20);
    
    //manopola
    manopolaEffetto.setBounds(120, 50, 120, 120);
}

//==============================================================================
void StringUIdemoAudioProcessorEditor::handleMouseEvent(const juce::MouseEvent& e)
{
    for (int i = 0; i < stringComponents.size(); ++i)
    {
        auto* sc = stringComponents.getUnchecked(i);

        if (sc->getBounds().contains(e.getPosition()))
        {
            float relPos = (e.position.x - (float)sc->getX()) / (float)sc->getWidth();
            relPos = juce::jlimit(0.0f, 1.0f, relPos);

            int posFret = juce::jlimit(0, numFret, (int)(relPos * numFret));

            // La nota suonata dipende dalla nota base CORRENTE (accordatura attuale)
            int baseMidi = audioProcessor.getStringMidiNote(i);
            int midiNote = baseMidi + posFret;

            if (oldPosFret != posFret || oldMidiNote != midiNote)
            {
                oldPosFret = posFret;
                oldMidiNote = midiNote;

                juce::String nomeNota = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3);
                notaSuonataLabel.setText("Nota: " + nomeNota + "  Tasto: " + juce::String(posFret),
                    juce::dontSendNotification);

                sc->stringPlucked(relPos);
                audioProcessor.pluckString(i, relPos);
                break;
            }
        }
    }
}

//==============================================================================

/// <summary>
/// Tuning label: mostra nome nota base + delta in semitoni rispetto al default.
/// Es.: "E2 (+2)" oppure "E2 (-1)" oppure "E2"
/// </summary>
/// <param name="stringIndex"></param>
void StringUIdemoAudioProcessorEditor::updateTuningLabel(int stringIndex)
{
    if (stringIndex < 0 || stringIndex >= StringUIdemoAudioProcessor::numStrings)
        return;

    int currentNote = audioProcessor.getStringMidiNote(stringIndex);
    int defaultNote = StringUIdemoAudioProcessor::defaultMidiNotes[stringIndex];
    int delta = currentNote - defaultNote;

    // Nome della nota base (senza numero di ottava nei pulsanti, con ottava nella label)
    juce::String noteName = juce::MidiMessage::getMidiNoteName(currentNote, true, true, 3);

    juce::String deltaStr;
    if (delta > 0) deltaStr = "(+" + juce::String(delta) + ")";
    else if (delta < 0) deltaStr = "(" + juce::String(delta) + ")";
    // se delta == 0 non mostriamo nulla

    juce::String labelText = noteName;
    if (deltaStr.isNotEmpty())
        labelText += " " + deltaStr;

    tuningLabels.getUnchecked(stringIndex)->setText(labelText, juce::dontSendNotification);
}

void StringUIdemoAudioProcessorEditor::updateAllTuningLabels()
{
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
        updateTuningLabel(i);
}

//==============================================================================
void StringUIdemoAudioProcessorEditor::SetTitle(juce::Graphics& g)
{
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("String UI Demo - pizzica le corde!",
        0, 10, getWidth(), 30, juce::Justification::centred);
}

void StringUIdemoAudioProcessorEditor::SetLineaSeparatrice(juce::Graphics& g)
{
    auto stringsAreaY = getHeight() - (StringUIdemoAudioProcessor::numStrings * 30 + 16);
    g.setColour(juce::Colour(0xFF4D453A));
    g.drawHorizontalLine(stringsAreaY - 4, 10.0f, (float)getWidth() - 10.0f);
}

void StringUIdemoAudioProcessorEditor::SetStrings(juce::Graphics& g)
{
    // Le etichette testuali (E2, A2...) vengono mostrate dalla tuningLabel,
    // quindi qui disegniamo solo una eventuale riga di sfondo per le corde
    // (opzionale, puoi rimuoverlo)
    for (int i = 0; i < StringUIdemoAudioProcessor::numStrings; ++i)
    {
        auto* sc = stringComponents.getUnchecked(i);
        g.setColour(stringColour(i).withAlpha(0.08f));
        g.fillRect(sc->getX(), sc->getY(), sc->getWidth(), sc->getHeight());
    }
}

void StringUIdemoAudioProcessorEditor::SetSeparationFret(juce::Graphics& g)
{
    auto* sc = stringComponents.getUnchecked(0);

    float startX = (float)sc->getX();
    float width = (float)sc->getWidth();
    float fretW = width / (float)numFret;

    g.setColour(juce::Colour(0xFFA88CEC));

    for (int i = 0; i <= numFret; ++i)
    {
        float x = startX + i * fretW;
        g.fillRect((int)x, sc->getY(), 3, sc->getHeight() * (numCorde + 1));
    }
}