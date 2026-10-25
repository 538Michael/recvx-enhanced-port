/* The cutscene cast randomizer's picks (pc/src/host/cast.c): deterministic from (seed, scene, kind), never a
   character's own look, different characters different models, "Random per cutscene" picking again per room. */
#include "../src/host/cast.h"
#include "../src/host/actors.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

static int in(const int* p, int n, int v) { for (int i = 0; i < n; i++) if (p[i] == v) return 1; return 0; }

int main(void)
{
    int cine[16], all[128], zom[64], hun[8], nc = cast_pool(CASTK_CHAR, 0, 0, cine, 16), na = cast_pool(CASTK_CHAR, 1, 0, all, 128);
    int nz = cast_pool(CASTK_ZOMBIE, 0, 0, zom, 64), nh = cast_pool(CASTK_HUNTER, 0, 0, hun, 8), c, s, scene, differ = 0;
    CHECK(nc == CAST_CHARACTERS && nz == 28 && nh == 2 && na > 40);   /* 8 cutscene models; 27 zombie looks and the anatomist; Hunter, Sweeper */
    CHECK(cine[0] == 0 && cine[4] == 8 && cine[6] == 13);            /* Claire's, Alfred's, Rodrigo's cutscene models */
    CHECK(!in(all, na, 10) && !in(all, na, 2) && in(all, na, ACTOR_PART + 4) && in(all, na, 17));   /* no wounded Alfred, no twin; player files, Tyrant */
    for (int i = 0; i < na; i++) CHECK(all[i] >= ACTOR_PART || actor_models[all[i]].anim != ANIM_IDLE);   /* humanoids only */
    /* who is cast */
    CHECK(cast_character(91, 0) == 0 && cast_character(43, 0) == 2 && cast_character(96, 1) == 4 && cast_character(98, 0) == 6);
    CHECK(cast_character(16, 0) < 0 && cast_character(12, 0) < 0 && cast_character(1, 0) < 0 && cast_character(68, 0) < 0);
    CHECK(cast_enemy_kind(1) == CASTK_ZOMBIE && cast_enemy_kind(52) == CASTK_ZOMBIE && cast_enemy_kind(26) == CASTK_ZOMBIE && cast_enemy_kind(5) == CASTK_HUNTER);
    CHECK(cast_enemy_kind(19) < 0 && cast_enemy_kind(15) < 0 && cast_enemy_kind(4) < 0 && cast_look(1, 2) == 38 && cast_look(1, 99) < 0);
    for (s = 0; s < 50; s++) {
        int got[CAST_CHARACTERS];
        for (c = 0; c < CAST_CHARACTERS; c++) {   /* fixed: a permutation of the cutscene models, nobody as itself */
            got[c] = cast_model((unsigned)s, 0, CASTK_CHAR, c, 0, 0);
            CHECK(in(cine, nc, got[c]) && got[c] != cine[c] && got[c] == cast_model((unsigned)s, 0, CASTK_CHAR, c, 0, 0));
            for (int d = 0; d < c; d++) CHECK(got[d] != got[c]);
            int a = cast_model((unsigned)s, 0, CASTK_CHAR, c, 1, 0);   /* all models: none of the character's own looks */
            CHECK(in(all, na, a) && a != cine[c] && (c > 3 || a != ACTOR_PART + c) && !(c == 0 && a == ACTOR_PART + 4));
        }
        for (scene = 1; scene < 20; scene++) {   /* random per cutscene: again in each room, the same for the same room */
            int m = cast_model((unsigned)s, (unsigned)scene * 10 + 1, CASTK_CHAR, 2, 0, 0);
            CHECK(m == cast_model((unsigned)s, (unsigned)scene * 10 + 1, CASTK_CHAR, 2, 0, 0) && m != cine[2]);
            differ += m != got[2];
        }
        for (int i = 0; i < nz; i++) {   /* each zombie look as another; distinct while the pool lasts */
            int m = cast_model((unsigned)s, 0, CASTK_ZOMBIE, zom[i], 0, 0);
            CHECK(in(zom, nz, m) && m != zom[i]);
        }
        CHECK(cast_model((unsigned)s, 0, CASTK_HUNTER, 20, 0, 0) == 21 && cast_model((unsigned)s, 0, CASTK_HUNTER, 21, 0, 0) == 20);
    }
    CHECK(differ > 50 * 19 / 2);   /* most rooms pick another model than the fixed one */
    CHECK(cast_model(1, 0, CASTK_CHAR, 8, 0, 0) < 0 && cast_model(1, 0, CASTK_ZOMBIE, 0, 0, 0) < 0);   /* not a key */
    {   /* the seed decides: two seeds differ somewhere */
        int d = 0;
        for (c = 0; c < CAST_CHARACTERS; c++) d += cast_model(1, 0, CASTK_CHAR, c, 0, 0) != cast_model(2, 0, CASTK_CHAR, c, 0, 0);
        CHECK(d > 0);
    }
    /* imported models: only when asked for, then in both pools; the list's identity */
    CHECK(cast_import_id() == 0);
    actor_imports[0] = (ActorModel){ .name = "Kevin", .anim = ANIM_MALE };
    actor_imports[1] = (ActorModel){ .name = "Cindy", .anim = ANIM_FEMALE };
    actor_import_n = 2;
    {
        unsigned id = cast_import_id();
        int p[16], n = cast_pool(CASTK_CHAR, 0, 2, p, 16), seen = 0;
        CHECK(id != 0 && n == CAST_CHARACTERS + 2 && p[n - 1] == ACTOR_MODELS + 1 && cast_pool(CASTK_ZOMBIE, 0, 2, zom, 64) == 28);
        for (s = 0; s < 40; s++)
            for (c = 0; c < CAST_CHARACTERS; c++) seen |= cast_model((unsigned)s, 0, CASTK_CHAR, c, 0, 2) >= ACTOR_MODELS;
        CHECK(seen);
        for (s = 0; s < 40; s++)
            for (c = 0; c < CAST_CHARACTERS; c++) CHECK(cast_model((unsigned)s, 0, CASTK_CHAR, c, 0, 0) < ACTOR_MODELS);
        actor_imports[2] = (ActorModel){ .name = "Jim", .anim = ANIM_MALE };
        actor_import_n = 3;
        CHECK(cast_import_id() != id);
    }
    printf("cast: ok\n");
    return 0;
}
