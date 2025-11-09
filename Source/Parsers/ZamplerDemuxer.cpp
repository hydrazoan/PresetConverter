#include "ZamplerDemuxer.h"

static juce::String toAsciiSafe(const void* data, size_t size)
{
    // Safe copy into String, keeping printable ASCII
    juce::String s;
    s.preallocateBytes((int) size);
    const char* c = static_cast<const char*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char ch = (unsigned char) c[i];
        if (ch == '\r' || ch == '\n' || (ch >= 0x20 && ch <= 0x7E))
            s << juce::String::charToString((juce_wchar) ch);
        else
            s << (juce::String) ""; // skip binary
    }
    return s;
}

std::vector<PresetData> ZamplerDemuxer::demux(const PluginProfile& profile,
                                              const PresetData& bankPreset) noexcept
{
    std::vector<PresetData> out;
    if (!bankPreset.isChunkBased || bankPreset.chunkData.getSize() == 0)
        return out;

    // 1) Profile-driven markers first
    {
        auto v = splitByMarkers(profile, bankPreset);
        if (!v.empty())
            return v;
    }

    // 2) Heuristic
    {
        auto v = splitHeuristically(bankPreset);
        if (!v.empty())
            return v;
    }

    // 3) Give up (caller will keep single-preset fallback)
    return out;
}

std::vector<PresetData> ZamplerDemuxer::splitByMarkers(const PluginProfile& profile,
                                                       const PresetData& bankPreset) noexcept
{
    std::vector<PresetData> out;

    // We store optional markers in PluginProfile.notes as JSON, e.g.:
    // {
    //   "demuxMarkers": ["<program ", "ProgramName="],
    //   "minBlockBytes": 2048
    // }
    // This avoids changing your profile schema. If notes isn't JSON, ignore.

    juce::var json = juce::JSON::parse(profile.notes);
    if (!json.isObject())
        return out;

    auto* obj = json.getDynamicObject();
    if (!obj) return out;

    const auto markersVar = obj->getProperty("demuxMarkers");
    const int  minBlock   = (int) obj->getProperty("minBlockBytes", 2048);

    juce::StringArray markers;
    if (markersVar.isArray())
    {
        for (const auto& v : *markersVar.getArray())
            markers.add(v.toString());
    }

    if (markers.isEmpty())
        return out;

    const auto bigText = toAsciiSafe(bankPreset.chunkData.getData(), bankPreset.chunkData.getSize());
    juce::Array<int> cuts;

    for (const auto& mk : markers)
    {
        int pos = 0;
        while (pos >= 0)
        {
            pos = bigText.indexOf(pos, mk);
            if (pos >= 0)
            {
                cuts.addIfNotAlreadyThere(pos);
                pos += mk.length();
            }
        }
    }

    cuts.sort();
    // Build segments from cuts
    for (int i = 0; i < cuts.size(); ++i)
    {
        const int start = cuts.getUnchecked(i);
        const int end   = (i + 1 < cuts.size() ? cuts.getUnchecked(i + 1) : bigText.length());
        const int len   = juce::jmax(0, end - start);

        if (len < minBlock) continue;

        const juce::String segmentText = bigText.substring(start, end);

        PresetData p;
        p.isValid      = true;
        p.isChunkBased = true; // still binary-based in Unify, but we only retained text for detection
        p.pluginName   = profile.pluginName;
        p.pluginId     = profile.pluginId;

        // Name heuristic
        p.presetName = extractLikelyNameFromBlock(segmentText.toRawUTF8(),
                                                  (size_t) segmentText.getNumBytesAsUTF8());
        if (p.presetName.isEmpty())
            p.presetName = "Program " + juce::String(out.size() + 1);

        // Store as text (since we can't easily recover exact binary splits using text cuts)
        // BUT to preserve the original bytes, we also keep a slice of the original chunk that
        // corresponds approximately to this text window. We map UTF-8 indices back to bytes
        // conservatively by clamping.
        const size_t totalBytes = bankPreset.chunkData.getSize();
        const double ratio      = totalBytes > 0 ? (double) start / (double) bigText.length() : 0.0;
        const double ratioEnd   = totalBytes > 0 ? (double) end   / (double) bigText.length() : 0.0;

        const size_t byteStart  = (size_t) juce::jlimit(0, (int) totalBytes, (int) std::floor(totalBytes * ratio));
        const size_t byteEnd    = (size_t) juce::jlimit(0, (int) totalBytes, (int) std::ceil (totalBytes * ratioEnd));
        const size_t byteLen    = (byteEnd > byteStart ? byteEnd - byteStart : 0);

        if (byteLen >= (size_t) minBlock)
        {
            p.chunkData.setSize(byteLen);
            std::memcpy(p.chunkData.getData(),
                        (const char*) bankPreset.chunkData.getData() + byteStart,
                        byteLen);
        }
        else
        {
            // fall back to textData if binary slice would be too tiny
            p.isChunkBased = false;
            p.isTextBased  = true;
            p.textData     = segmentText;
        }

        out.push_back(std::move(p));
    }

    // If we found only one big block, it's not a split — return empty to fall back
    if (out.size() <= 1)
        out.clear();

    return out;
}

std::vector<PresetData> ZamplerDemuxer::splitHeuristically(const PresetData& bankPreset) noexcept
{
    std::vector<PresetData> out;
    const auto bigText = toAsciiSafe(bankPreset.chunkData.getData(), bankPreset.chunkData.getSize());

    // Try simple delimiters seen in many text-ish preset packs
    const juce::StringArray probes { "ProgramName=", "PresetName=", "<program", "[Program]", "@PROGRAM@" };
    juce::Array<int> cuts;

    for (const auto& mk : probes)
    {
        int pos = 0;
        while (pos >= 0)
        {
            pos = bigText.indexOf(pos, mk);
            if (pos >= 0)
            {
                cuts.addIfNotAlreadyThere(pos);
                pos += mk.length();
            }
        }
    }

    cuts.sort();
    if (cuts.size() < 2)
        return out;

    for (int i = 0; i < cuts.size(); ++i)
    {
        const int start = cuts.getUnchecked(i);
        const int end   = (i + 1 < cuts.size() ? cuts.getUnchecked(i + 1) : bigText.length());
        const juce::String seg = bigText.substring(start, end);

        if (seg.trim().isEmpty())
            continue;

        PresetData p;
        p.isValid      = true;
        p.isChunkBased = false; // no reliable binary segmentation
        p.isTextBased  = true;
        p.textData     = seg;
        p.presetName   = extractLikelyNameFromBlock(seg.toRawUTF8(), (size_t) seg.getNumBytesAsUTF8());

        if (p.presetName.isEmpty())
            p.presetName = "Program " + juce::String(out.size() + 1);

        out.push_back(std::move(p));
    }

    if (out.size() <= 1)
        out.clear();

    return out;
}

juce::String ZamplerDemuxer::extractLikelyNameFromBlock(const void* data, size_t size) noexcept
{
    juce::String s = toAsciiSafe(data, size);

    // Try several common name keys
    const juce::StringArray keys { "ProgramName=", "PresetName=", "Name=", "name=", "title=", "patch_name=" };

    for (const auto& k : keys)
    {
        const int idx = s.indexOfIgnoreCase(k);
        if (idx >= 0)
        {
            auto rest = s.substring(idx + k.length());
            // name until EOL or quote
            int eol = rest.indexOfChar('\n');
            if (eol < 0) eol = rest.length();
            auto cand = rest.substring(0, eol).unquoted().trim();
            if (cand.isNotEmpty())
                return cand;
        }
    }

    // XML-ish
    {
        const int a = s.indexOfIgnoreCase("<name>");
        const int b = s.indexOfIgnoreCase("</name>");
        if (a >= 0 && b > a)
        {
            auto cand = s.substring(a + 6, b).trim();
            if (cand.isNotEmpty()) return cand;
        }
    }

    // Fallback
    return {};
}