#pragma once

#include "IO/PresetManager.h"

namespace pam
{
/** Factory presets embedded with juce_add_binary_data (Resources/PsytranceArpMatrix/Presets). */
juce::Array<PresetManager::FactoryPreset> getFactoryPresets();
}
