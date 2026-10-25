#pragma once

#include "PatternSerializer.h"

namespace pam
{
/** Disk layout (created on first run, also shipped as a ZIP in /Resources):

    <Documents>/PsytranceArpMatrix/
        Presets/
            Bassline_Rolling/  Acid_Grooves/  Melodic_Leads/  Stabs_Rhythmics/
        Sequences/
            User_Exported/     <- one-click "Save" target
            Drag_Cache/        <- MIDI files created for drag & drop (auto-cleaned)
*/
class PresetManager
{
public:
    static juce::StringArray factoryCategories() { return { "Bassline_Rolling", "Acid_Grooves", "Melodic_Leads", "Stabs_Rhythmics" }; }

    explicit PresetManager (juce::File rootDirectory = defaultRootDirectory());

    static juce::File defaultRootDirectory();

    juce::File getRoot() const              { return root; }
    juce::File getPresetsDir() const        { return root.getChildFile ("Presets"); }
    juce::File getUserExportDir() const     { return root.getChildFile ("Sequences").getChildFile ("User_Exported"); }
    juce::File getDragCacheDir() const      { return root.getChildFile ("Sequences").getChildFile ("Drag_Cache"); }

    bool ensureFolderStructure() const;

    struct FactoryPreset
    {
        juce::String category, fileName;
        const void* data = nullptr;
        size_t size = 0;
    };

    /** Copies embedded factory presets to disk (existing user files are kept unless overwrite). */
    int installFactoryPresets (const juce::Array<FactoryPreset>& presets, bool overwrite = false) const;

    juce::StringArray getCategories() const;
    juce::Array<juce::File> getPresets (const juce::String& category) const;
    juce::Array<juce::File> getAllPresets() const;

    bool loadPreset (const juce::File& file, Pattern& pattern, PresetMeta* meta = nullptr) const;

    /** Saves into Presets/<category>/<name>.json (overwrites). */
    juce::File savePreset (const Pattern& pattern, const PresetMeta& meta, const juce::String& category) const;

    /** One-click save into Sequences/User_Exported with a unique, timestamped name. */
    juce::File quickSaveSequence (const Pattern& pattern, PresetMeta meta) const;

    /** Removes Drag_Cache files older than `maxAgeHours`. */
    void cleanDragCache (int maxAgeHours = 24) const;

    static juce::String sanitiseName (const juce::String& name);

private:
    juce::File root;
};
} // namespace pam
