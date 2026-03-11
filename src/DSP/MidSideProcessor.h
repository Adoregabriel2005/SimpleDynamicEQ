#pragma once

// ============================================================
//  MidSideProcessor
//  Converts L/R ↔ M/S and back for an entire block
// ============================================================
class MidSideProcessor
{
public:
    // L/R → M/S in-place
    static void encodeBlock(float* L, float* R, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float m = (L[i] + R[i]) * 0.5f;
            float s = (L[i] - R[i]) * 0.5f;
            L[i] = m;
            R[i] = s;
        }
    }

    // M/S → L/R in-place
    static void decodeBlock(float* M, float* S, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = M[i] + S[i];
            float r = M[i] - S[i];
            M[i] = l;
            S[i] = r;
        }
    }
};
