/* Audio loading and playback. */
#include "engine/audio/decode_mixer.inc"
#include "engine/audio/playback.inc"
#if defined(CB_WASM)
#include "platform/web/audio_output_web.inc"
#elif defined(CB_PC)
#include "platform/pc/audio_output_pc.inc"
#else
#include "platform/android/audio_output.inc"
#endif
