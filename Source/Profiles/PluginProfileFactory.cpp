#include "PluginProfileFactory.h"

PluginProfileFactory::PluginProfileFactory()
{
    loadBuiltInProfiles();
}

int PluginProfileFactory::loadProfilesFromDirectory(const juce::File& dir)
{
    lastError = {};
    int loaded = 0;

    if (!dir.isDirectory())
    {
        lastError = "Not a directory: " + dir.getFullPathName();
        return 0;
    }

    juce::Array<juce::File> jsonFiles;
    dir.findChildFiles(jsonFiles, juce::File::findFiles, false, "*.json");

    for (const auto& f : jsonFiles)
        if (loadProfile(f)) ++loaded;

    return loaded;
}

bool PluginProfileFactory::loadProfile(const juce::File& profileFile)
{
    lastError = {};

    PluginProfile p;
    if (!p.loadFromFile(profileFile))
    {
        lastError = "Failed to load profile: " + profileFile.getFullPathName();
        DBG(lastError);
        return false;
    }

    if (!p.isValid())
    {
        lastError = "Invalid profile (missing fields): " + profileFile.getFileName();
        DBG(lastError);
        return false;
    }

    if (profiles.find(p.pluginId) != profiles.end())
        DBG("Profile overwrite: pluginId=" + p.pluginId + " by " + profileFile.getFileName());

    registerProfile(p);
    return true;
}

void PluginProfileFactory::registerProfile(const PluginProfile& profile)
{
    if (!profile.isValid()) return;
    profiles[profile.pluginId] = profile;
}

const PluginProfile* PluginProfileFactory::getProfileById(const juce::String& pluginId) const
{
    auto it = profiles.find(pluginId);
    return it != profiles.end() ? &it->second : nullptr;
}

const PluginProfile* PluginProfileFactory::getProfileByName(const juce::String& pluginName) const
{
    for (const auto& kv : profiles)
        if (kv.second.pluginName.equalsIgnoreCase(pluginName))
            return &kv.second;
    return nullptr;
}

juce::StringArray PluginProfileFactory::getRegisteredPluginNames() const
{
    juce::StringArray names;
    for (const auto& kv : profiles) names.add(kv.second.pluginName);
    names.removeDuplicates(false);
    names.sort(true);
    return names;
}

std::vector<PluginProfile> PluginProfileFactory::getAllProfiles() const
{
    std::vector<PluginProfile> v;
    v.reserve(profiles.size());
    for (const auto& kv : profiles) v.push_back(kv.second);
    return v;
}

bool PluginProfileFactory::hasProfile(const juce::String& pluginId) const
{
    return profiles.find(pluginId) != profiles.end();
}

void PluginProfileFactory::clearProfiles()
{
    profiles.clear();
}

PluginProfile PluginProfileFactory::createDefaultProfile(const juce::String& pluginId,
                                                         const juce::String& pluginName) const
{
    PluginProfile p;
    p.pluginId     = pluginId;
    p.pluginName   = pluginName.isNotEmpty() ? pluginName : "Unknown Plugin";
    p.manufacturer = "Unknown";
    p.version      = "1.0";
    p.isChunkBased = true;
    p.isVst2       = true;
    p.defaultLayerType = "Sampler";
    p.profileAuthor    = "Auto-generated";
    p.notes            = "Default profile generated at runtime.";
    return p;
}

void PluginProfileFactory::loadBuiltInProfiles()
{
    registerProfile(createZamplerProfile());
}

PluginProfile PluginProfileFactory::createZamplerProfile() const
{
    PluginProfile p;
    p.pluginName   = "Zampler RX";
    p.pluginId     = "ZMPL"; // adjust if needed
    p.manufacturer = "Zampler Sounds";
    p.version      = "1.0";
    p.isChunkBased = true;
    p.isVst2       = true;
    p.defaultLayerType = "Sampler";
    p.profileAuthor    = "PresetConverter";
    p.creationDate     = juce::Time::getCurrentTime();
    p.notes            = "Built-in Zampler profile (chunk-based).";
    return p;
}