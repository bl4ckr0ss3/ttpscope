# TTPscope

![License](https://img.shields.io/badge/license-MIT-blue)
![Language](https://img.shields.io/badge/C-C17-A8B9D6?logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey)
![PRs](https://img.shields.io/badge/PRs-welcome-brightgreen)
TTPscope is a fast, offline-first terminal research and study client for
MITRE ATT&CK®. It combines a scriptable command-line interface, a responsive
matrix renderer, relationship exploration, personal notes, and an interactive
ncurses browser in one C executable.

<img src="assets/demo.gif" width="877" alt="TTPscope demo: search, Enterprise matrix, group and technique views">

> TTPscope is an independent project. It is not affiliated with, sponsored by,
> or endorsed by The MITRE Corporation.

## Current features

- Synchronizes the official Enterprise ATT&CK STIX bundle
- Searches IDs, names, aliases, and descriptions
- Shows tactics, techniques, groups, software, campaigns, mitigations, data
  sources, data components, and their relationships
- Renders a terminal-width-aware Enterprise matrix with platform filtering
- Stores timestamped research notes as portable Markdown
- Offers an interactive, keyboard-driven ncurses browser
- Works offline after the initial synchronization
- Uses no runtime service, telemetry, or account

## Build

Requirements: a C17 compiler, CMake 3.20+, wide-character ncurses development
headers, and `curl` for the `sync` command.

On Debian or Ubuntu:

```sh
sudo apt install build-essential cmake libncurses-dev curl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Run it directly or install it:

```sh
./build/ttpscope sync
./build/ttpscope search "credential dumping"
./build/ttpscope show T1003
./build/ttpscope matrix --platform Windows
./build/ttpscope tui

sudo cmake --install build
```

## Research workflow

```sh
ttpscope sync
ttpscope show G0016
ttpscope search "scheduled task"
ttpscope note T1053.005 "Compare the procedure examples with current Windows event coverage."
ttpscope notes T1053.005
```

The STIX bundle is cached under `${XDG_CACHE_HOME:-~/.cache}/ttpscope`.
Research notes are ordinary Markdown files under
`${XDG_DATA_HOME:-~/.local/share}/ttpscope/notes`, making them easy to search,
version, and back up.

## Key bindings

| Key | Action |
| --- | --- |
| `/` or `s` | Edit search filter |
| `j` / `k` or arrows | Move selection |
| Page Up / Page Down | Move by ten rows |
| `g` / `G` | First / last result |
| `q` | Quit |

The browser reads ATT&CK descriptions comfortably: links render as
underlined labels, citations are demoted to dim `[source]` markers, and
metadata labels are de-emphasized. Two environment hooks exist for
scripted demos and screenshots: `TTPSCOPE_TUI_SELECT=<query>` starts the
browser pre-filtered, and `TTPSCOPE_TUI_QUIT_AFTER=<seconds>` exits
automatically.

## Data and attribution

TTPscope downloads ATT&CK data from MITRE's official
[`attack-stix-data`](https://github.com/mitre-attack/attack-stix-data)
repository. The application code is licensed under MIT; ATT&CK data remains
subject to MITRE's separate terms. See [NOTICE](NOTICE).

MITRE ATT&CK® and ATT&CK® are registered trademarks of The MITRE Corporation.

## Roadmap

- Citation-focused source browser and extraction
- Bookmarks, tags, reading queues, and spaced-repetition study sessions
- Dataset version pinning and object-level change comparison
- ATT&CK Navigator layer import and export
- ICS and Mobile knowledge bases
- Optional SQLite full-text index for very large custom bundles

Contributions and research-driven feature requests are welcome. Please read
[CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

An example GitHub Actions workflow is provided at `docs/ci.yml.example`. Copy
it to `.github/workflows/ci.yml` when repository workflow permissions are
available.
