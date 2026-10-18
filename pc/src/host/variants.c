/* Enemy variants (variants.h, docs/ENEMIES.md "Enemy variants"): the built-in table and the
   mods/enemies.ini reader. */
#include "variants.h"
#include "actors.h"
#include "fileio.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void host_log(const char* fmt, ...);

/* Mutated Steve: Monster Steve's model on Tyrant 1's AI. Monster Steve's own code (en17.c) cannot die
   (an empty Die mode, nothing moves him there), kills the player in one blow below 120 hit points (his
   swing outright), drives room light 2 and the camera; it is written for the Antarctic chase. The
   Tyrant (en19.c, form 0) walks, swings, staggers, falls and dies anywhere (the randomizer's Tyrant).
   His death is the Tyrant's fall, played by the Tyrant's skeleton and mapped onto Steve's like every
   motion. Sounds: from Steve's room's bank (RM_9280), his steps, roar, axe and hurt cry for the Tyrant's steps,
   swing and fall (the Tyrant makes no sound when hit). */
static EnemyVariant table[VARIANT_MAX] = {
    { .id = 1, .name = "Mutated Steve", .type = 19, .mdlver = 0, .model = 19,   /* catalogue: "Monster Steve" */
      .attach_n = 2, .attach = { { 3, 9 }, { 4, 13 } },   /* his claw and his axe arm (en17.c bhEne17_SetLinkWork) */
      .hp = 1.3f, .armour = 1.0f, .damage = 1.25f, .speed = 1.0f, .scale = 1.0f,
      .sound_room = 9280, .hurt = 2,   /* his bank: 0 steps, 2 hurt (en17.c bhEne17_DamageAdd), 3 the axe striking, 7 a swing, 9 his roar */
      .sound_n = 3, .sound = { { 0, 0 }, { 8, 9 }, { 5, 3 } },   /* the Tyrant's steps, swing (motion 9) and fall (motion 18) */
      .wild = 1, .mild = 0, .heavy = 1, .big = 1, .stages = VARIANT_STAGES_ALL },
};
static int builtin_n = 1, n = 1;

static void make_ui(EnemyVariant* v) { snprintf(v->ui, sizeof(v->ui), "[C] %s", v->name); }

__attribute__((constructor)) static void builtin_init(void)
{
    for (int i = 0; i < builtin_n; i++) make_ui(&table[i]);
}

int variant_count(void) { return n; }
const EnemyVariant* variant_at(int i) { return i >= 0 && i < n ? &table[i] : NULL; }
const EnemyVariant* variant_by_id(int id)
{
    for (int i = 0; i < n; i++) if (table[i].id == id) return &table[i];
    return NULL;
}
void variants_reset_user(void) { n = builtin_n; }

/* ---------------------------------------------------------------- mods/enemies.ini */
static char* trim(char* s)
{
    char* e;
    while (isspace((unsigned char)*s)) s++;
    for (e = s + strlen(s); e > s && isspace((unsigned char)e[-1]); e--);
    *e = 0;
    return s;
}

static int model_named(const char* s)   /* a catalogue entry by name or index, -1 */
{
    char* end;
    long k = strtol(s, &end, 10);
    if (*s && !*end) return k >= 0 && k < ACTOR_MODELS ? (int)k : -1;
    for (int i = 0; i < ACTOR_MODELS; i++)
        if (!strcasecmp(actor_models[i].name, s)) return i;
    return -1;
}

/* "a:b, c:d" into out[max][2] (each 0..lim); count or -1 */
static int pairs(const char* s, short (*out)[2], int max, int lim)
{
    int k = 0, a, b, m;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (sscanf(s, "%d : %d%n", &a, &b, &m) != 2 || k == max || a < 0 || b < 0 || a > lim || b > lim) return -1;
        out[k][0] = (short)a; out[k][1] = (short)b; k++;
        s += m;
    }
    return k;
}

/* "0-9" / "1,5,6" / "all" */
static int stage_bits(const char* s)
{
    int bits = 0, a, b, m;
    if (!strcasecmp(s, "all")) return VARIANT_STAGES_ALL;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (sscanf(s, "%d - %d%n", &a, &b, &m) == 2) {}
        else if (sscanf(s, "%d%n", &a, &m) == 1) b = a;
        else return -1;
        if (a < 0 || b > 9 || a > b) return -1;
        for (; a <= b; a++) bits |= 1 << a;
        s += m;
    }
    return bits;
}

static int number(const char* s, float* out, float lo, float hi)
{
    char* end;
    float f = strtof(s, &end);
    if (end == s || *trim(end) || f < lo || f > hi) return 0;
    *out = f;
    return 1;
}

/* one key of section v; 0 with *why set: not understood */
static int key(EnemyVariant* v, const char* k, char* val, const char** why, int* have_base)
{
    float f;
    short pr[16][2];
    int c, i;
    *why = "bad value";
    if (!strcmp(k, "id")) { if (!number(val, &f, VARIANT_USER_MIN, VARIANT_USER_MAX) || f != (int)f) { *why = "id must be 100..999"; return 0; } v->id = (unsigned short)f; return 1; }
    if (!strcmp(k, "base")) {
        int t, ver = 0, m;
        if (sscanf(val, "%d%n", &t, &m) == 1 && (!val[m] || sscanf(val + m, ".%d", &ver) == 1)) {
            if (t < 1 || t > 99 || ver < 0 || ver > 63) return 0;
            v->type = (unsigned char)t; v->mdlver = (unsigned char)ver;
        } else {
            if ((m = model_named(val)) < 0 || actor_models[m].id > 30) { *why = "no enemy of that name in the spawner list"; return 0; }
            v->type = actor_models[m].id; v->mdlver = actor_models[m].mdlver;
        }
        *have_base = 1;
        return 1;
    }
    if (!strcmp(k, "model")) {
        if (!strcasecmp(val, "none")) { v->model = -1; return 1; }
        if ((v->model = (short)model_named(val)) < 0) { *why = "no \"You appear as\" model of that name"; return 0; }
        return 1;
    }
    if (!strcmp(k, "attach")) {
        if ((c = pairs(val, pr, 4, 63)) < 0) return 0;
        for (i = 0; i < c; i++) { v->attach[i][0] = (unsigned char)pr[i][0]; v->attach[i][1] = (unsigned char)pr[i][1]; }
        v->attach_n = (unsigned char)c;
        return 1;
    }
    if (!strcmp(k, "hp")) return number(val, &v->hp, 0.05f, 50.0f);
    if (!strcmp(k, "armour") || !strcmp(k, "armor")) return number(val, &v->armour, 0.0f, 20.0f);
    if (!strcmp(k, "damage")) return number(val, &v->damage, 0.0f, 20.0f);
    if (!strcmp(k, "speed")) return number(val, &v->speed, 0.5f, 2.0f);
    if (!strcmp(k, "scale")) return number(val, &v->scale, 0.25f, 4.0f);
    if (!strcmp(k, "motions")) {
        if ((c = pairs(val, v->motion, 8, 511)) < 0) return 0;
        v->motion_n = (unsigned char)c;
        return 1;
    }
    if (!strcmp(k, "sounds")) { if (!number(val, &f, 0, 9999) || f != (int)f) return 0; v->sound_room = (unsigned short)f; return 1; }
    if (!strcmp(k, "sound_map")) {
        if ((c = pairs(val, pr, 16, 255)) < 0) return 0;
        for (i = 0; i < c; i++) { v->sound[i][0] = (unsigned char)pr[i][0]; v->sound[i][1] = (unsigned char)pr[i][1]; }
        v->sound_n = (unsigned char)c;
        return 1;
    }
    if (!strcmp(k, "stages")) { if ((c = stage_bits(val)) < 0) return 0; v->stages = (unsigned short)c; return 1; }
    {   /* small whole numbers */
        static const struct { const char* k; size_t off; int max; } u8[] = {
            { "wild", offsetof(EnemyVariant, wild), 20 }, { "mild", offsetof(EnemyVariant, mild), 20 },
            { "slots", offsetof(EnemyVariant, slots), 64 }, { "big", offsetof(EnemyVariant, big), 1 },
            { "heavy", offsetof(EnemyVariant, heavy), 1 }, { "hurt_sound", offsetof(EnemyVariant, hurt), 255 },
        };
        for (i = 0; i < (int)(sizeof(u8) / sizeof(u8[0])); i++)
            if (!strcmp(k, u8[i].k)) {
                if (!number(val, &f, 0, (float)u8[i].max) || f != (int)f) return 0;
                *((unsigned char*)v + u8[i].off) = (unsigned char)f;
                return 1;
            }
    }
    *why = "unknown key";
    return 0;
}

static void say(void (*log)(int, const char*), int line, const char* fmt, const char* a)
{
    char m[160];
    if (!log) return;
    snprintf(m, sizeof(m), fmt, a);
    log(line, m);
}

/* the section in progress: kept if it has an id and a base */
static int finish(EnemyVariant* v, int have_base, int line, void (*log)(int, const char*))
{
    if (!v->name[0]) return 0;
    if (!v->id || !have_base) { say(log, line, "[%s] left out: it needs an id and a base", v->name); return 0; }
    if (variant_by_id(v->id)) { say(log, line, "[%s] left out: its id is taken", v->name); return 0; }
    if (n == VARIANT_MAX) { say(log, line, "[%s] left out: too many enemies", v->name); return 0; }
    v->user = 1;
    make_ui(v);
    table[n++] = *v;
    return 1;
}

int variants_parse(const char* text, void (*log)(int line, const char* msg))
{
    EnemyVariant v;
    int added = 0, line = 0, have_base = 0, start = 0;
    const char* p = text;
    memset(&v, 0, sizeof(v));
    while (p && *p) {
        char buf[512], *s, *eq;
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, p, len);
        buf[len] = 0;
        p = nl ? nl + 1 : NULL;
        line++;
        if ((s = strpbrk(buf, ";#"))) *s = 0;   /* comments */
        s = trim(buf);
        if (!*s) continue;
        if (*s == '[') {
            char* e = strchr(s, ']');
            added += finish(&v, have_base, start, log);
            memset(&v, 0, sizeof(v));
            have_base = 0;
            start = line;
            if (!e || e == s + 1) { say(log, line, "bad section name %s", s); continue; }
            *e = 0;
            snprintf(v.name, sizeof(v.name), "%s", trim(s + 1));
            v.model = -1;
            v.hp = v.armour = v.damage = v.speed = v.scale = 1.0f;
            v.stages = VARIANT_STAGES_ALL;
            continue;
        }
        if (!(eq = strchr(s, '=')) || !v.name[0]) { say(log, line, "not a \"key = value\" line in a [section]: %s", s); continue; }
        *eq = 0;
        {
            const char* why;
            char* k = trim(s);
            for (char* c = k; *c; c++) *c = (char)tolower((unsigned char)*c);
            if (!key(&v, k, trim(eq + 1), &why, &have_base)) {
                char m[160];
                snprintf(m, sizeof(m), "[%s] %s: %s", v.name, k, why);
                if (log) log(line, m);
            }
        }
    }
    added += finish(&v, have_base, start, log);
    return added;
}

static void ini_log(int line, const char* msg) { host_log("enemies.ini line %d: %s\n", line, msg); }

void variants_load(void)
{
    const char* dir = getenv("RECVX_MODS") ? getenv("RECVX_MODS") : "mods";
    char path[1100], *text;
    FILE* f;
    long size;
    snprintf(path, sizeof(path), "%s/enemies.ini", dir);
    if (!(f = pc_fopen(path, "rb"))) return;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > (1 << 20) || !(text = calloc(1, (size_t)size + 1))) { fclose(f); return; }
    if (fread(text, 1, (size_t)size, f) == (size_t)size) {
        int k = variants_parse(text, ini_log);
        host_log("enemies: %s: %d custom enemies\n", path, k);
    }
    free(text);
    fclose(f);
}
