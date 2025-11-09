#pragma once
#include <JuceHeader.h>
#include "../Core/PresetData.h"

class UnifyPatchGenerator
{
public:
    UnifyPatchGenerator() = default;
    ~UnifyPatchGenerator() = default;

    bool savePatch(const PresetData& preset, const juce::File& file) noexcept;
    juce::String getLastError() const noexcept { return lastError; }

private:
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UnifyPatchGenerator)
};