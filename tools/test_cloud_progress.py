#!/usr/bin/env python3
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from test_settings_storage import ANDROID_LOG_H, JNI_H

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "engine/network.c"

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt;
    return 0;
}
void ds_console_log(int is_error, const char *format, ...) { (void)is_error; (void)format; }

static const char *CLOUD_NO_EBUC =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":200,\"cls\":0,\"azum\":1,\"santa\":0,"
    "\"level\":0,\"levels\":0}";

static const char *CLOUD_FRESH =
    "{\"nick\":\"tester\",\"cups\":120,\"candies\":50,\"cls\":1,\"azum\":1,\"santa\":0,"
    "\"ebuc\":1,\"level\":0,\"levels\":0}";

static const char *CLOUD_NO_ASTRA =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":0,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"level\":0,\"levels\":0}";

static const char *CLOUD_LEGACY_ASTRA =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":5,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"astra\":0,\"azum_skin\":1,\"level\":2,\"levels\":3,\"astra_level\":2,"
    "\"astra_levels\":3}";

static int run_first_save(const char *dir) {
    net_set_data_path(dir);

    net_save_progress_all(7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(net_load_ebuc() == 1);
    assert(net_load_azum() == 1);
    assert(net_load_cups() == 7);
    puts("first save: classes bought earlier survive a save issued before any read");
    return 0;
}

static int run_offline_buy(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_ebuc() == 0);
    net_save_progress_all(100, 50, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(cloud_dirty_get() == 1);
    int push = apply_user_json_keep_local(CLOUD_NO_EBUC);
    assert(push == 1);
    assert(net_load_ebuc() == 1);
    assert(net_load_class() == 3);
    assert(net_load_candies() == 50);
    assert(net_load_cups() == 100);
    puts("offline buy: the ebuC, its selection and the spent candies come back from the device");
    return 0;
}

static int run_clean(const char *dir) {
    net_set_data_path(dir);
    net_save_progress_all(100, 50, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    cloud_dirty_set(0);
    int push = apply_user_json_keep_local(CLOUD_FRESH);
    assert(push == 0);
    assert(net_load_class() == 1);
    assert(net_load_cups() == 120);
    assert(net_load_ebuc() == 1);
    puts("clean: with no dirty mark a fresher cloud record wins, ownership intact");
    return 0;
}

static int run_astra(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_astra() == 0);
    net_save_astra(1, 3, 3);
    net_save_progress_all(90, 20, 4, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 3);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_class() == 4);
    assert(apply_user_json_keep_local(CLOUD_NO_ASTRA) == 1);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 3);
    assert(net_load_class() == 4);
    puts("astra: an offline purchase, selection and levels survive an older cloud profile");
    return 0;
}

static int run_legacy_local(const char *dir) {
    net_set_data_path(dir);
    char path[320];
    snprintf(path, sizeof(path), "%s/progress.dat", dir);
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    /* 18-field save from before per-class Astra progress existed. */
    fputs("100 5 0 0 20 2 3 0 0 0 0 0 0 0 0 0 0 0\n", f);
    fclose(f);
    assert(net_load_class() == 4);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 2);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_level() == 2 && net_load_levels_unlocked() == 3);
    net_save_progress_all(100, 20, 4, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    puts("legacy local save: class ID 5 is rewritten as ordinary Astra");
    return 0;
}

static int run_legacy_profile(const char *dir) {
    net_set_data_path(dir);
    assert(apply_user_json_keep_local(CLOUD_LEGACY_ASTRA) == 0);
    assert(net_load_class() == 4);
    assert(pending_skin == 0);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 2);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_level() == 2 && net_load_levels_unlocked() == 3);

    net_save_progress_all(80, 10, 5, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    net_save_astra(0, 1, 1);
    cloud_dirty_set(1);
    assert(apply_user_json_keep_local(CLOUD_NO_ASTRA) == 1);
    assert(net_load_class() == 4 && net_load_astra() == 1);
    puts("legacy Astra class: class ID 5 migrates to the ordinary Astra and survives a dirty local merge");
    return 0;
}

static int run_admin_ban_guard(const char *dir) {
    net_set_data_path(dir);
    net_ban_set("Dimasi4ek229", 1);
    net_ban_set("QWERTYUIOPAJ1234", 1);
    assert(net_is_banned("Dimasi4ek229") == 0);
    assert(net_is_banned("qwertyuiopaj1234") == 0);
    char path[320];
    snprintf(path, sizeof(path), "%s/bans.dat", dir);
    FILE *f = fopen(path, "r");
    assert(f == NULL);

    net_ban_set("ordinary_player", 1);
    assert(net_is_banned("ordinary_player") == 1);
    net_ban_set("ordinary_player", 0);
    assert(net_is_banned("ordinary_player") == 0);
    puts("moderation: neither administrator can be banned; ordinary bans still work");
    return 0;
}

static int run_text_cmd(const char *dir) {
    (void)dir;
    /* Canonical quoted form: text "the banner text" [color]. */
    assert(net_chat_is_text_cmd("text \"Sale is on\" red") == 1);
    assert(strcmp(net_chat_text_cmd_text("text \"Sale is on\" red"), "Sale is on") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"Sale is on\" red"), "red") == 0);
    assert(strcmp(net_chat_text_cmd_text("text \"just text\""), "just text") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"just text\""), "white") == 0);
    /* A color word inside the quotes is text, not a color. */
    assert(strcmp(net_chat_text_cmd_text("text \"red alert\""), "red alert") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"red alert\""), "white") == 0);
    /* Legacy unquoted form still works. */
    assert(strcmp(net_chat_text_cmd_text("text hello blue"), "hello") == 0);
    assert(strcmp(net_chat_text_cmd_color("text hello blue"), "blue") == 0);
    assert(strcmp(net_chat_text_cmd_text("text hello"), "hello") == 0);
    /* Empty or unterminated quotes are not commands. */
    assert(net_chat_is_text_cmd("text \"\" red") == 0);
    assert(net_chat_is_text_cmd("text \"oops") == 0);
    assert(net_chat_is_text_cmd("text") == 0);
    puts("text cmd: quoted text, trailing color, legacy form OK");
    return 0;
}

static int run_rooms_parse(const char *dir) {
    (void)dir;
    char names[16][25];
    int n = parse_room_names("{\"main\":true,\"dima-room\":true,\"bad name!\":true,\"ok_2\":false}", names, 16);
    assert(n == 3);
    assert(strcmp(names[0], "main") == 0 && strcmp(names[1], "dima-room") == 0 && strcmp(names[2], "ok_2") == 0);
    assert(parse_room_names("null", names, 16) == 0);
    assert(room_name_valid("main") == 1 && room_name_valid("") == 0 && room_name_valid("bad name") == 0);
    char h1[16], h2[16], h3[16];
    room_pass_hash("s3cret", h1, sizeof(h1));
    room_pass_hash("s3cret", h2, sizeof(h2));
    room_pass_hash("other", h3, sizeof(h3));
    assert(strcmp(h1, h2) == 0 && strcmp(h1, h3) != 0 && strlen(h1) == 8);
    room_pass_hash("", h1, sizeof(h1));
    assert(strcmp(h1, "00000000") == 0); /* empty password = open room */
    puts("rooms: name parsing/validation and password hash OK");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: test <mode> <dir>\n"); return 2; }
    if (strcmp(argv[1], "first-save") == 0) return run_first_save(argv[2]);
    if (strcmp(argv[1], "offline-buy") == 0) return run_offline_buy(argv[2]);
    if (strcmp(argv[1], "clean") == 0) return run_clean(argv[2]);
    if (strcmp(argv[1], "astra") == 0) return run_astra(argv[2]);
    if (strcmp(argv[1], "legacy-local") == 0) return run_legacy_local(argv[2]);
    if (strcmp(argv[1], "legacy-profile") == 0) return run_legacy_profile(argv[2]);
    if (strcmp(argv[1], "admin-ban-guard") == 0) return run_admin_ban_guard(argv[2]);
    if (strcmp(argv[1], "text-cmd") == 0) return run_text_cmd(argv[2]);
    if (strcmp(argv[1], "rooms-parse") == 0) return run_rooms_parse(argv[2]);
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
'''

def check_class_rules():
    rules = json.loads((ROOT / "firebase.rules.json").read_text(encoding="utf-8"))["rules"]
    validators = [
        rules["users"]["$nick"]["cls"][".validate"],
        rules["rooms"]["$room"]["players"]["$slot"]["cls"][".validate"],
    ]
    for rule in validators:
        assert "newData.val() <= 4" in rule and "newData.val() == 6" in rule
        assert "newData.val() == 5" not in rule
        assert rule.count("(") == rule.count(")")
        assert "auth != null" in rule
        assert "dimasi4ek229@cb4.game" in rule
        assert "qwertyuiopaj1234@cb4.game" in rule


def main():
    check_class_rules()
    with tempfile.TemporaryDirectory(prefix="cubic-cloud-") as directory:
        temp = Path(directory)
        android = temp / "android"
        android.mkdir()
        (android / "log.h").write_text(ANDROID_LOG_H)
        (android / "asset_manager.h").write_text(
            "typedef struct AAssetManager AAssetManager;\n")
        (android / "native_window.h").write_text("typedef struct ANativeWindow ANativeWindow;\n")
        (temp / "jni.h").write_text(JNI_H)
        (temp / "test.c").write_text(HARNESS)
        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O0",
            "-D_POSIX_C_SOURCE=200809L", "-D__ANDROID__",
            "-Werror=implicit-function-declaration",
            "-I", str(temp), "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(temp / "test"),
        ], check=True)
        run = [str(temp / "test")]


        first = temp / "first"
        first.mkdir()
        (first / "progress.dat").write_text(
            "10 3 1 0 5 0 0 0 0 0 0 0 0 0 0 1 0 0\n", encoding="utf-8")
        subprocess.run([*run, "first-save", str(first)], check=True)
        saved = (first / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[2] == "1" and saved[15] == "1", saved

        offline = temp / "offline"
        offline.mkdir()
        subprocess.run([*run, "offline-buy", str(offline)], check=True)
        assert (offline / "progress.dirty").exists()

        clean = temp / "clean"
        clean.mkdir()
        subprocess.run([*run, "clean", str(clean)], check=True)
        assert not (clean / "progress.dirty").exists()

        astra = temp / "astra"
        astra.mkdir()
        subprocess.run([*run, "astra", str(astra)], check=True)
        saved = (astra / "progress.dat").read_text(encoding="utf-8").split()
        assert len(saved) == 22 and saved[18:] == ["1", "3", "3", "0"], saved

        legacy_local = temp / "legacy-local"
        legacy_local.mkdir()
        subprocess.run([*run, "legacy-local", str(legacy_local)], check=True)
        saved = (legacy_local / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[1] == "4" and saved[18] == "1", saved

        legacy_class = temp / "legacy-class"
        legacy_class.mkdir()
        subprocess.run([*run, "legacy-profile", str(legacy_class)], check=True)
        saved = (legacy_class / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[1] == "4" and saved[18] == "1", saved

        bans = temp / "bans"
        bans.mkdir()
        subprocess.run([*run, "admin-ban-guard", str(bans)], check=True)

        textcmd = temp / "textcmd"
        textcmd.mkdir()
        subprocess.run([*run, "text-cmd", str(textcmd)], check=True)

        roomsdir = temp / "roomsdir"
        roomsdir.mkdir()
        subprocess.run([*run, "rooms-parse", str(roomsdir)], check=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
