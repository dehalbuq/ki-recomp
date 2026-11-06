# Legal notice, independent development and credits

> This document is general project information, **not legal advice**.
> Software/game copyright rules differ by jurisdiction. Obtain a
> qualified legal review and any necessary permission before publicly
> distributing a game-derived executable.

## Please distinguish three kinds of material

1. **Open-source port/runtime source** — the independent source under
   this repository's [MIT license](../LICENSE) is reusable according
   to that license, subject to all embedded third-party notices.
2. **Original commercial game assets** — `kinst.zip`, `kinst.chd`,
   the U98 and sound ROMs, music, graphics, BIOS, extracted code
   images, and captures belong to their relevant rightsholders.
   They are **not** part of this Git repository or the PortMaster
   ZIP; contributors must not upload or redistribute them.
3. **The statically recompiled binary** — the release build executes
   translated code from the original Killer Instinct game. Even
   though the ZIP contains no separate ROM or CHD, it **may embed
   copyright-protected game-derived executable material**. Saying
   "no ROMs included" does **not** establish that redistribution
   of this binary is authorized. Its redistribution may require
   permission or another valid legal basis. Do not assume that a
   legal copy of the game automatically grants distribution rights.

The safest publicly shareable deliverable pending legal review is
**this source code, build documentation, generic tooling, and
independent ROM-free tests**. A person building privately should use
their own lawfully obtained assets under applicable law. Never link
to or solicit unauthorized commercial game downloads.

## Independent nature

This is a non-commercial, independent community port and technical
exercise. It is **not affiliated with, licensed by, sponsored by or
endorsed by** the game rightsholders, including Rare, Midway and
their successors. `Killer Instinct` and related names, characters,
music, graphics and trademarks are the property of their respective
owners. This project does not claim ownership over those elements.

PortMaster integration here does not imply listing in or endorsement
by the official PortMaster catalog.

## Attribution

- **Original arcade game:** Rare (development), Midway (arcade
  distribution), and their respective contributors/rightsholders.
- **Original public recompilation project:** [kilnan/ki-recomp](https://github.com/kilnan/ki-recomp)
  — static MIPS R4600-to-C++ translation, KI board runtime and SDL2 player.
  The source fork retains upstream license notices.
- **R36S ARM64 port:** [dehalbuq](https://github.com/dehalbuq)
  — RK3326/Cortex-A35 optimization and PortMaster launcher,
  OpenGL ES 2/RGBA/NEON path, MIPS dispatch optimization,
  audio smoothing, early blue boot presentation, startup notices,
  packaging and field tests.
- **ADSP-2105 emulator:** derived from the [MAME](https://www.mamedev.org/)
  ADSP-21xx implementation credited to **Aaron Giles**, under
  BSD-3-Clause. See
  [third_party/adsp2100/LICENSE](../third_party/adsp2100/LICENSE).
- **SDL2:** input, audio and rendering integration (zlib license).
- **libchdr:** access to user-supplied CHD images (BSD-3-Clause);
  **miniz:** archive operations (MIT).
- **rabbitizer:** MIPS instruction decoding in the *build tool*
  (MIT). **ki-rom/kipack** is invoked as a separate build-time tool
  subject to its own AGPL-3.0-only terms; no code from it should
  be copied into this fork without meeting its license obligations.
- **MAME:** technical documentation and emulation reference; not an
  endorsed or bundled commercial game distributor.

All original copyright notices must remain intact when adapting
code. A third-party tool used at build time should not be presented
as if its source or copyright automatically became MIT.

## In-game notice and behavior

The R36S port displays two one-time pages **before opening original
game files**: first independent-port/source credits, then legal
status and the requirement for lawful game files. Both pages use
SDL2's renderer and a tiny embedded bitmap font. They have no
runtime work after boot and do not alter the arcade game. A or
Start can advance the notice after a brief minimum display time.

The short on-screen text is not a waiver, does not grant commercial
game rights, and cannot substitute for permission to redistribute
a derivative/recompiled executable.

## Contributor rules

Only contribute original work, appropriately licensed third-party
code with attribution, ROM-free synthetic tests and documentation.
Never include ROM/CHD files, game-derived generated C++,
ROM extraction output, copied proprietary graphics, samples,
BIOS files, game-derived patches, or private CI artifacts in public
issues, PRs or releases.

If any licensing/ownership question arises, open an issue describing
the source and requested use *without attaching proprietary content*.
