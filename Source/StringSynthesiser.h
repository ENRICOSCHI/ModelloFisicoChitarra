#include <JuceHeader.h>

class StringSynthesiser
{
public:
    StringSynthesiser(double sampleRate, double frequencyInHz,float hardness) : fs(sampleRate),currentHardness(hardness)
    {
        doPluckForNextBuffer.set(false);
        maxDelayLength = (size_t)juce::roundToInt(sampleRate / 20.0);
        delayLine.resize(maxDelayLength, 0.0f);
        excitationSample.resize(maxDelayLength, 0.0f);
        setFrequency(frequencyInHz);
    }

    void SetHardness(float h) {
        currentHardness = juce::jlimit(0.01f, 1.0f, h);
    }

    void SetDamping(float d) {
        currentDamping = juce::jlimit(0.0f,1.0f,d);
    }

    void SetSustain(float s) {
        currentSustain = juce::jlimit(0.0f, 1.0f, s);
    }

    void stringPlucked(float pluckPosition)
    {
        if (doPluckForNextBuffer.compareAndSetBool(1, 0))
            amplitude = std::sin(juce::MathConstants<float>::pi * pluckPosition);
    }

    void generateAndAddData(float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool(0, 1))
            exciteInternalBuffer();

        //damping
        float dampCoeff = currentDamping * 0.5f;

        for (int i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % currentDelayLength;
            
            //filtraggio damping
            float filtered = delayLine[pos] * (1.0f - dampCoeff) + delayLine[nextPos] * dampCoeff;

            //sustain: feedback gain tra 0.9f e 0.099f
            float feedbackGain = 0.9f + currentSustain * 0.099f;

            delayLine[nextPos] = filtered * feedbackGain;

            outBuffer[i] += delayLine[pos];
            pos = nextPos;
        }
    }

    void setFrequency(double newFrequencyInHz)
    {
        currentDelayLength = (size_t)juce::roundToInt(fs / newFrequencyInHz);
        currentDelayLength = juce::jlimit((size_t)2, maxDelayLength, currentDelayLength);
        pos = 0; // reset posizione per evitare artefatti

        generateExcitation();
    }

private:

    /// <summary>
    /// Riempie excitationSample con un buffer di rumore che simula il colpo iniziale sulla corda 
    /// Il "burst" di energia che poi il delay line di Karplus-Strong fa rimbalzare in loop.
    /// </summary>
    void generateExcitation() {
        float lastSample = 0.0f;
        float maxVal = 0.0f;

        for (size_t i = 0; i < currentDelayLength; ++i)
        {
            //genera rumore bianco casuale tra -1 e 1 (ogni campione è indipendente dal precedente)
            float noise = (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f;
            //media pesata tra il campione nuovo e il precedente
            //dove 1 = plettro e 0 = dito
            //un po' come un filtro passa-basso di primo ordine
            //dove più l'hardness è bassa e più taglia gli acuti
            float shaped = noise * currentHardness + lastSample * (1.0f - currentHardness);
            excitationSample[i] = shaped;
            //tengo traccia del valore precedente (per il filtro)
            lastSample = shaped;
            //e del valore massimo raggiunto per la normalizzazione
            maxVal = std::max(maxVal, std::abs(shaped));
        }

        //normalizzo per evitare l'abbassamento di volume
        if (maxVal > 0.0f)
        {
            for (size_t i = 0; i < currentDelayLength; ++i)
                excitationSample[i] /= maxVal;//divido il buffer per il valore massimo così da avere il picco sempre a 1 
        }
    }

    void exciteInternalBuffer()
    {
        generateExcitation();
        for (size_t i = 0; i < currentDelayLength; ++i)
            delayLine[i] = excitationSample[i] * (float)amplitude;
    }

    double fs;
    size_t maxDelayLength;
    size_t currentDelayLength = 0;

    const double decay = 0.998;
    double amplitude = 0.0;

    float currentHardness = 0.5f;
    float currentDamping = 0.5f;
    float currentSustain = 0.8f;

    juce::Atomic<int> doPluckForNextBuffer;
    std::vector<float> excitationSample, delayLine;
    size_t pos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringSynthesiser)
};