#include "FrequencyProblemDetector.h"
#include <algorithm>
#include <cmath>

void FrequencyProblemDetector::analyze(const float* magnitudesDb, int numBins,
                                        double sampleRate, float peakSample)
{
    problems.clear();

    if (numBins < 4) return;

    // ---- Clipping detection ----
    clipping = (peakSample >= 0.99f);
    if (clipping)
    {
        Problem p;
        p.type = Problem::Clipping;
        p.magnitude = 20.f * std::log10(std::max(peakSample, 0.0001f));
        p.description = "CLIP";
        problems.push_back(p);
    }

    // ---- Noise floor estimation ----
    // Take the lowest 10% of bins as noise floor estimate
    std::vector<float> sorted(magnitudesDb, magnitudesDb + numBins);
    std::sort(sorted.begin(), sorted.end());
    int floorCount = std::max(1, numBins / 10);
    float sum = 0.f;
    for (int i = 0; i < floorCount; ++i)
        sum += sorted[(size_t)i];
    noiseFloor = sum / (float)floorCount;

    // ---- Resonance detection ----
    // Look for bins that are significantly above their local average
    const float resonanceThreshDb = 12.f;  // must be 12dB above local avg
    const int   windowHalf        = 8;     // ±8 bins for local average

    for (int i = windowHalf; i < numBins - windowHalf; ++i)
    {
        float localSum = 0.f;
        int count = 0;
        for (int j = i - windowHalf; j <= i + windowHalf; ++j)
        {
            if (j != i)
            {
                localSum += magnitudesDb[j];
                ++count;
            }
        }
        float localAvg = localSum / (float)count;
        float diff = magnitudesDb[i] - localAvg;

        if (diff > resonanceThreshDb && magnitudesDb[i] > noiseFloor + 20.f)
        {
            // Check it's a local peak (higher than immediate neighbors)
            if (magnitudesDb[i] >= magnitudesDb[i - 1] &&
                magnitudesDb[i] >= magnitudesDb[i + 1])
            {
                double binFreq = ((double)i / (double)numBins) * (sampleRate * 0.5);

                Problem p;
                p.type = Problem::Resonance;
                p.freqHz = (float)binFreq;
                p.magnitude = magnitudesDb[i];
                if (binFreq >= 1000.0)
                    p.description = juce::String(binFreq / 1000.0, 1) + "k";
                else
                    p.description = juce::String((int)binFreq) + " Hz";
                problems.push_back(p);
            }
        }
    }

    // Limit to top 5 resonances by magnitude
    if (problems.size() > 6)
    {
        // Keep clipping entry, sort resonances by magnitude
        std::stable_sort(problems.begin(), problems.end(),
            [](const Problem& a, const Problem& b)
            {
                if (a.type == Problem::Clipping) return true;
                if (b.type == Problem::Clipping) return false;
                return a.magnitude > b.magnitude;
            });
        problems.resize(6);
    }
}
