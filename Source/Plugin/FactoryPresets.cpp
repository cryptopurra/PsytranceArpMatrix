#include "FactoryPresets.h"
#include <BinaryData.h>

namespace pam
{
juce::Array<PresetManager::FactoryPreset> getFactoryPresets()
{
    juce::Array<PresetManager::FactoryPreset> list;

    for (int i = 0; i < PamPresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = PamPresets::getNamedResource (PamPresets::namedResourceList[i], size);
        if (data == nullptr || size <= 0)
            continue;

        // the category lives inside the JSON (binary data flattens folders)
        const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        auto category = json.getProperty ("category", "User").toString();

        PresetManager::FactoryPreset fp;
        fp.category = category.isNotEmpty() ? category : juce::String ("User");
        fp.fileName = PamPresets::originalFilenames[i];
        fp.data = data;
        fp.size = (size_t) size;
        list.add (fp);
    }
    return list;
}
} // namespace pam
