#pragma once

namespace awchain
{
// The effects worth reaching for on a voice: compressors and gates, de-essers,
// air and slew, channel strips and console colour, tape, plates and rooms,
// doublers, and the filters a vocal chain actually uses. A hand-picked list,
// mine and not Chris's; he tags nothing as "vocal". Edit it freely.
inline constexpr const char *kForVocals[] = {
    // dynamics
    "Pressure5", "Pressure4", "Compresaturator", "ButterComp2", "Recurve", "Pyewacket", "VariMu", "Logical4",
    "SoftGate", "Gatelope", "PodcastDeluxe", "Podcast", "StoneFireComp", "BeziComp", "PurestSquish", "curve",
    "DigitalBlack",
    // clipping
    "ClipOnly3", "ADClip8", "OneCornerClip", "ClipSoftly", "AQuickVoiceClip",
    // brightness, de-essing
    "DeBess", "DeEss", "DeHiss", "Air4", "Air3", "Air2", "Slew2", "Slew3", "Slew4", "SlewSonic", "Acceleration2",
    "Sinew", "Smooth",
    // eq and filters
    "Parametric", "PearEQ", "PearLiteEQ", "SmoothEQ3", "FatEQ", "BezEQ3", "Isolator3", "Capacitor2", "Highpass2",
    "Lowpass2", "ResEQ2", "ToneSlant", "Baxandall2", "ZHighpass2", "ZLowpass2", "XHighpass",
    // tone colour and channels
    "Apicolypse", "Neverland", "Elation", "Precious", "Channel9", "Luxor", "Cider", "Calibre", "Crystal",
    "BussColors4", "Console9Channel", "ConsoleLAChannel", "ConsoleMCChannel", "PurestConsoleChannel",
    "Console0Channel",
    // saturation, warmth, distance
    "Density2", "Density", "Drive", "Tube2", "PurestWarm3", "Mojo", "Spiral2", "Focus", "UnBox", "Creature",
    "Hypersoft", "Inflamer", "Distance3", "Distance2", "Interstage", "Coils2", "Desk4", "TubeDesk", "TransDesk",
    "Discontapeity", "Discontinuity", "SingleEndedTriode", "Hype", "Shape", "Sweeten", "Energy2", "Hombre",
    // tape
    "ToTape8", "ToTape9", "Dubly3", "IronOxide5", "TapeDust", "Flutter2",
    // doubling, chorus, delay
    "Doublelay", "TripleSpread", "StereoDoubler", "ADT", "Chorus", "ChorusEnsemble", "Ensemble", "Vibrato",
    "Melt", "TapeDelay2", "Srsly3",
    // reverb
    "kPlate140", "kPlate240", "kPlateA", "kPlateB", "kPlateC", "kPlateD", "Galactic3", "Galactic", "ClearCoat",
    "CreamCoat", "CrunchCoat", "Chamber", "Verbity2", "MatrixVerb", "kCathedral5", "kStation", "kWoodRoom",
    "kBeyond", "NonlinearSpace", "VerbTiny", "Dattorro",
    // utility and the odd one
    "PurestGain", "PurestFade", "EveryTrim", "DeNoise", "VoiceTrick", "LeadAmp",
};
} // namespace awchain
