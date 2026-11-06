# Contributing to ki-recomp and the R36S port

Thank you for helping with the independent open-source Killer Instinct
recompilation project!

## R36S ARM64 / RK3326 contributions

**Please target the active branch [`port/r36s-portmaster`](https://github.com/dehalbuq/ki-recomp/tree/port/r36s-portmaster), not `main`, for R36S hardware/PortMaster changes.**

- [Detailed R36S contributor guide](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/CONTRIBUTING.md)
- [Progress, benchmarks, known limitations](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_PROGRESS.md)
- [Licensing, original game assets and static-recompilation distribution caveats](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_LEGAL.md)
- [Guia em português](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_README_PTBR.md)

A useful PR explains the problem, change, platform/firmware,
reproduction steps, ROM-free test evidence and any measured performance
difference. Use a separate diagnostic build for FPS profiling rather
than attributing its overhead to normal Release gameplay.

Do **not** upload copyrighted game ROMs (`kinst.zip`, boot BIOS, sound
ROMs), CHD, extracted program code, generated game-derived C++,
audio/graphics, MAME game captures, or privately built executables to
public pull requests or issues. Source-only development and synthetic
ROM-free tests are preferred until redistribution permissions have
been reviewed.

For changes to the base portable compiler rather than the R36S fork,
consult [upstream kilnan/ki-recomp](https://github.com/kilnan/ki-recomp)
and preserve all third-party credits, rights and notices.

[Create an issue](https://github.com/dehalbuq/ki-recomp/issues) if you
would like to discuss a proposed improvement before opening a PR.
