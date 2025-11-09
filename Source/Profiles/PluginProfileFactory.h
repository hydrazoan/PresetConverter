#pragma once
#include <JuceHeader.h>
#include "PluginProfile.h"

class PluginProfileFactory
{
public:
    PluginProfileFactory();
    ~PluginProfileFactory() = default;

    int  loadProfilesFromDirectory(const juce::File& profilesDirectory);
    bool loadProfile(const juce::File& profileFile);
    void registerProfile(const PluginProfile& profile);

    const PluginProfile* getProfileById(const juce::String& pluginId) const;
    const PluginProfile* getProfileByName(const juce::String& pluginName) const;

    juce::StringArray          getRegisteredPluginNames() const;
    std::vector<PluginProfile> getAllProfiles() const;

    bool hasProfile(const juce::String& pluginId) const;
    int  getProfileCount() const { return (int) profiles.size(); }
    void clearProfiles();

    PluginProfile createDefaultProfile(const juce::String& pluginId,
                                       const juce::String& pluginName) const;

    juce::String getLastError() const { return lastError; }

private:
    void          loadBuiltInProfiles();
    PluginProfile createZamplerProfile() const;

    std::map<juce::String, PluginProfile> profiles;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProfileFactory)
};