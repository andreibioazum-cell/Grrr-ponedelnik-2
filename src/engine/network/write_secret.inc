/* Секреты базы: «пароль на запись» и хвост пароля аккаунта.
 *
 * Аутентификации самой по себе мало. Пароль аккаунта = «пароль игрока + хвост»,
 * поэтому любой, кто знает хвост и публичный ключ API, может создать аккаунт с
 * чужим ником и писать в базу от его имени. Раньше хвостом была строка "|cb4v1"
 * — её угадывали за секунду. Теперь это 32 случайных символа.
 *
 * Второй барьер — поле "w", которое клиент добавляет в КАЖДОЕ тело записи:
 * правила firebase.rules.json отвергают любую запись, где
 * newData.child('w').val() не равен секрету записи. Скрипт, у которого есть
 * только ключ API, не пройдёт, даже залогинившись.
 *
 * Оба значения живут в engine/network.h кусками, каждый байт xor 0x5a: так
 * строку не находит grep по собранному apk. Новые значения генерирует
 * tools/rotate_db_secret.py — она же синхронно правит firebase.rules.json. */

static const char *net_secret(char *buf, size_t cap, const char *const *chunks, size_t count) {
    size_t i, at = 0;
    for (i = 0; i < count && at + 1 < cap; i++) {
        const char *p = chunks[i];
        for (; *p && at + 1 < cap; p++) {
            buf[at++] = (char)((unsigned char)*p ^ NET_SECRET_XOR);
            buf[at] = 0;
        }
    }
    buf[at] = 0;
    return buf;
}

/* Поле "w": то, что проверяют правила базы. */
static const char *net_write_secret(void) {
    static const char *chunks[] = {NET_WRITE_SECRET_0, NET_WRITE_SECRET_1, NET_WRITE_SECRET_2};
    static char secret[NET_WRITE_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Хвост пароля аккаунта. В базу не уходит никогда. */
static const char *net_pass_suffix(void) {
    static const char *chunks[] = {NET_PASS_SECRET_0, NET_PASS_SECRET_1, NET_PASS_SECRET_2};
    static char secret[NET_PASS_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Старый хвост пароля: по нему считали пароли всех аккаунтов, созданных до
 * перехода на NET_PASS_SECRET, и он уже утек в открытый репозиторий. Новые
 * аккаунты им не пользуются; старые пускают по нему ровно один раз и тут же
 * переписывают пароль на новый хвост (fb_update_password). Когда игроки
 * переедут — удалить эту строку, вызов fb_sign_in_legacy() и сам макрос. */
#define FB_LEGACY_PASS_SUFFIX "|cb4v1"

/* Добавить поле "w" в тело записи. Тело приходит как JSON-объект от движка,
 * поэтому достаточно убрать закрывающую скобку и дописать своё поле.
 * Секрет состоит только из букв и цифр, экранировать его не нужно. */
static int net_seal_body(char *body, size_t cap) {
    size_t n;
    const char *secret = net_write_secret();
    if (!body || cap < 4)
        return 0;
    n = strlen(body);
    if (n < 2 || body[n - 1] != '}')
        return 0;
    /* ,"w":" секрет "} и завершающий ноль; если не влезает — тело не трогаем */
    if (n + strlen(secret) + 8 > cap)
        return 0;
    body[--n] = 0;
    snprintf(body + n, cap - n, ",\"w\":\"%s\"}", secret);
    return 1;
}

/* Собрать оба секрета заранее, пока игра ещё в один поток. */
static void net_secret_prepare(void) {
    (void)net_write_secret();
    (void)net_pass_suffix();
}
