#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

// Pure maths + music theory helpers (no UI, no audio).
namespace Theory
{
    inline int wrap12 (int n) { return ((n % 12) + 12) % 12; }

    // Name used for keys, e.g. "C#/Db"
    inline juce::String keyName (int n)
    {
        static const char* names[] = { "C", "C#/Db", "D", "D#/Eb", "E", "F",
                                       "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
        return names[wrap12 (n)];
    }

    // Short name used in note lists / root buttons
    inline juce::String noteName (int n)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };
        return names[wrap12 (n)];
    }

    struct Scale
    {
        const char* name;
        std::vector<int> intervals;
        int relOffset;   // semitones from this scale's root to its relative scale's root
        int relScale;    // index of the relative scale
    };

    // Index order matters (relScale refers to these indices):
    // 0 Major, 1 Natural Minor, 2 Dorian, 3 Phrygian, 4 Lydian, 5 Mixolydian,
    // 6 Locrian, 7 Harmonic Minor, 8 Melodic Minor, 9 Major Pent, 10 Minor Pent, 11 Blues
    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> s = {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 },  9,  1 },
            { "Minor",            { 0, 2, 3, 5, 7, 8, 10 },  3,  0 },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, 10,  0 },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 },  8,  0 },
            { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 },  7,  0 },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 },  5,  0 },
            { "Locrian",          { 0, 1, 3, 5, 6, 8, 10 },  1,  0 },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 },  3,  0 },
            { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 },  3,  0 },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 },         9, 10 },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 },        3,  9 },
            { "Blues",            { 0, 3, 5, 6, 7, 10 },     3,  9 }
        };
        return s;
    }

    inline double centsToHz (double cents, double refHz)
    {
        return refHz * std::pow (2.0, cents / 1200.0);
    }

    inline double hzToCents (double hz, double refHz)
    {
        if (hz <= 0.0 || refHz <= 0.0) return 0.0;
        return 1200.0 * std::log2 (hz / refHz);
    }

    struct ShiftResult
    {
        int    semitones = 0;      // nearest whole-semitone shift
        double residualCents = 0;  // what's left over (-50..+50)
        int    newRoot = 0;        // key after the shift
        int    relRoot = 0;        // relative key of the shifted key
        int    relScale = 0;       // relative scale index
    };

    inline ShiftResult shiftKey (int root, int scaleIdx, double cents)
    {
        ShiftResult r;
        r.semitones     = (int) std::lround (cents / 100.0);
        r.residualCents = cents - r.semitones * 100.0;
        r.newRoot       = wrap12 (root + r.semitones);
        const auto& sc  = scales()[(size_t) scaleIdx];
        r.relRoot       = wrap12 (r.newRoot + sc.relOffset);
        r.relScale      = sc.relScale;
        return r;
    }

    inline juce::String scaleNotes (int root, int scaleIdx)
    {
        juce::String out;
        for (int iv : scales()[(size_t) scaleIdx].intervals)
            out << noteName (root + iv) << "  ";
        return out.trim();
    }
}
