#pragma once
#include <JuceHeader.h>

/**
 * @struct PluginProfile
 * @brief Describes a plugin and how its presets should be interpreted.
 */
struct PluginProfile
{
    // Core identity
    juce::String pluginName;     ///< Primary plugin name (human-readable)
    juce::String pluginId;       ///< Unique ID (VST2 4CC, or other)
    juce::String manufacturer;
    juce::String version;

    // Format flags
    bool isChunkBased = false;   ///< True for binary/chunk presets
    bool isVst2       = false;
    bool isVst3       = false;

    // Usability
    juce::String defaultLayerType = "Synth";
    juce::String profileAuthor    = "PresetConverter";
    juce::String notes;

    // NEW: Alternate names to match by (case-insensitive)
    juce::StringArray aliases;

    // Optional: listed samples (for samplers)
    juce::StringArray requiredSamples;

    // Optional/future: parameter mappings (opaque to the factory)
    juce::Array<juce::var> parameterMappings;

    PluginProfile() = default;

    bool isValid() const noexcept
    {
        return pluginName.isNotEmpty() || pluginId.isNotEmpty();
    }
};