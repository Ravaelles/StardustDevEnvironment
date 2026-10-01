# Atlantis (Java) + StardustDevEnvironment (OpenBW) — backend Strategy

Status: partially implemented and verified (details in §5).
Last verification: full-Atlantis `javac` passes; OpenBW banner and JBWAPI
polling confirmed live; client↔server handshake still to do.

## 1. Problem

Atlantis only knew one way to play: Windows + ChaosLauncher injecting BWAPI
into a live StarCraft (`Main.localAtlantisSetup` ran `taskkill`, `cmd /c`,
`bwapi.ini` patching, keyboard hook, ChaosLauncher start). On Linux with
OpenBW the model is inverted: an external server (`BWAPILauncher`) hosts the
game and the bot (Java `BWClient`) only attaches to it via shared memory.
The old code would invoke Windows commands on Linux and die in `AKeyboard`
(which calls `System.exit(1)` when the native hook is unavailable).

## 2. Solution: `GameLauncher` Strategy (Atlantis, Java)

Package `src/atlantis/config/launcher/`:

| Class | Role |
|---|---|
| `GameLauncher` | Strategy interface: `void launch(String[] args)` — prepares the backend, the bot is started afterwards by `Atlantis.run()` |
| `ChaosGameLauncher` | Old Windows flow 1:1 (map, keyboard, process kills, `bwapi.ini`, ChaosLauncher). No behavior change |
| `OpenBWGameLauncher` | Linux backend: picks a map (advisory), prints a banner, touches **neither** processes/keyboard/`bwapi.ini` |
| `GameLauncherFactory` | Single decision point: `Env.isOpenBW()` → OpenBW, otherwise Chaos (default, backward compatible) |

Supporting changes:

- `Env` (`src/atlantis/config/env/Env.java`): new `GAME_LAUNCHER` key
  in `bwapi-data/AI/ENV` (`OPENBW`, anything else = Chaos).
  Chaos by default — existing Windows setups keep working untouched (OCP).
- `Main.localAtlantisSetup` (`src/main/Main.java`): one line —
  `GameLauncherFactory.forCurrentEnv().launch(args)`. All Windows logic
  lives in `ChaosGameLauncher`, all Linux logic in `OpenBWGameLauncher` (SRP).
- Game logic (`Atlantis.run()` → `BWClient.startGame()`) knows nothing about
  the backend (DIP) — it blocks on the server regardless of who hosts it.

ENV templates (`GAME_LAUNCHER` key is case-sensitive, value is case-insensitive):

- `bwapi-data/AI/ENV LOCAL-EXAMPLE` — appended `GAME_LAUNCHER=CHAOS` with a comment.
- `bwapi-data/AI/ENV OPENBW-EXAMPLE` (new) — ready to copy over the
  (git-ignored) `bwapi-data/AI/ENV`.
- The live `bwapi-data/AI/ENV` is git-ignored — never touch it in the repo.

## 3. Server side (StardustDevEnvironment, C++)

Zero changes to existing code (OCP) — new files only:

- `scripts/run-openbw-server.sh` — starts `BWAPILauncher` from
  `build/test` (MPQs + `maps/` + `bwapi-data/`), map/race from arguments or
  `BWAPI_CONFIG_AUTO_MENU__*`. Checks map and MPQ presence before starting.
- This document.

## 4. Launch recipe (target)

```bash
# Terminal 1 — server (StardustDevEnvironment):
./scripts/run-openbw-server.sh "maps/sscai/(4)Python.scx" Protoss

# Terminal 2 — bot (Atlantis, GAME_LAUNCHER=OPENBW in bwapi-data/AI/ENV):
cd /ravaelles/JAVA/starcraft-ai/Atlantis
java -jar Atlantis.jar
```

Expected on the client: the `[Atlantis] Backend: OpenBW...` banner, then
`BWClient.startGame()` blocks until the server hosts a game, and the game runs.
Map/race on the server side are binding; the map choice in `Main` is advisory.

## 5. Verification status and remaining work

Verified by execution:

1. Full-Atlantis `javac` (1380 files, classpath `lib/*`): `EXIT:0`.
   The only excluded file is `src/tests/unit/ATargetingTest.java` — a
   pre-existing failure (import of the removed `jdk.nashorn.internal`, diff
   mode-only, 0 content lines), untouched by this change.
2. OpenBW mode live: the banner prints, `BWClient.startGame()` enters polling
   (`Game table mapping not found` in a loop) instead of firing Windows
   commands — the strategy works.
3. Server: `BWAPILauncher` from `build/test` stays alive (`kill -0` after 4 s)
   with an env-provided map.

Not working yet: the client↔server handshake. Hard findings from the code:

- `JBWAPI-Rav.jar` speaks the POSIX protocol: `ClientConnectionPosix`
  (AF_UNIX socket + `PosixShm`, package `org.newsclub.net.unix` in the jar).
- This OpenBW/BWAPI fork has no unix-socket server: `Server::checkForConnections()`
  in `3rdparty/openbw/bwapi/bwapi/BWAPI/Source/BWAPI/Server.cpp:149` is **empty**,
  and so are the `if (serverEnabled)` blocks in the constructor. `Main.cpp:31`
  requires `externalModuleConnected`, set in `GameUpdate.cpp:363` only when
  `server.isConnected()` — a dead path on Linux.
- Conclusion: the client bridge is missing on the server side. Options:
  **(a)** a server from the `linux-client-support` branch of the
  `basil-ladder/bwapi` fork (which is what
  `JavaBWAPI/.../build_with_openbw.md` refers to; its README states outright:
  *"Using a client bot — Currently, only JBWAPI bots are supported"*),
  **(b)** a mini C++ host in this repo (e.g. `tests-atlantis`) embedding a
  `ClientConnectionPosix`-compatible bridge, with a `BWTest`-like API
  (fork + slots). Only then can a full Atlantis vs Steamhammer/Locutus game happen.

Protocol the server must honor (decompiled from `JBWAPI-Rav.jar`,
`bwapi/ClientConnectionPosix`): the `/bwapi_shared_memory_game_list` segment
(JBWAPI polls it in a loop — hence `Game table mapping not found`), then
`/bwapi_shared_memory_<id>` per game plus the AF_UNIX socket
`/tmp/bwapi_socket_<id>` for frame sync and commands. This fork creates none
of them.

## 6. Where Atlantis lives

Sibling: `/ravaelles/JAVA/starcraft-ai/Atlantis` (next to `StardustDevEnvironment/`),
**not** inside it. Separate repo, separate build (IntelliJ vs CMake —
CMake `file(GLOB)` must not see Java), clean upstream env, MPQs shared at the
`starcraft-ai/` level. The only coupling inside the env is this document +
the server script.
