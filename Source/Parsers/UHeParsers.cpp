#include "UHeParsers.h"

std::vector<PresetData> UHeParser::parseFile(const juce::File& file) noexcept
{
    std::vector<PresetData> out;

    if (!file.existsAsFile())
    {
        lastError = "U-He preset not found: " + file.getFullPathName();
        return out;
    }

    const auto ext = file.getFileExtension().toLowerCase();

    PresetData p;
    p.presetName = file.getFileNameWithoutExtension();
    p.pluginName = {};
    p.pluginId   = {};
    p.isValid    = true;

    if (ext == ".h2p" || ext == ".uhe-preset" || ext == ".txt" || ext == ".h2p.txt")
    {
        p.isTextBased = true;
        p.textData    = file.loadFileAsString();
    }
    else
    {
        lastError = "Unsupported U-He extension: " + ext;
        return out;
    }

    out.push_back(std::move(p));
    return out;
}