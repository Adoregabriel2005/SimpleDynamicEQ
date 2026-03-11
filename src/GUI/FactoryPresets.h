#pragma once
#include <JuceHeader.h>

// ============================================================
//  FactoryPresets — boom-bap focused EQ presets
// ============================================================
namespace FactoryPresets
{
    struct BandPreset
    {
        int    bandIndex;
        bool   enabled;
        float  freq;
        float  gain;
        float  Q;
        int    type;    // FilterType enum
        int    model;   // AnalogModel enum
    };

    struct Preset
    {
        const char* name;
        const char* category;
        std::vector<BandPreset> bands;
    };

    inline std::vector<Preset> getAll()
    {
        return {
            // ---- DRUMS ----
            { "Boom Bap Kick", "Drums", {
                { 0, true, 60.f,  4.0f, 0.8f,  0, 3 },   // Pultec low boost
                { 1, true, 350.f, -3.0f, 1.2f, 0, 1 },   // SSL cut muddy mids
                { 2, true, 4000.f, 2.5f, 1.0f, 0, 4 },   // API attack
            }},
            { "Crispy Snare", "Drums", {
                { 0, true, 200.f,  -2.0f, 1.5f, 0, 2 },  // Neve cut boxiness
                { 1, true, 900.f,  1.5f,  1.0f, 0, 4 },   // API crack
                { 2, true, 5000.f, 3.0f,  0.8f, 0, 1 },   // SSL brightness
                { 3, true, 120.f,  0.0f,  0.7f, 3, 0 },   // Clean low cut
            }},
            { "Dusty Hi-Hats", "Drums", {
                { 0, true, 300.f,  0.0f, 0.7f,  3, 0 },   // Clean low cut
                { 1, true, 8000.f, -2.0f, 0.6f, 0, 2 },   // Neve soften highs
                { 2, true, 12000.f, 2.0f, 0.5f, 2, 5 },   // Sontec shelf air
            }},
            { "Full Drum Bus", "Drums", {
                { 0, true, 40.f,   0.0f, 0.7f, 3, 0 },    // Clean low cut
                { 1, true, 80.f,   3.0f, 0.7f, 1, 3 },    // Pultec low shelf
                { 2, true, 400.f,  -2.5f, 1.0f, 0, 1 },   // SSL cut mud
                { 3, true, 3000.f, 2.0f, 0.8f,  0, 4 },   // API presence
                { 4, true, 12000.f, 1.5f, 0.5f, 2, 2 },   // Neve air shelf
            }},

            // ---- BASS ----
            { "808 Sub", "Bass", {
                { 0, true, 30.f,  0.0f, 0.7f, 3, 0 },     // Clean sub cut
                { 1, true, 55.f,  3.0f, 0.9f, 0, 3 },     // Pultec sub boost
                { 2, true, 250.f, -3.5f, 1.2f, 0, 1 },    // SSL clear mids
                { 3, true, 800.f, 0.0f, 0.7f, 4, 0 },     // Clean high cut
            }},
            { "Warm Bass Guitar", "Bass", {
                { 0, true, 40.f,   0.0f, 0.7f, 3, 0 },    // Clean low cut
                { 1, true, 80.f,   2.0f, 0.7f, 1, 2 },    // Neve warm shelf
                { 2, true, 700.f,  1.5f, 1.0f, 0, 4 },    // API growl
                { 3, true, 2500.f, -2.0f, 0.8f, 0, 1 },   // SSL tame highs
            }},

            // ---- VOCALS ----
            { "Boom Bap Vocal", "Vocals", {
                { 0, true, 80.f,   0.0f, 0.7f, 3, 0 },    // Clean rumble cut
                { 1, true, 250.f,  -2.0f, 1.2f, 0, 2 },   // Neve de-mud
                { 2, true, 3000.f, 2.5f, 0.8f,  0, 4 },   // API presence
                { 3, true, 10000.f, 2.0f, 0.5f, 2, 2 },   // Neve air shelf
            }},
            { "De-Harsh Vocal", "Vocals", {
                { 0, true, 80.f,   0.0f, 0.7f, 3, 0 },    // Clean rumble cut
                { 1, true, 3500.f, -4.0f, 3.0f, 0, 5 },   // Sontec de-harsh
                { 2, true, 8000.f, 1.5f, 0.5f, 2, 2 },    // Neve air shelf
            }},

            // ---- SAMPLES / KEYS ----
            { "Vinyl Sample Clean", "Samples", {
                { 0, true, 30.f,   0.0f, 0.7f, 3, 0 },    // Clean sub cut
                { 1, true, 300.f,  -1.5f, 1.0f, 0, 2 },   // Neve clear mud
                { 2, true, 5000.f, 1.5f, 0.6f,  0, 5 },   // Sontec air
                { 3, true, 15000.f, 0.0f, 0.7f, 4, 0 },   // Clean alias cut
            }},
            { "Rhodes / Keys", "Samples", {
                { 0, true, 60.f,   0.0f, 0.7f, 3, 0 },    // Clean low cut
                { 1, true, 200.f,  1.5f, 0.7f, 1, 3 },    // Pultec warmth
                { 2, true, 1000.f, -1.5f, 1.0f, 0, 1 },   // SSL scoop mids
                { 3, true, 6000.f, 2.0f, 0.5f, 2, 2 },    // Neve presence shelf
            }},

            // ---- MASTER ----
            { "Master - Boom Bap", "Master", {
                { 0, true, 25.f,  0.0f, 0.7f, 3, 0 },     // Clean sub cut
                { 1, true, 60.f,  2.0f, 0.6f, 1, 3 },     // Pultec low shelf
                { 2, true, 350.f, -1.0f, 0.8f, 0, 5 },    // Sontec gentle mud cut
                { 3, true, 3000.f, 1.0f, 0.7f, 0, 4 },    // API presence
                { 4, true, 12000.f, 1.5f, 0.5f, 2, 5 },   // Sontec air shelf
            }},
            { "Master - Clean Polish", "Master", {
                { 0, true, 30.f,   0.0f, 0.7f, 3, 0 },    // Clean sub cut
                { 1, true, 250.f,  -1.0f, 0.8f, 0, 5 },   // Sontec mud cut
                { 2, true, 8000.f, 1.0f, 0.5f, 2, 5 },    // Sontec air shelf
                { 3, true, 18000.f, 0.0f, 0.7f, 4, 0 },   // Clean alias cut
            }},
        };
    }
}
