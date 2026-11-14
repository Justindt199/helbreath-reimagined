# Helbreath assets

Shared client data for classic Helbreath: sprites, maps, sounds, music,
fonts, and contents files.

This pack is the Helbreath USA 3.72-era client data. That label is a
comparison point, not a claim that the files come from an official dump.

These files are the common base. Game-specific trees layer their own files
on top:

- [helbreath-v3.82](https://github.com/play-helbreath/helbreath-v3.82) —
  recovered and restored 3.82 client and server
- [helbreath-equilibrium](https://github.com/play-helbreath/helbreath-equilibrium) —
  Equilibrium Project client overlay and server

To build a playable client folder, copy this repository, then copy that
game’s `client/` files over it so Equilibrium (or 3.82) files replace the
base where names match.
