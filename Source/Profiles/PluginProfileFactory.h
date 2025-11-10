#pragma once

#include <JuceHeader.h>
#include "../Core/PluginProfile.h"

/**
 * @class PluginProfileFactory
 * @brief Factory for creating and managing plugin profiles
 */
class PluginProfileFactory
{
public:
    PluginProfileFactory();
    ~PluginProfileFactory() = default;

    int loadProfilesFromDirectory(const juce::File& profilesDirectory);
    bool loadProfile(const juce::File& profileFile);

    void registerProfile(const PluginProfile& profile);

    const PluginProfile* getProfileById(const juce::String& pluginId) const;
    const PluginProfile* getProfileByName(const juce::String& pluginName) const;

    // ✅ NEW: Lookup by alias
    const PluginProfile* getProfileByAlias(const juce::String& alias) const;

    juce::StringArray getRegisteredPluginNames() const;
    std::vector<PluginProfile> getAllProfiles() const;
    bool hasProfile(const juce::String& pluginId) const;
    int getProfileCount() const { return static_cast<int>(profiles.size()); }

    void clearProfiles();

    PluginProfile createDefaultProfile(const juce::String& pluginId,
                                       const juce::String& pluginName) const;

    juce::String getLastError() const { return lastError; }

private:
    void loadBuiltInProfiles();
    PluginProfile createZamplerProfile() const;

    // Keyed by ID
    std::map<juce::String, PluginProfile> profiles;

    // ✅ NEW: alias → ID
    std::map<juce::String, juce::String> aliasLookup;

    juce::String lastError;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProfileFactory)
};