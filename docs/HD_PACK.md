# HD texture packs

The port loads HD texture packs made for PCSX2 as they are: no conversion, no data of ours derived
from a pack. How to install one and the settings: [USER_GUIDE.md](USER_GUIDE.md#hd-texture-packs).
This page is about how it works (`pc/src/gs/hdpack.c`).

## How PCSX2 names a texture

From PCSX2's `GSTextureCache::HashCacheKey::Create` / `HashTextureLevel` and
`GSTextureReplacements.cpp` (file names `%llx-%llx-%08x.dds`, or `%llx-%08x` for direct colour):

- **TEX0 hash**: XXH3-64 over the 2^TW x 2^TH texels at TEX0's TBP0/TBW/PSM in GS memory. For
  PSMCT32, PSMT8 and PSMT4 textures of at least one GS block (8x8, 16x16, 32x16) the raw 256-byte
  memory blocks, block row by block row (the swizzled bytes as they lie in GS memory); otherwise
  (smaller textures and the formats that do not fill 32 bits: CT24, CT16, T8H, T4HL/HH) the
  expanded texels row by row, one byte per index for paletted formats, RGBA32 with TEXA applied
  for direct ones.
- **CLUT hash** (paletted only): XXH3-64 over the 16 or 256 palette entries as RGBA32 from the
  CLUT buffer, CSA applied (for CT32 T8 the offset clamps at the last 16-entry block, as
  `GSClut::ReadCLUT_T32_I8`).
- **bits**: PSM | TW << 6 | TH << 10; for CT24/CT16/CT16S also TEXA's TA0 << 15 | AEM << 23 |
  TA1 << 24. Bit 14 (TCC in old packs) is ignored. Region (`-r`) and mip level (`-mipN`) names
  are not used.

The game issues the same TEX0s from the same GS memory and CLUT contents as on the PS2, and the GS
memory model is exact, so the key is computed exactly: each texture is hashed when the renderer
decodes it (native textures: `textures.c ntex_lookup`, after the deferred uploads over the
texture are written into GS memory; everything else: `gs_gl.c tex_get`). A hit binds the
replacement instead of the decoded texture; the draw keeps the original texture's size for its
UVs, so the replacement covers the same 0..1 space at its own resolution.

- Palette animation: decoded textures are cached per palette, so a new palette is a new decode
  and a new key (its own replacement if the pack has one, else the original).
- Render-to-texture (render targets sampled as textures, `StoreRenderTex` captures) and movie
  frames never reach a decode, so they are never replaced (PCSX2 does not hash them either).
- Textures whose 2^TW x 2^TH area reaches past the uploaded image hash whatever else lies in GS
  memory there, as on the PS2; they match only if that memory matches too.
- The game never sets TEX1 MXL > 0, so PCSX2's mip-level hashing does not come up.

## Content check

A key match only says the pack was made from the same texture data, not that the replacement shows
the same picture. Packs made on a translated disc or with a redesigned UI replace menu, inventory
and title textures with other layouts and text, and the port builds its own entries
(MULTIPLAYER, ENHANCED SETTINGS, RETURN TO TITLE) from sub-rectangles of the original menu
textures. So the first time a replacement is loaded it is compared with the original texels
(taken from GS memory and the CLUT at the lookup) on a grid of at most 32 x 32 of them: each
against the replacement's texels over its area (box filter, colour weighted by alpha), decoding
only the BC blocks those cover (bcdec: BC1/2/3/7; RGBA; PNG). A result near a threshold is
repeated over all texels.

- **alpha**: PCSX2 dumps GS alpha (0x80 = 1.0) and uses a replacement's alpha byte as is, but
  pack authors often store 0..255, which doubles the strength of blended UI. The check picks what
  fits the original best: as is, scaled by 128/255, or (when the original's alpha is constant)
  the original's constant. The draw uses it (`u_tex_alpha_scale`; the constant through
  `GL_TEXTURE_SWIZZLE_A`). Different if it still differs by more than 32 on average.
- **colour** (texels visible in the original): different if it differs by more than 24 on average,
  or by more than 48 in over 8% of the texels. Upscaled textures of a large pack stayed within 21
  and 7%; translated or redesigned ones started at 19 and 13%.
- **broken**: a replacement that is (nearly) transparent where the original shows is never used.

A different replacement is logged (`hd textures: <file> does not show the original (...)`) and
used only with `hd_textures_ui` (or, for font pages, `hd_textures_font`). Verdicts are kept in
`<save>/hd_textures.txt` (verdict, alpha mode, file size, path in the pack folder), so each file
is checked once; delete it to check again.

## Tags: font and the port's menu letters

Native texture definitions carry tags set by the game thread when the TIM2 is uploaded
(`texture_glue.c`, sent with the definition in the frame stream):

- **font**: the game's font pages, the first four textures of the effect list
  (`bhSetFontTexture`, drawn by `bhDispFont`/`bhDispFontEx`: messages, item names and
  descriptions), while `sys->ss_flg & 0x40` says they are loaded. Replaced only with
  `hd_textures_font`.
- **keep**: the textures the port cuts its own menu entries from (`pc_tex_keep_original` in
  `lobby_glue.c` and `options_glue.c`). Even with `hd_textures_ui` they get only replacements
  that show the original.

A tag that changes makes a new definition. Textures resolved from GS memory (native textures off)
carry no tags.

## Loading

The folder is indexed by file name once, at the first texture (some 6700 files: a few ms). A file
is read, checked (the first time) and uploaded on the render thread when its texture is first
drawn (DDS: BC1/BC2/BC3/BC7 compressed as they are, uncompressed RGBA/BGRA; PNG through SDL). A
room's textures are first drawn under its fade-in, so the room shows its replacements from its
first visible frame; the cost is a pause of the frames that first draw them (tens of ms per room
from the page cache, about 1-2.5 ms of it per file for the check on the first visit). Loaded
replacements stay in a budget of 1024 MB (`RECVX_HD_CACHE_MB`); the least recently used are
dropped and read again when needed. One log line sums it up 2 s after lookups and loads stop:

```
hd textures: 95 hits (4 rejected, 1 font kept), 0 misses; 80 loaded, 295 MB kept (read 40 ms, checks 136 ms, upload 29 ms, keys 4 ms)
```

Test aids: `RECVX_TRACE_HD=1` logs every lookup (`hit`/`miss` with its PCSX2 name and tags) and
every check; `RECVX_HD_DUMP=dir` writes each looked-up original as PNG under its PCSX2 name.

## Limitations

- Packs usually have no mipmaps (PCSX2 draws them the same way): 2048/4096 replacements on
  distant surfaces shimmer.
- The content check is a threshold: a replacement that changes a few small letters in a big
  texture can pass, a heavily repainted one fail.
- Menus and other 2D: with `hd_textures_ui` (on by default) redesigned menus are used; the
  textures the port cuts its own menu entries from keep the original (`NTEX_KEEP`). A layout
  that moves things the game draws at fixed coordinates can still look off; turn the option
  off to keep the original menus.
- Without a pack nothing changes (frame-identical, `pc/tools/layout_check.sh`).
