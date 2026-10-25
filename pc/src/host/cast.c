/* The cutscene cast randomizer's choices (cast.h; docs/ACTORS.md "Cutscene cast"). */
#include "cast.h"
#include "actors.h"
#include <string.h>

/* the characters' cutscene models (a face mask: their faces move with the lip sync), in character order */
static const signed char cine[CAST_CHARACTERS] = { 0, 1, 3, 5, 8, 12, 13, 14 };

int cast_character(int id, int ver)
{
    (void)ver;
    switch (id) {
    case 91: return 0;                       /* Claire */
    case 92: case 42: return 1;              /* Chris */
    case 93: case 43: return 2;              /* Steve */
    case 97: case 49: case 47: return 3;     /* Wesker */
    case 94: case 96: case 44: return 4;     /* Alfred (16, his boss type, is not cast) */
    case 95: return 5;                       /* Alexia (12..14, her forms, are not) */
    case 98: return 6;                       /* Rodrigo */
    case 99: return 7;                       /* the man in the hazmat suit */
    default: return -1;
    }
}

int cast_enemy_kind(int id)
{
    return id == 1 || id == 51 || id == 52 || id == 26 ? CASTK_ZOMBIE : id == 5 ? CASTK_HUNTER : -1;   /* zombies (scripted ones too), the anatomist; Hunters */
}

int cast_look(int id, int ver)
{
    for (int i = 0; i < ACTOR_MODELS; i++)
        if (actor_models[i].id == id && actor_models[i].mdlver == ver) return i;
    return -1;
}

static int listed(void) { return __atomic_load_n(&actor_import_n, __ATOMIC_ACQUIRE); }   /* (the main thread appends) */
static int imports_n(int imports) { return imports > 0 && imports <= listed() ? imports : 0; }

int cast_pool(int kind, int all, int imports, int* out, int max)
{
    int n = 0, i;
    if (kind == CASTK_CHAR && !all)
        for (i = 0; i < CAST_CHARACTERS && n < max; i++) out[n++] = cine[i];
    else if (kind == CASTK_CHAR) {   /* every humanoid look the menus list, the player files too; not the wounded Alfred (a special state) */
        for (i = 0; i < 5 && n < max; i++) out[n++] = ACTOR_PART + i;
        for (i = 0; i < ACTOR_MODELS && n < max; i++)
            if (actor_models[i].anim != ANIM_IDLE && appear_canon(COOP_CHR_ACTOR + i) == COOP_CHR_ACTOR + i && i != 10) out[n++] = i;
    } else
        for (i = 0; i < ACTOR_MODELS && n < max; i++)
            if (cast_enemy_kind(actor_models[i].id) == kind) out[n++] = i;
    if (kind == CASTK_CHAR)
        for (i = 0; i < imports_n(imports) && n < max; i++) out[n++] = ACTOR_MODELS + i;
    return n;
}

/* the story character a model shows (a pick of the same one is no change), -1 */
static int model_char(int m)
{
    static const signed char part[5] = { 0, 1, 2, 3, 0 };   /* player files: Claire, Chris, Steve, Wesker, Claire's second costume */
    if (m >= ACTOR_PART && m < ACTOR_PART + 5) return part[m - ACTOR_PART];
    for (int c = 0; c < CAST_CHARACTERS; c++) if (cine[c] == m) return c;
    return -1;
}

static unsigned rng;
static unsigned rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }   /* xorshift32, as the enemy randomizer's */

#define POOL_MAX (ACTOR_MODELS + 5 + ACTOR_IMPORT_MAX)
static int pool[POOL_MAX], keys[POOL_MAX], pick[POOL_MAX];
static unsigned char used[POOL_MAX];
static int kind_now;
static int own(int key, int m) { return kind_now == CASTK_CHAR ? model_char(m) == key : m == key; }   /* m is key's own look */

int cast_model(unsigned seed, unsigned scene, int kind, int key, int all, int imports)
{
    int n = cast_pool(kind, all, imports, pool, POOL_MAX), nk = 0, i, j, k, t;
    kind_now = kind;
    if (kind == CASTK_CHAR) for (nk = 0; nk < CAST_CHARACTERS; nk++) keys[nk] = nk;
    else for (i = 0; i < n; i++) keys[nk++] = pool[i];   /* each look of the kind, in catalogue order */
    for (j = 0; j < nk && keys[j] != key; j++);
    if (j == nk || n < 2) return -1;
    rng = (seed * 2654435761u) ^ (scene * 4u + (unsigned)kind + 0x9E3779B9u);
    if (!rng) rng = 1;
    rnd(); rnd();
    for (i = n - 1; i > 0; i--) { k = (int)(rnd() % (unsigned)(i + 1)); t = pool[i]; pool[i] = pool[k]; pool[k] = t; }   /* Fisher-Yates */
    memset(used, 0, (size_t)n);
    /* every key in turn (all of them, so each answer is the same whichever key is asked): the first model of the
       shuffle not taken and not its own; when only its own are left, it trades with an earlier key whose model is
       not its own and who may take one of those left; with the pool used up, the first not its own again */
    for (i = 0; i < nk; i++) {
        pick[i] = -1;
        for (k = 0; k < n && pick[i] < 0; k++) if (!used[k] && !own(keys[i], pool[k])) pick[i] = k;
        for (k = 0; k < n && pick[i] < 0; k++) {
            if (used[k]) continue;   /* a model left (its own): to an earlier key it suits */
            for (t = 0; t < i && pick[i] < 0; t++)
                if (!own(keys[t], pool[k]) && !own(keys[i], pool[pick[t]])) { pick[i] = pick[t]; pick[t] = k; used[k] = 1; }
        }
        for (k = 0; k < n && pick[i] < 0; k++) if (!own(keys[i], pool[k])) pick[i] = k;
        if (pick[i] < 0) return -1;
        used[pick[i]] = 1;
    }
    return pool[pick[j]];
}

unsigned cast_import_id(void)
{
    static int last = -1;
    static unsigned id;
    unsigned h = 2166136261u;
    int n = listed();
    if (n == last) return id;   /* (append-only: the count names the list) */
    last = n;
    if (!n) return id = 0;
    for (int k = 0; k < n; k++) {
        for (const char* s = actor_imports[k].name; s && *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
        h = (h ^ (0x100u | actor_import_group[k])) * 16777619u;
    }
    return id = h ? h : 1;
}
