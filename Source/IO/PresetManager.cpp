#include "PresetManager.h"

namespace pam
{
PresetManager::PresetManager (juce::File rootDirectory) : root (std::move (rootDirectory)) {}

juce::File PresetManager::defaultRootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PsytranceArpMatrix");
}

bool PresetManager::ensureFolderStructure() const
{
    bool ok = true;
    for (auto& c : factoryCategories())
        ok &= getPresetsDir().getChildFile (c).createDirectory().wasOk();
    ok &= getUserExportDir().createDirectory().wasOk();
    ok &= getDragCacheDir().createDirectory().wasOk();
    return ok;
}

int PresetManager::installFactoryPresets (const juce::Array<FactoryPreset>& presets, bool overwrite) const
{
    ensureFolderStructure();
    int written = 0;
    for (auto& fp : presets)
    {
        auto dir = getPresetsDir().getChildFile (sanitiseName (fp.category));
        dir.createDirectory();
        auto f = dir.getChildFile (fp.fileName);
        if (f.existsAsFile() && ! overwrite)
            continue;
        if (f.replaceWithData (fp.data, fp.size))
            ++written;
    }
    return written;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (auto& d : getPresetsDir().findChildFiles (juce::File::findDirectories, false))
        cats.add (d.getFileName());
    cats.sort (true);
    return cats;
}

juce::Array<juce::File> PresetManager::getPresets (const juce::String& category) const
{
    auto files = getPresetsDir().getChildFile (category).findChildFiles (juce::File::findFiles, false, "*.json");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

juce::Array<juce::File> PresetManager::getAllPresets() const
{
    juce::Array<juce::File> all;
    for (auto& c : getCategories())
        all.addArray (getPresets (c));
    all.addArray (getUserExportDir().findChildFiles (juce::File::findFiles, false, "*.json"));
    return all;
}

bool PresetManager::loadPreset (const juce::File& file, Pattern& pattern, PresetMeta* meta) const
{
    if (! file.existsAsFile())
        return false;
    return PatternSerializer::fromJson (file.loadFileAsString(), pattern, meta);
}

juce::String PresetManager::sanitiseName (const juce::String& name)
{
    auto s = name.trim().replaceCharacters (" /\\:*?\"<>|", "__________");
    s = s.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-#.");
    return s.isEmpty() ? juce::String ("Untitled") : s;
}

juce::File PresetManager::savePreset (const Pattern& pattern, const PresetMeta& metaIn, const juce::String& category) const
{
    auto meta = metaIn;
    meta.category = category;
    auto dir = getPresetsDir().getChildFile (sanitiseName (category));
    dir.createDirectory();
    auto f = dir.getChildFile (sanitiseName (meta.name) + ".json");
    return f.replaceWithText (PatternSerializer::toJson (pattern, meta), false, false, "\n") ? f : juce::File();
}

juce::File PresetManager::quickSaveSequence (const Pattern& pattern, PresetMeta meta) const
{
    getUserExportDir().createDirectory();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    if (meta.name.isEmpty())
        meta.name = "Sequence";
    meta.category = "User_Exported";
    auto f = getUserExportDir().getNonexistentChildFile (sanitiseName (meta.name) + "_" + stamp, ".json", false);
    return f.replaceWithText (PatternSerializer::toJson (pattern, meta), false, false, "\n") ? f : juce::File();
}

void PresetManager::cleanDragCache (int maxAgeHours) const
{
    const auto limit = juce::Time::getCurrentTime() - juce::RelativeTime::hours (maxAgeHours);
    for (auto& f : getDragCacheDir().findChildFiles (juce::File::findFiles, true, "*.mid"))
        if (f.getLastModificationTime() < limit)
            f.deleteFile();
}
} // namespace pam
