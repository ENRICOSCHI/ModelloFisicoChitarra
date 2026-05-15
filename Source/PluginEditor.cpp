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
    notaSuonataLabel.setText("Nota", juce::NotificationType::dontSendNotification);
    notaSuonataLabel.setFont(juce::FontOptions(13.0f));
    notaSuonataLabel.setColour(juce::Label::textColourId, juce::Colours::white);

	// --- Manopole ---
    for (int i = 0; i < numManopole; ++i)
    {
        // Setup Manopola
        manopolaEffetto[i].setSliderStyle(juce::Slider::Rotary);
        manopolaEffetto[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 15);

        // Nascondo la box, lasciando solamente il testo
        manopolaEffetto[i].setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        manopolaEffetto[i].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        manopolaEffetto[i].setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);

        manopolaEffetto[i].setNumDecimalPlacesToDisplay(2);
        manopolaEffetto[i].setRange(0.0f, 1.0f);
        manopolaEffetto[i].setValue(0.5f);
        manopolaEffetto[i].setLookAndFeel(&stilePomello); // Usiamo lo stesso stile per tutte
        addAndMakeVisible(manopolaEffetto[i]);

        // Setup Titolo
        titoloManopolaEffetto[i].setText("Effetto " + juce::String(i + 1), juce::dontSendNotification);
        titoloManopolaEffetto[i].setJustificationType(juce::Justification::centred);
        titoloManopolaEffetto[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titoloManopolaEffetto[i]);
    }

    // Inizializza tutte le label di tuning con i valori correnti
    updateAllTuningLabels();

    setSize(1280, 720);

    // Rende la finestra ridimensionabile (questo è possibile grazie ai LocalBounds settati in precedenza)
    setResizable(true, true);

    // Limiti di dimensione della finestra
    setResizeLimits(1280, 720, 1500, 840);

    // Blocco delle proporzioni (Così si scala solo in obliquo)
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(1280.0 / 720.0);

    #pragma region Timer

	// Avvio il timer per controllare le interazioni Audio Thread -> UI Thread (per la MIDI)
	startTimerHz(60); // Timer che scade 60 volte al secondo (ogni ~16ms)

    #pragma endregion

    #pragma region Attachments
    
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.apvts, "drive", manopolaEffettoDrive);

	gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.apvts, "gain", manopolaEffettoGain);

    #pragma endregion
}

StringUIdemoAudioProcessorEditor::~StringUIdemoAudioProcessorEditor() 
{
	audioProcessor.puntatoreOscilloscopio = nullptr; // Rimuovo il puntatore all'oscilloscopio dal processor (good practice)
	stopTimer(); // Ferma il timer quando l'editor viene distrutto (good practice)
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

#pragma region paint UI

void StringUIdemoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF201513));

    SetTitle(g);
    SetLineaSeparatrice(g);
    SetStrings(g);
    SetSeparationFret(g);

    // Disegno delle aree
    
    // Imposta un colore semitrasparente per evidenziare le aree
	g.setColour(juce::Colours::white.withAlpha(0.2f));

    // Disegna un bordo
    g.drawRect(areaParametriSinistra.reduced(4), 2.0f);
	g.drawRect(areaEffettiDestra.reduced(4), 2.0f);


}

#pragma endregion

#pragma region resized UI

void StringUIdemoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Calcola quanto la finestra è stata ingrandita o rimpicciolita
    float scale = (float)getWidth() / 750.0f;

    #pragma region Area corde scalata
        // --- AREA CORDE SCALATA ---
        int stringH = 17 * scale;
        int gap = 4 * scale;
        int rightMargin = 10 * scale;
        int scaledTuningPanelWidth = tuningPanelWidth * scale; // Scala il tuo pannello
        const int totalStrings = StringUIdemoAudioProcessor::numStrings;

        int stringsAreaH = totalStrings * stringH + (totalStrings - 1) * gap + (16 * scale);

        auto bottomArea = area.removeFromBottom(stringsAreaH);
        areaCordeSotto = bottomArea;
    

        resetTuningButton.setBounds(4 * scale, bottomArea.getY() - (18 * scale), scaledTuningPanelWidth - (65 * scale), 13 * scale);
        bottomArea.removeFromTop(8 * scale);

        for (int i = 0; i < totalStrings; ++i)
        {
            // Separazione della zona inferiore
            auto row = bottomArea.removeFromTop(stringH);
            bottomArea.removeFromTop(gap);

            auto tuningRow = row.removeFromLeft(scaledTuningPanelWidth);
            row.removeFromRight(rightMargin);
            stringComponents.getUnchecked(i)->setBounds(row);

            int btnW = 22 * scale;
            int lblW = 44 * scale;

            tuningRow.removeFromLeft(4 * scale);
            tuningDownButtons.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(btnW).reduced(0, 1));

            tuningRow.removeFromLeft(2 * scale);
            tuningLabels.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(lblW).reduced(0, 1));

		    // Scala la grandezza del testo delle label di tuning
            tuningLabels.getUnchecked(i)->setFont(juce::FontOptions(11.0f * scale, juce::Font::bold));

            tuningRow.removeFromLeft(4 * scale);
            tuningUpButtons.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(btnW).reduced(0, 1));
        }
    #pragma endregion

    #pragma region Area superiore scalata
        // --- AREA SUPERIORE SCALATA ---
        area.removeFromTop(30 * scale);
        auto notaSuonataArea = area.removeFromBottom(19 * scale);
        notaSuonataLabel.setBounds(notaSuonataArea.withSizeKeepingCentre(538 * scale, 20 * scale).translated(0, -2 * scale));
        notaSuonataLabel.setFont(juce::FontOptions(10.0f * scale));

        auto leftParamsArea = area.removeFromLeft(area.getWidth() / 2);
        auto rightEffectsArea = area;

        areaParametriSinistra = leftParamsArea;
        areaEffettiDestra = rightEffectsArea;
    #pragma endregion

    #pragma region Griglia manopole scalata
        // --- GRIGLIA MANOPOLE SCALATA ---
        auto workArea = rightEffectsArea.reduced(10 * scale);
        auto topRow = workArea.removeFromTop(workArea.getHeight() / 2);
        auto bottomRow = workArea;

        juce::Rectangle<int> celleManopole[4];
        celleManopole[0] = topRow.removeFromLeft(topRow.getWidth() / 2);
        celleManopole[1] = topRow;
        celleManopole[2] = bottomRow.removeFromLeft(bottomRow.getWidth() / 2);
        celleManopole[3] = bottomRow;

        for (int i = 0; i < numManopole; ++i)
        {
            // Visto che la cella mantiene sempre la stessa proporzione, 
            // la manopola occupa il 75% dello spazio della cella
            int dynamicKnobSize = celleManopole[i].getWidth() * 0.35f;

            auto manopolaBounds = celleManopole[i].withSizeKeepingCentre(dynamicKnobSize, dynamicKnobSize);
            manopolaEffetto[i].setBounds(manopolaBounds);

            // Scala il testo dentro lo slider della manopola
            manopolaEffetto[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 25 * scale, 10 * scale);

            titoloManopolaEffetto[i].setBounds(manopolaBounds.getX(), manopolaBounds.getY() - (12 * scale), dynamicKnobSize, 20 * scale);
            // Scala il font dei titoli
            titoloManopolaEffetto[i].setFont(juce::FontOptions(10.0f * scale));
        }
    #pragma endregion

    #pragma region Sezione oscilloscopio
        //--- SETUP OSCILLOSCOPIO ---
        // Parametri dell'onda visualizzata
        oscilloscopio.setBufferSize(1024);
        oscilloscopio.setSamplesPerBlock(128);
        oscilloscopio.setRepaintRate(120); // Aggiornamento a 120 FPS

        audioProcessor.puntatoreOscilloscopio = &oscilloscopio;

        // Colori: Sfondo e Colore dell'Onda
        oscilloscopio.setColours(juce::Colour(0xFF201513), juce::Colours::cyan);
        oscilloscopio.setOpaque(true); // Per migliorare le prestazioni, dato che disegniamo tutto lo sfondo
        addAndMakeVisible(oscilloscopio);

        // Posizionamento dell'oscilloscopio
        oscilloscopio.setBounds(areaParametriSinistra.reduced(10 * scale));
    #pragma endregion
}

#pragma endregion

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
    float scale = (float)getWidth() / 750.0f;

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    // Scaliamo il font del titolo (da 18 a 18 * scale)
    g.setFont(juce::FontOptions(18.0f * scale, juce::Font::bold));

    g.drawText("String UI Demo - pizzica le corde!",
        0, 5 * scale, getWidth(), 30 * scale, juce::Justification::centred);
}

void StringUIdemoAudioProcessorEditor::SetLineaSeparatrice(juce::Graphics& g)
{
    // Usiamo il bordo dell'areaCordeSotto calcolata nel resized! È molto più sicuro.
    float lineaY = areaCordeSotto.getY() - 4.0f;
    g.setColour(juce::Colour(0xFF4D453A));
    g.drawHorizontalLine((int)lineaY, 10.0f, (float)getWidth() - 10.0f);
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