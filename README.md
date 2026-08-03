# Wii Cleaner

A small Wii homebrew utility for removing the built-in Photo Channel, Wii Shop Channel, Forecast Channel, and News Channel titles from a real Wii.

The app is intentionally narrow: it targets only the known title IDs for those channels and does not target IOS titles, the System Menu, the Mii Channel, the Homebrew Channel, save data, or shared content.

## Warning

This program permanently deletes titles from the Wii NAND.

Make a NAND backup with BootMii before running it. You are responsible for any console damage, data loss, or unexpected behavior caused by using this tool.

## Targets

| Title ID | Code | Channel |
| --- | --- | --- |
| `00010002-48414141` | `HAAA` | Photo Channel 1.0 / Photo stub |
| `00010002-48415941` | `HAYA` | Photo Channel 1.1 |
| `00010002-48414241` | `HABA` | Wii Shop Channel |
| `00010002-4841424B` | `HABK` | Wii Shop Channel Korea |
| `00010002-48414641` | `HAFA` | Forecast Channel dummy |
| `00010002-48414645` | `HAFE` | Forecast Channel USA |
| `00010002-4841464A` | `HAFJ` | Forecast Channel Japan |
| `00010002-48414650` | `HAFP` | Forecast Channel PAL |
| `00010002-4841464B` | `HAFK` | Forecast Channel Korea |
| `00010002-48414741` | `HAGA` | News Channel dummy |
| `00010002-48414745` | `HAGE` | News Channel USA |
| `00010002-4841474A` | `HAGJ` | News Channel Japan |
| `00010002-48414750` | `HAGP` | News Channel PAL |
| `00010002-4841474B` | `HAGK` | News Channel Korea |

## How It Works

For each target title, the app:

1. Checks whether the title, title directory, or ticket exists.
2. Tries normal ES deletion first:
   - `ES_DeleteTitleContent`
   - `ES_DeleteTitle`
   - `ES_DeleteTicket`
3. If the title is still present, uses a targeted ISFS fallback for only that title directory and ticket path.
4. Prints a per-title result so you can see what was removed and what, if anything, failed.

## Controls

| Button | Action |
| --- | --- |
| `A` | Scan target title IDs |
| `1`, then `2`, then `PLUS` | Permanently uninstall targets |
| `HOME` | Exit |

The uninstall action requires the full `1 -> 2 -> PLUS` confirmation sequence to avoid accidental deletion.

## Requirements

- A real Wii
- Homebrew Channel
- AHBPROT access through `<ahb_access/>` in `meta.xml`
- SD card or USB device supported by the Homebrew Channel
- devkitPro, devkitPPC, and libogc to build from source

## Install

Copy these files to your SD or USB homebrew apps directory:

```text
boot.dol -> sd:/apps/wii-cleaner/boot.dol
meta.xml -> sd:/apps/wii-cleaner/meta.xml
```

## Build

Install devkitPro with devkitPPC and libogc, then run:

```sh
make
```

The build output is:

```text
wii-cleaner.dol
```

Copy it to your app folder as `boot.dol`.

On Windows, the devkitPro make template may fail if the project path contains spaces. If that happens, build from a path without spaces.

## Project Layout

```text
wii-cleaner/
  Makefile
  README.md
  meta.xml
  boot.dol
  source/
    main.c
```

## Notes

- This is for Wii only, not vWii.
- Channel availability varies by region and system history.
- The app does not delete shared content because shared content may be referenced by other titles.

## License

No license has been selected yet.
