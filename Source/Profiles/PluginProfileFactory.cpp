#include "PluginProfileFactory.h"

// Small helpers to safely extract properties with defaults (JUCE single-arg getProperty)
namespace
{
    inline juce::String getStringProp(const juce::DynamicObject* obj,
                                      const juce::Identifier& key,
                                      const juce::String& def = {})
    {
        if (obj == nullptr) return def;
        const juce::var v = obj->getProperty(key);
        return v.isVoid() ? def : v.toString();
    }

    inline bool getBoolProp(const juce::DynamicObject* obj,
                            const juce::Identifier& key,
                            bool def = false)
    {
        if (obj == nullptr) return def;
        const juce::var v = obj->getProperty(key);
        return v.isVoid() ? def : (bool) v;
    }

    inline void getStringArrayProp(const juce::DynamicObject* obj,
                                   const juce::Identifier& key,
                                   juce::StringArray& out)
    {
        out.clear();
        if (obj == nullptr) return;
        const juce::var v = obj->getProperty(key);
        if (!v.isArray()) return;
        auto* arr = v.getArray();
        if (arr == nullptr) return;
        for (auto& item : *arr)
            out.add(item.toString());
    }

    inline void getVarArrayProp(const juce::DynamicObject* obj,
                                const juce::Identifier& key,
                                juce::Array<juce::var>& out)
    {
        out.clear();
        if (obj == nullptr) return;
        const juce::var v = obj->getProperty(key);
        if (!v.isArray()) return;
        auto* arr = v.getArray();
        if (arr == nullptr) return;
        for (auto& item : *arr)
            out.add(item);
    }

    inline juce::String toLowerKey(const juce::String& s)
    {
        return s.trim().toLowerCase();
    }
}

//==============================================================================

PluginProfileFactory::PluginProfileFactory() = default;

//==============================================================================

int PluginProfileFactory::loadProfilesFromDirectory(const juce::File& profilesDirectory)
{
    lastError.clear();

    if (!profilesDirectory.isDirectory())
    {
        lastError = "Not a valid directory: " + profilesDirectory.getFullPathName();
        return 0;
    }

    auto jsonFiles = profilesDirectory.findChildFiles(juce::File::findFiles, false, "*.json");

    int loadedCount = 0;
    for (const auto& file : jsonFiles)
    {
        if (loadProfile(file))
            ++loadedCount;
    }

    return loadedCount;
}

bool PluginProfileFactory::loadProfile(const juce::File& profileFile)
{
    lastError.clear();

    juce::var parsed = juce::JSON::parse(profileFile);
    if (!parsed.isObject())
    {
        lastError = "Invalid JSON in: " + profileFile.getFileName();
        return false;
    }

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        lastError = "JSON is not an object: " + profileFile.getFileName();
        return false;
    }

    PluginProfile profile;

    // Single-arg getProperty + safe conversion helpers
    profile.pluginName     = getStringProp(obj, "pluginName");
    profile.pluginId       = getStringProp(obj, "pluginId");
    profile.manufacturer   = getStringProp(obj, "manufacturer");
    profile.version        = getStringProp(obj, "version");
    profile.isChunkBased   = getBoolProp  (obj, "isChunkBased", false);
    profile.isVst2         = getBoolProp  (obj, "isVst2", false);
    profile.isVst3         = getBoolProp  (obj, "isVst3", false);
    profile.defaultLayerType = getStringProp(obj, "defaultLayerType", "Synth");
    profile.profileAuthor    = getStringProp(obj, "profileAuthor", "PresetConverter");
    profile.notes            = getStringProp(obj, "notes");

    getStringArrayProp(obj, "aliases",         profile.aliases);
    getStringArrayProp(obj, "requiredSamples", profile.requiredSamples);
    getVarArrayProp   (obj, "parameterMappings", profile.parameterMappings);

    if (!profile.isValid())
    {
        lastError = "Profile missing pluginName and pluginId: " + profileFile.getFileName();
        return false;
    }

    registerProfile(profile);
    return true;
}

void PluginProfileFactory::registerProfile(const PluginProfile& profile)
{
    if (!profile.isValid())
        return;

    // Store by ID if present; otherwise create a synthetic key from name
    juce::String idKey = profile.pluginId;
    if (idKey.isEmpty())
        idKey = "__NAME__:" + profile.pluginName; // ensure uniqueness even without ID

    profiles[idKey] = profile;

    // Fill alias lookup (lower-cased)
    for (const auto& alias : profile.aliases)
        aliasLookup[toLowerKey(alias)] = idKey;

    // Also map the canonical pluginName as an alias for resilience
    if (profile.pluginName.isNotEmpty())
        aliasLookup[toLowerKey(profile.pluginName)] = idKey;
}

const PluginProfile* PluginProfileFactory::getProfileById(const juce::String& pluginId) const
{
    auto it = profiles.find(pluginId);
    if (it != profiles.end())
        return &it->second;

    return nullptr;
}

const PluginProfile* PluginProfileFactory::getProfileByAlias(const juce::String& alias) const
{
    auto it = aliasLookup.find(toLowerKey(alias));
    if (it == aliasLookup.end())
        return nullptr;

    return getProfileById(it->second);
}

const PluginProfile* PluginProfileFactory::getProfileByName(const juce::String& pluginName) const
{
    // 1) Exact name match across stored profiles
    auto norm = toLowerKey(pluginName);
    for (const auto& pair : profiles)
    {
        const auto& p = pair.second;
        if (toLowerKey(p.pluginName) == norm)
            return &p;
    }

    // 2) Alias match
    if (auto* byAlias = getProfileByAlias(pluginName))
        return byAlias;

    // 3) Not found
    return nullptr;
}

juce::StringArray PluginProfileFactory::getRegisteredPluginNames() const
{
    juce::StringArray names;
    for (const auto& pair : profiles)
        if (pair.second.pluginName.isNotEmpty())
            names.add(pair.second.pluginName);

    names.sort(true);
    names.removeDuplicates(true);
    return names;
}

std::vector<PluginProfile> PluginProfileFactory::getAllProfiles() const
{
    std::vector<PluginProfile> out;
    out.reserve(profiles.size());
    for (const auto& pair : profiles)
        out.push_back(pair.second);
    return out;
}

bool PluginProfileFactory::hasProfile(const juce::String& pluginId) const
{
    return profiles.find(pluginId) != profiles.end();
}

void PluginProfileFactory::clearProfiles()
{
    profiles.clear();
    aliasLookup.clear();
}

PluginProfile PluginProfileFactory::createDefaultProfile(const juce::String& pluginId,
                                                         const juce::String& pluginName) const
{
    PluginProfile p;
    p.pluginId        = pluginId;
    p.pluginName      = pluginName.isNotEmpty() ? pluginName : "Unknown Plugin";
    p.manufacturer    = "Unknown";
    p.version         = "1.0";
    p.isChunkBased    = true;   // safest default for binary VSTs
    p.isVst2          = true;   // common case for FXP/FXB
    p.defaultLayerType = "Synth";
    p.profileAuthor   = "Auto-generated";
    p.notes           = "Default profile with no parameter mappings.";
    return p;
}