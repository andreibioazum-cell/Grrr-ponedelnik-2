/* Browser WebAudio stereo 44.1 kHz mixer hook: mixes real WAV music & SFX! */
static int snd_web_paused = 0;

void web_mix_audio_s16(int16_t *dst, int frames) {
    if (!dst || frames <= 0)
        return;
    if (snd_web_paused) {
        memset(dst, 0, (size_t)frames * 2u * sizeof(int16_t));
        return;
    }
    snd_frame(dst, frames);
}

int ds_sound_init(AAssetManager *assets) {
    snd_amgr = assets;
    snd_web_paused = 0;
    return 1;
}

void ds_sound_shutdown(void) { snd_release_sounds(); }
void ds_sound_suspend(void) { snd_web_paused = 1; }
void ds_sound_pause(void) { snd_web_paused = 1; }
void ds_sound_resume(void) { snd_web_paused = 0; }
void ds_sound_set_java_vm(void *vm) { (void)vm; }
