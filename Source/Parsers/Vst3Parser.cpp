#include "Vst3Parser.h"

std::vector<PresetData> Vst3Parser::parseFile(const juce::File& file) noexcept
{
    std::vector<PresetData> out;

    if (!file.existsAsFile())
    {
        lastError = "VST3 preset not found: " + file.getFullPathName();
        return out;
    }

    PresetData p;
    p.presetName   = file.getFileNameWithoutExtension();
    p.pluginName   = {};
    p.pluginId     = {};
    p.isChunkBased = true;
    p.isValid      = true;

    juce::MemoryBlock b;
    if (!file.loadFileAsData(b))
    {
        lastError = "Failed to read VST3 preset data";
        return out;
    }

    p.chunkData = b;
    out.push_back(std::move(p));
    return out;
}