#include "MainComponent.h"
#include "../Parsers/ZamplerDemuxer.h"

MainComponent::MainComponent()
    : progressBar(progress)
{
    setSize(900, 700);
    setupComponents();

    // Load profiles
    auto profilesDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                           .getParentDirectory().getChildFile("Profiles");

    if (profilesDir.exists())
    {
        int loaded = profileFactory.loadProfilesFromDirectory(profilesDir);
        logMessage("Loaded " + juce::String(loaded) + " plugin profiles");
    }

    // Populate selector
    pluginSelector.clear();
    pluginSelector.addItem("(Automatic detection)", 1);

    auto names = profileFactory.getRegisteredPluginNames();
    int baseId = 2;
    for (const auto& n : names) pluginSelector.addItem(n, baseId++);
    pluginSelector.setSelectedId(1);

    logMessage("Preset Converter ready");
    logMessage("Drag and drop FXB/FXP/H2P/VSTPRESET files or click Browse");
}

MainComponent::~MainComponent() = default;

void MainComponent::setupComponents()
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("VST to Unify Preset Converter", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(instructionsLabel);
    instructionsLabel.setText("Drag and drop files here, or click Browse",
                              juce::dontSendNotification);
    instructionsLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(pluginSelector);
    pluginSelector.onChange = [this]
    {
        if (auto* p = getSelectedProfile())
            logMessage("Manual profile selected: " + p->pluginName + " (ID: " + p->pluginId + ")");
        else
            logMessage("Automatic detection enabled.");
    };

    addAndMakeVisible(browseButton);
    browseButton.setButtonText("Browse...");
    browseButton.onClick = [this] { browseForFiles(); };

    addAndMakeVisible(settingsButton);
    settingsButton.setButtonText("Output Directory...");
    settingsButton.onClick = [this] { selectOutputDirectory(); };

    addAndMakeVisible(progressBar);
    progressBar.setPercentageDisplay(true);

    addAndMakeVisible(logTextEditor);
    logTextEditor.setMultiLine(true);
    logTextEditor.setReadOnly(true);
    logTextEditor.setScrollbarsShown(true);
    logTextEditor.setCaretVisible(false);
    logTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    logTextEditor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    if (isDragging)
    {
        g.setColour(juce::Colours::lightblue.withAlpha(0.5f));
        g.fillRoundedRectangle(getLocalBounds().reduced(10).toFloat(), 10.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(getLocalBounds().reduced(10).toFloat(), 10.0f, 3.0f);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);

    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);
    instructionsLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(20);

    auto selectorArea = area.removeFromTop(30);
    pluginSelector.setBounds(selectorArea.removeFromLeft(320));
    area.removeFromTop(10);

    auto buttonArea = area.removeFromTop(30);
    browseButton.setBounds(buttonArea.removeFromLeft(180));
    buttonArea.removeFromLeft(10);
    settingsButton.setBounds(buttonArea.removeFromLeft(180));
    area.removeFromTop(10);

    progressBar.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);
    logTextEditor.setBounds(area);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".fxp" || ext == ".fxb" || ext == ".h2p" ||
            ext == ".h2p.txt" || ext == ".uhe-preset" ||
            ext == ".txt" || ext == ".json" || ext == ".vstpreset")
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isDragging = false;
    repaint();

    for (auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            logMessage("Dropped: " + file.getFileName());
            convertFile(file);
        }
    }
}

void MainComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isDragging = true;
    repaint();
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    isDragging = false;
    repaint();
}

void MainComponent::browseForFiles()
{
    logMessage("Browsing for preset files...");

    auto chooser = std::make_shared<juce::FileChooser>(
        "Select preset files",
        juce::File{},
        "*.fxp;*.fxb;*.h2p;*.h2p.txt;*.uhe-preset;*.txt;*.json;*.vstpreset");

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, chooser](const juce::FileChooser& fc) noexcept
        {
            auto results = fc.getResults();
            if (results.isEmpty())
            {
                logMessage("No files selected.");
                return;
            }
            for (auto& file : results)
            {
                logMessage("Selected: " + file.getFullPathName());
                convertFile(file);
            }
        });
}

void MainComponent::selectOutputDirectory()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select output directory", juce::File{}, "");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) noexcept
        {
            auto results = fc.getResults();
            if (results.isEmpty())
            {
                logMessage("No output directory selected.");
                return;
            }

            outputDirectory = results[0];
            logMessage("Output directory set to: " + outputDirectory.getFullPathName());
        });
}

const PluginProfile* MainComponent::getSelectedProfile() const
{
    int id = pluginSelector.getSelectedId();
    if (id <= 1) return nullptr;

    auto selectedName = pluginSelector.getText().trim();
    if (selectedName.isEmpty()) return nullptr;

    if (auto* byName = profileFactory.getProfileByName(selectedName)) return byName;
    if (auto* byId   = profileFactory.getProfileById(selectedName))   return byId;
    return nullptr;
}

void MainComponent::convertFile(const juce::File& inputFile)
{
    logMessage("=== Converting: " + inputFile.getFileName() + " ===");

    // Output folder default
    juce::File outDir = outputDirectory.exists() ? outputDirectory
                                                 : inputFile.getParentDirectory().getChildFile("Converted");
    outDir.createDirectory();

    std::vector<PresetData> presets;

    auto ext = inputFile.getFileExtension().toLowerCase();
    if (ext == ".fxp" || ext == ".fxb")
    {
        logMessage("Parsing VST2 file...");
        presets = vst2.parseFile(inputFile);
        if (presets.empty())
        {
            logMessage("ERROR: " + vst2.getLastError());
            return;
        }
    }
    else if (ext == ".vstpreset")
    {
        logMessage("Parsing VST3 preset...");
        presets = vst3.parseFile(inputFile);
        if (presets.empty())
        {
            logMessage("ERROR: " + vst3.getLastError());
            return;
        }
    }
    else if (ext == ".h2p" || ext == ".h2p.txt" || ext == ".uhe-preset" || ext == ".txt")
    {
        logMessage("Parsing U-He preset...");
        presets = uhe.parseFile(inputFile);
        if (presets.empty())
        {
            logMessage("ERROR: " + uhe.getLastError());
            return;
        }
    }
    else
    {
        logMessage("Unsupported file type: " + ext);
        return;
    }

    // Profile resolution
    const PluginProfile* profile = getSelectedProfile();

    if (profile == nullptr)
    {
        const auto& first = presets.front();

        if (first.pluginName.isNotEmpty())
            profile = profileFactory.getProfileByName(first.pluginName);

        if (profile == nullptr && first.pluginId.isNotEmpty())
            profile = profileFactory.getProfileById(first.pluginId);

        if (profile != nullptr)
            logMessage("Auto-detected profile: " + profile->pluginName + " (ID: " + profile->pluginId + ")");
    }
    else
    {
        logMessage("Using manual profile: " + profile->pluginName + " (ID: " + profile->pluginId + ")");
    }

    PluginProfile fallback;
    if (profile == nullptr)
    {
        const auto& first = presets.front();
        fallback = profileFactory.createDefaultProfile(first.pluginId,
                    first.pluginName.isNotEmpty() ? first.pluginName : "Unknown Plugin");
        profile = &fallback;
        logMessage("No matching profile found. Using default profile: " + fallback.pluginName);
    }

    convertPresets(presets, *profile, outDir);
    logMessage("✔ Conversion complete for " + inputFile.getFileName());
}

void MainComponent::convertPresets(const std::vector<PresetData>& presets,
                                   const PluginProfile& profile,
                                   const juce::File& outputDir)
{
    int successCount = 0;
    const int total = (int) presets.size();

    for (int i = 0; i < total; ++i)
    {
        const auto& preset = presets[(size_t) i];

        updateProgress((double) i / juce::jmax(1, total));

        auto filename = preset.presetName.isNotEmpty() ? preset.presetName
                                                       : "Preset_" + juce::String(i + 1);

        filename = filename.replaceCharacter('/', '-')
                           .replaceCharacter('\\', '-')
                           .replaceCharacter(':', '-');

        auto patchFile = outputDir.getChildFile(filename + ".unify");
        int suffix = 1;
        while (patchFile.existsAsFile())
            patchFile = outputDir.getChildFile(filename + "_" + juce::String(suffix++) + ".unify");

        if (unifyGenerator.savePatch(preset, patchFile))
        {
            logMessage("✓ Created: " + patchFile.getFileName());

            auto guruFile = patchFile.withFileExtension(".guru");
            if (guruGenerator.saveGuruFile(preset, guruFile))
                logMessage("  + Metadata: " + guruFile.getFileName());

            ++successCount;
        }
        else
        {
            logMessage("✗ Failed: " + filename + " - " + unifyGenerator.getLastError());
        }
    }

    updateProgress(1.0);
    logMessage("\nConverted " + juce::String(successCount) + " of " + juce::String(total) + " presets");
    logMessage("Output: " + outputDir.getFullPathName());
}

void MainComponent::logMessage(const juce::String& message)
{
    logTextEditor.moveCaretToEnd();
    logTextEditor.insertTextAtCaret(message + "\n");
    logTextEditor.moveCaretToEnd();
}

void MainComponent::updateProgress(double newProgress)
{
    progress = newProgress;
    repaint();
}