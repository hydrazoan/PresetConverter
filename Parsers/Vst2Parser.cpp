#include "Vst2Parser.h"
#include <cstring>
#include "../Utility/FormatHelpers.h"

using namespace FormatHelpers;

static juce::String fixedName(const char* bytes, size_t len)
{
    return juce::String::fromUTF8(bytes, (int) len).trim();
}

//======================================================================
std::vector<PresetData> Vst2Parser::parseFile(const juce::File& file)
{
    if (file.hasFileExtension(".fxp"))
        return { parseFxpPreset(file) };

    if (file.hasFileExtension(".fxb"))
        return parseFxbBank(file);

    lastError = "Unsupported VST2 type: " + file.getFileExtension();
    return {};
}

//======================================================================
PresetData Vst2Parser::parseFxpPreset(const juce::File& file)
{
    PresetData preset;

    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
    {
        lastError = "Failed to read FXP: " + file.getFullPathName();
        return preset;
    }

    juce::MemoryInputStream s(data, false);
    FxpHeader h{};

    if (!readFxpHeader(s, h))
    {
        lastError = "Invalid FXP header";
        return preset;
    }

    preset.pluginId   = fixedName(h.fxID, 4);
    preset.presetName = fixedName(h.prgName, sizeof(h.prgName));
    preset.isValid    = true;

    const auto fxpMagic = makeMagic(h.fxMagic);

    if (matchesMagic(fxpMagic, "FxCk"))
    {
        // Chunk-based preset
        if (s.getNumBytesRemaining() < 4)
        {
            lastError = "FXP missing chunk size";
            return preset;
        }

        const auto chunkSize = readInt32BE(s);
        if (chunkSize <= 0 || (juce::int64) chunkSize > s.getNumBytesRemaining())
        {
            lastError = "Invalid FXP chunk size";
            return preset;
        }

        preset.isChunkBased = true;
        preset.chunkData.setSize((size_t) chunkSize);
        s.read(preset.chunkData.getData(), (size_t) chunkSize);
    }
    else
    {
        // Parameter-based preset
        preset.isChunkBased = false;

        if (h.numParams < 0 || h.numParams > 50000)
        {
            lastError = "Invalid FXP param count";
            return preset;
        }

        preset.parameters.ensureStorageAllocated(h.numParams);
        for (int i = 0; i < h.numParams; ++i)
            preset.parameters.add(readFloatBE(s));
    }

    return preset;
}

//======================================================================
// FXB bank:
// - "FBCh"   -> chunk-based bank, one big state (we export as a single preset)
// - "FxBk"   -> parameter bank, sequence of FXP-like programs
std::vector<PresetData> Vst2Parser::parseFxbBank(const juce::File& file)
{
    std::vector<PresetData> out;

    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
    {
        lastError = "Failed to read FXB: " + file.getFullPathName();
        return out;
    }

    juce::MemoryInputStream s(data, false);
    FxbHeader b{};
    if (!readFxbHeader(s, b))
    {
        lastError = "Invalid FXB header";
        return out;
    }

    const auto bankMagic = makeMagic(b.fxMagic);

    // Case 1: Chunked bank (FBCh)
    if (matchesMagic(bankMagic, "FBCh"))
    {
        if (s.getNumBytesRemaining() < 4)
        {
            lastError = "FXB missing chunk size";
            return out;
        }

        const auto chunkSize = readInt32BE(s);
        if (chunkSize <= 0 || (juce::int64) chunkSize > s.getNumBytesRemaining())
        {
            lastError = "Invalid FXB chunk size";
            return out;
        }

        PresetData p;
        p.pluginId     = fixedName(b.fxID, 4);
        p.presetName   = file.getFileNameWithoutExtension();
        p.isChunkBased = true;
        p.isValid      = true;

        p.chunkData.setSize((size_t) chunkSize);
        s.read(p.chunkData.getData(), (size_t) chunkSize);

        out.push_back(std::move(p));
        return out;
    }

    // Case 2: Parameter bank (FxBk)
    if (matchesMagic(bankMagic, "FxBk"))
    {
        // The format embeds a sequence of FXP-like programs.
        // We attempt to parse each program as an FXP header + body.
        for (int i = 0; i < b.numPrograms && !s.isExhausted(); ++i)
        {
            auto pos = s.getPosition();

            FxpHeader ph{};
            if (!readFxpHeader(s, ph))
            {
                // Could not read next program header; bail out safely
                s.setPosition(pos);
                lastError = "Failed reading FXB program header at index " + juce::String(i);
                break;
            }

            PresetData prog;
            prog.pluginId   = fixedName(ph.fxID, 4);
            prog.presetName = fixedName(ph.prgName, sizeof(ph.prgName));
            prog.isValid    = true;

            const auto fxpMagic = makeMagic(ph.fxMagic);

            if (matchesMagic(fxpMagic, "FxCk"))
            {
                if (s.getNumBytesRemaining() < 4)
                {
                    lastError = "Missing per-program chunk size";
                    break;
                }
                const auto csz = readInt32BE(s);
                if (csz <= 0 || (juce::int64) csz > s.getNumBytesRemaining())
                {
                    lastError = "Invalid per-program chunk size";
                    break;
                }

                prog.isChunkBased = true;
                prog.chunkData.setSize((size_t) csz);
                s.read(prog.chunkData.getData(), (size_t) csz);
            }
            else
            {
                // Parameter-based
                prog.isChunkBased = false;
                if (ph.numParams < 0 || ph.numParams > 50000)
                {
                    lastError = "Unreasonable param count";
                    break;
                }
                prog.parameters.ensureStorageAllocated(ph.numParams);
                for (int p = 0; p < ph.numParams; ++p)
                    prog.parameters.add(readFloatBE(s));
            }

            if (prog.presetName.isEmpty())
                prog.presetName = "Program " + juce::String(i + 1);

            out.push_back(std::move(prog));
        }
        return out;
    }

    lastError = "Unknown FXB magic: " + magicToString(bankMagic);
    return out;
}

//======================================================================
bool Vst2Parser::readFxbHeader(juce::MemoryInputStream& s, FxbHeader& h)
{
    if (s.read(h.chunkMagic, 4) != 4) return false;
    h.byteSize = readBigEndianInt32(s);
    if (s.read(h.fxMagic, 4) != 4) return false;
    h.version  = readBigEndianInt32(s);
    if (s.read(h.fxID, 4) != 4) return false;
    h.fxVersion   = readBigEndianInt32(s);
    h.numPrograms = readBigEndianInt32(s);
    s.read(h.future, sizeof(h.future));
    return matchesMagic(makeMagic(h.chunkMagic), "CcnK");
}

bool Vst2Parser::readFxpHeader(juce::MemoryInputStream& s, FxpHeader& h)
{
    if (s.read(h.chunkMagic, 4) != 4) return false;
    h.byteSize = readBigEndianInt32(s);
    if (s.read(h.fxMagic, 4) != 4) return false;
    h.version = readBigEndianInt32(s);
    if (s.read(h.fxID, 4) != 4) return false;
    h.fxVersion = readBigEndianInt32(s);
    h.numParams = readBigEndianInt32(s);
    if (s.read(h.prgName, sizeof(h.prgName)) != sizeof(h.prgName)) return false;
    return matchesMagic(makeMagic(h.chunkMagic), "CcnK");
}

int32_t Vst2Parser::readBigEndianInt32(juce::MemoryInputStream& s)
{
    int32_t v{};
    s.read(&v, sizeof(v));
    return juce::ByteOrder::swapIfLittleEndian(v);
}