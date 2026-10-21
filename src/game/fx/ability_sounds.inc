/* Звуки способностей. У каждой способности свой короткий эффект, файлы
 * лежат в assets/audio и синтезируются tools/gen_ability_sounds.py:
 *   удар            punch.wav
 *   рывок Азума     dash.wav
 *   захват Астры    grab.wav
 *   подарок Санты   gift.wav
 *   станция буКа    station.wav
 *   оверклок        overclock.wav
 * У зомби-скина Азума свои, кастомные: удар рычит, рывок стонет
 * (zombie_punch.wav / zombie_dash.wav). Вызывается из точек запуска
 * способностей (start_punch_now, start_dash_now, start_grab_now,
 * fire_super), поэтому онлайн-соперник свои способности не озвучивает —
 * звук только у локального игрока. */

/* Проиграть эффект способности. Загрузка — ленивая и кешируемая самим
 * snd_load: если файла нет, звук просто молчит (snd_play не вызывается). */
static void ability_snd(const char *name) {
    if (snd_load(name) == 1) {
        snd_play(name);
    }
}

/* Предзагрузка на старте и громкость чуть ниже музыки интерфейса, чтобы
 * первый удар в бою не дёргал диск и не бил по ушам. */
static void ability_sounds_init(void) {
    const char *names[] = {PUNCH_SOUND, DASH_SOUND, GRAB_SOUND, GIFT_SOUND,
                           STATION_SOUND, OVERCLOCK_SOUND, ZOMBIE_PUNCH_SOUND, ZOMBIE_DASH_SOUND};
    int i = 0;
    while (i < 8) {
        if (snd_load(names[i]) == 1) {
            snd_volume(names[i], 0.8);
        }
        i = i + 1;
    }
}

static double player_is_zombie_azum(void) {
    return player_class == CLASS_AZUM && current_skin() == SKIN_ZOMBIE ? 1 : 0;
}

static void punch_snd(void) {
    ability_snd(player_is_zombie_azum() == 1 ? ZOMBIE_PUNCH_SOUND : PUNCH_SOUND);
}

static void dash_snd(void) {
    ability_snd(player_is_zombie_azum() == 1 ? ZOMBIE_DASH_SOUND : DASH_SOUND);
}
