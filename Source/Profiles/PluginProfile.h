#pragma once
#include <JuceHeader.h>

struct PluginProfile
{
    juce::String pluginName;
    juce::String pluginId;       // VST2 4CC, etc.
    juce::String manufacturer;
    juce::String version;
    bool         isChunkBased = true;
    bool         isVst2      = true;

    juce::String defaultLayerType = "Sampler";
    juce::String profileAuthor    = "PresetConverter";
    juce::Time   creationDate     = juce::Time::getCurrentTime();
    juce::String notes;

    bool isValid() const { return pluginId.isNotEmpty() && pluginName.isNotEmpty(); }

    bool loadFromFile(const juce::File& f)
    {
        if (!f.existsAsFile()) return false;
        juce::var v = juce::JSON::parse(f);
        if (!v.isObject()) return false;

        auto* o = v.getDynamicObject(); if (!o) return false;

        pluginName       = o->getProperty("pluginName").toString();
        pluginId         = o->getProperty("pluginId").toString();
        manufacturer     = o->getProperty("manufacturer").toString();
        version          = o->getProperty("version").toString();
        isChunkBased     = (bool) o->getProperty("isChunkBased");
        isVst2           = (bool) o->getProperty("isVst2");
        defaultLayerType = o->getProperty("defaultLayerType").toString();
        profileAuthor    = o->getProperty("profileAuthor").toString();
        notes            = o->getProperty("notes").toString();

        return isValid();
    }

    JUCE_LEAK_DETECTOR(PluginProfile)
};