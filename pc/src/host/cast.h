/* The cutscene cast randomizer (docs/ACTORS.md "Cutscene cast"): which room records are the cast and
   which "You appear as" model each one is drawn as. Pure functions over the catalogue (actors.c), so
   ctest "cast" checks the picks; the game side (drawing, loading, the hand pieces) is
   pc/glue/cast_glue.c. Visual only: types, scripts and enemy logic are never touched. */
#ifndef RECVX_CAST_H
#define RECVX_CAST_H
#ifdef __cplusplus
extern "C" {
#endif

enum { CAST_OFF, CAST_CHARS, CAST_ENEMIES, CAST_BOTH };   /* EnemyOpts.cast: bit 0 the story characters, bit 1 enemies */
enum { CASTK_CHAR, CASTK_ZOMBIE, CASTK_HUNTER };          /* body kinds: what may become what */
#define CAST_CHARACTERS 8   /* Claire, Chris, Steve, Wesker, Alfred, Alexia, Rodrigo, the man in the hazmat suit */

/* the story character (0..7) a record of type id, model version ver is, -1: none (Alfred's and
   Alexia's boss types, props, enemies) */
int cast_character(int id, int ver);
/* CASTK_ZOMBIE / CASTK_HUNTER for a record whose look the randomizer swaps, -1: none */
int cast_enemy_kind(int id);
/* the catalogue entry of an enemy record's look (its own model), -1: not in the catalogue */
int cast_look(int id, int ver);
/* the models of kind (all: CASTK_CHAR with "All models"; imports: the imported models 0..imports-1
   too), as actor indices (catalogue entries, imports ACTOR_MODELS + k), in catalogue order; the count */
int cast_pool(int kind, int all, int imports, int* out, int max);
/* What record key of kind is drawn as: the actor index, -1 none. key: the character (CASTK_CHAR), else
   the catalogue entry of its look (cast_look). scene: 0 for "Fixed per character" (the same in every
   room), else the room file + 1 ("Random per cutscene"). Every key of the kind is assigned at once from
   a shuffle of the pool seeded by (seed, scene, kind), each in turn the next model not taken and not
   its own: different keys get different models while the pool has enough. */
int cast_model(unsigned seed, unsigned scene, int kind, int key, int all, int imports);
/* identity of the imported models list (names, groups, order): co-op uses them only when both sides
   have the same; 0 with none */
unsigned cast_import_id(void);

#ifdef __cplusplus
}
#endif
#endif
