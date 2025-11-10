#pragma once

#include <JuceHeader.h>
#include "../Core/PluginProfile.h"

/**
 * @class PluginProfileFactory
 * @brief Factory for creating and managing plugin profiles
 *
 * - Loads profiles from JSON files in a directory
 * - Matches by pluginId (exact), name (case-insensitive), or alias
 */
class PluginProfileFactory
{
public:
    PluginProfileFactory();
    ~PluginProfileFactory() = default;

    /** Load all profiles from a directory of *.json files. Returns count loaded. */
    int loadProfilesFromDirectory(const juce::File& profilesDirectory);

    /** Load a single profile JSON file. Returns true if loaded & registered. */
    bool loadProfile(const juce::File& profileFile);

    /** Register a profile programmatically (must be valid). */
    void registerProfile(const PluginProfile& profile);

    /** Look up by ID (exact match). */
    const PluginProfile* getProfileById(const juce::String& pluginId) const;

    /** Look up by name (case-insensitive), falling back to alias. */
    const PluginProfile* getProfileByName(const juce::String& pluginName) const;

    /** Look up by alias (case-insensitive). */
    const PluginProfile* getProfileByAlias(const juce::String& alias) const;

    /** Get all registered plugin names (for UI dropdown). */
    juce::StringArray getRegisteredPluginNames() const;

    /** Get all registered profiles (copy). */
    std::vector<PluginProfile> getAllProfiles() const;

    /** Check presence by ID. */
    bool hasProfile(const juce::String& pluginId) const;

    /** Number of registered profiles. */
    int getProfileCount() const { return static_cast<int>(profiles.size()); }

    /** Remove all profiles. */
    void clearProfiles();

    /** Create a reasonable default if detection failed. */
    PluginProfile createDefaultProfile(const juce::String& pluginId,
                                       const juce::String& pluginName) const;

    juce::String getLastError() const { return lastError; }

private:
    // Keyed by pluginId (exact, case-sensitive as written in JSON)
    std::map<juce::String, PluginProfile> profiles;

    // alias (lowercased) -> pluginId
    std::map<juce::String, juce::String> aliasLookup;

    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProfileFactory)
};