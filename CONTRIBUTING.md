# Contributing

This is a community fork of [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy).
Unlike the upstream repo, this fork actively accepts new features, improvements,
and ports — if it makes the buddy better and doesn't break the reference behaviour,
it's welcome here.

## What we'll take

- **Bug fixes** — anything that makes the device behave incorrectly
- **New features** — new UI, new settings, new hardware support
- **Ports** — other boards that speak the same BLE protocol
- **Refactors** — as long as they improve maintainability without changing behaviour
- **Tests** — host-side unit tests for pure logic are especially welcome

If your change also qualifies under the upstream
[CONTRIBUTING.md](https://github.com/anthropics/claude-desktop-buddy/blob/main/CONTRIBUTING.md)
(protocol corrections, boot-blocking bugs), please raise it there too.

## How to contribute

1. Fork this repo and create a branch from `main`
2. Keep each PR focused on one logical change
3. For new settings: add a single entry to `BOOL_SETTINGS[]` in `stats.h` — load, save, and menu rendering are automatic
4. For new hardware: swap the M5Stick drivers, keep the BLE and stats layers unchanged
5. Open a PR against `main` in this repo

## Running tests

The host-side unit tests in `test/test_stats/` cover the pure logic in `stats.h`
and run without hardware via WSL GCC:

```bash
# From the project root in WSL:
g++ -std=c++14 -Itest/stubs -Isrc test/test_stats/test_main.cpp test/stubs/unity.c -o /tmp/test_stats && /tmp/test_stats
```

## Staying in sync with upstream

This fork tracks upstream `main`. When upstream merges a fix, it will be pulled
into this fork's `main` and the enhancement branches rebased on top.
