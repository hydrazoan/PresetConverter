#pragma once
#include <JuceHeader.h>
#include <cstring>

using uint8 = juce::uint8;
using int32 = juce::int32;

namespace FormatHelpers
{
    // FourCC constructed from 4 bytes (BE order)
    inline int32 makeMagic(const char* fourCC)
    {
        return (int32)((uint8)fourCC[0] << 24 |
                       (uint8)fourCC[1] << 16 |
                       (uint8)fourCC[2] << 8  |
                       (uint8)fourCC[3]);
    }

    inline juce::String magicToString(int32 magic)
    {
        char b[5];
        b[0] = (char)((magic >> 24) & 0xFF);
        b[1] = (char)((magic >> 16) & 0xFF);
        b[2] = (char)((magic >> 8)  & 0xFF);
        b[3] = (char)( magic        & 0xFF);
        b[4] = '\0';
        return juce::String(b);
    }

    inline juce::String magicToString(const char* fourCC, size_t len = 4)
    {
        return juce::String::fromUTF8(fourCC, (int)len);
    }

    inline int32 readInt32BE(juce::InputStream& s)
    {
        uint8 b[4];
        if (s.read(b, 4) != 4) return 0;
        return (int32)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
    }

    inline float readFloatBE(juce::InputStream& s)
    {
        uint8 b[4];
        if (s.read(b, 4) != 4) return 0.0f;
        uint32 u = (uint32)(b[0] << 24) | (uint32)(b[1] << 16) | (uint32)(b[2] << 8) | (uint32)b[3];
        float f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }

    inline bool matchesMagic(int32 magic, const char* expected)
    {
        return magic == makeMagic(expected);
    }
}