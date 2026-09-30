#include "ChainProcessor.h"

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new awchain::ChainProcessor(); }
