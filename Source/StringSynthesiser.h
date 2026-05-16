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
        currentHardness = juce::jlimit(0.0f, 1.0f, h);
    }

    void stringPlucked(float pluckPosition)
    {
        jassert(pluckPosition >= 0.0f && pluckPosition <= 1.0f);
        if (doPluckForNextBuffer.compareAndSetBool(1, 0))
            amplitude = std::sin(juce::MathConstants<float>::pi * pluckPosition);
    }

    void generateAndAddData(float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool(0, 1))
            exciteInternalBuffer();

        for (int i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % currentDelayLength;
            delayLine[nextPos] = (float)(decay * 0.5 * (delayLine[nextPos] + delayLine[pos]));
            outBuffer[i] += delayLine[pos];
            pos = nextPos;
        }
    }

    void setFrequency(double newFrequencyInHz)
    {
        currentDelayLength = (size_t)juce::roundToInt(fs / newFrequencyInHz);
        currentDelayLength = juce::jlimit((size_t)2, maxDelayLength, currentDelayLength);
        pos = 0; // reset posizione per evitare artefatti

        /*std::generate(excitationSample.begin(),
            excitationSample.begin() + currentDelayLength,
            [] { return (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f; });*/

        generateExcitation();
    }

private:

    void generateExcitation() {
        float lastSample = 0.0f;
        float maxVal = 0.0f;

        for (size_t i = 0; i < currentDelayLength; ++i)
        {
            float noise = (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f;
            float shaped = noise * currentHardness + lastSample * (1.0f - currentHardness);
            excitationSample[i] = shaped;
            lastSample = shaped;
            maxVal = std::max(maxVal, std::abs(shaped));
        }

        // Normalizza: riporta sempre il picco a 1.0 indipendentemente dall'hardness
        if (maxVal > 0.0f)
        {
            for (size_t i = 0; i < currentDelayLength; ++i)
                excitationSample[i] /= maxVal;
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

    juce::Atomic<int> doPluckForNextBuffer;
    std::vector<float> excitationSample, delayLine;
    size_t pos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringSynthesiser)
};