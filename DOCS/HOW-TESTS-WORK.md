# How `./tests-steamhammer --gtest_filter=RushDefense.*` works

Goal: a deterministic, headless rush-defense simulation on the OpenBW engine —
no Windows, no ChaosLauncher, no clicking. This document describes exactly
the `RushDefense.Steamhammer9PoolSpeed` test.

## 0. TL;DR — what happens after you type the command

```
cd build/test                                   # WORKING DIRECTORY IS MANDATORY
./tests-steamhammer --gtest_filter=RushDefense.*
```

1. `gtest` picks a single test, `TEST(RushDefense, Steamhammer9PoolSpeed)`
   from `test/RushDefense.cpp:4` (the `tests-steamhammer` binary contains only
   `Steamhammer.cpp + RushDefense.cpp + test_common`, see `test/CMakeLists.txt:53`).
2. The test builds a `BWTest` struct (`test/BWTest.h:36`): fixed map (Python),
   fixed seed (`30841`), opponent race (Zerg), opponent module factory
   (`UAlbertaBotModule` with the `9PoolSpeed` strategy), a `5000`-frame limit,
   `expectWin = false`, and two starting-unit lists
   (`myInitialUnits`, `opponentInitialUnits`).
3. `BWTest::run()` (`test/BWTest.cpp:187`) calls `fork()`: after 500 ms the
   child runs `runGame(true)` (opponent, Steamhammer) while the parent runs
   `runGame(false)` (us, `DemoAIModule`). Two processes, each seeing one side
   of the same deterministic OpenBW simulation (same seed).
4. Each process creates its own `BW::GameOwner` (OpenBW engine, data from the
   MPQs in the working directory), attaches its `AIModule`, injects the
   scenario units via `createUnit` (CherryVis patch in OpenBW), then spins the
   `update() → nextFrame()` loop until game over / frame limit / time limit.
5. After the game, our side checks in `onEndMine` (`test/RushDefense.cpp:66`)
   whether any Probe survived (`EXPECT_TRUE(hasAProbe)`), writes the replay +
   CherryVis logs to `replays/`, and the parent reaps the opponent process
   (waits up to 5 s, then `SIGKILL`).

Requirements: working directory `build/test` (relative paths `maps/`,
`bwapi-data/`, MPQs: `StarDat.mpq`, `BrooDat.mpq`, `Patch_rt.mpq`).

## 1. Scenario: `test/RushDefense.cpp:4`

| Field | Value | Meaning |
|---|---|---|
| `test.map` | `Maps::GetOne("Python")` | Fixed map `(4)Python.scx`; no randomization (otherwise random pick from the `sscai` pool) |
| `test.randomSeed` | `30841` | Determinism; `-1` would mean random from `1..100000` (`test/BWTest.cpp:201`) |
| `opponentRace` / `myRace` | Zerg / Protoss (defaults) | Races of slots 1 / 0 |
| `opponentModule` | `new UAlbertaBotModule()` + `Config::StardustTestStrategyName = "9PoolSpeed"` | C++ Steamhammer as the enemy, forced strategy (`test/RushDefense.cpp:10`) |
| `myModule` | `nullptr` → `new DemoAIModule()` | Our bot is the demo from `src/DemoAIModule.*` (`test/BWTest.cpp:327`) |
| `frameLimit` / `timeLimit` | `5000` / `600` (s) | Scenario end instead of a full game (defaults `30000` / `600`) |
| `expectWin` | `false` | The game result itself is not asserted; only `onEndMine` asserts |
| `myInitialUnits` | 2× Pylon, 2× Gateway, ~14× Probe, 1× Zealot | Our base to defend (tiles/positions from `test/RushDefense.cpp:19`) |
| `opponentInitialUnits` | Spawning Pool, ~9× Drone, Overlord, ~11× Zergling | The rush to fend off (`test/RushDefense.cpp:41`) |
| `onEndMine` | `EXPECT_TRUE(hasAProbe)` | The only assertion: did at least one worker survive |

## 2. `BWTest::run()` — fork into two sides (`test/BWTest.cpp:187`)

```
run()
├── map/seed selection (here: already set, no randomization)
├── scheduleInitialUnitCreation() × 2  → myInitialUnitsByFrame / opponentInitialUnitsByFrame
├── fork()
│   ├── child (opponent): sleep 500 ms → runGame(true)  → _exit()
│   └── parent (mine):    runGame(false)
└── parent waits for child (poll every 100 ms, SIGKILL after 5 s)
```

- `scheduleInitialUnitCreation()` (`test/BWTest.cpp:26`) spreads units across
  frames: workers/Overlords → frame 0, Pylons → +1, non-combat buildings → +1,
  combat buildings → +1, rest → last. Returns the setup frame count;
  `run()` takes the `max()` of both sides.
- The `sleep(500ms)` in the child is primitive synchronization: the parent is
  supposed to create the multiplayer game first, the child joins slot 1.
- `SIGFPE/SIGSEGV/SIGABRT` handlers in both processes print a backtrace
  (`execinfo.h`) — the opponent side only logs, our side additionally fails
  the test (`EXPECT_FALSE(true)`).

## 3. `BWTest::runGame(bool opponent)` (`test/BWTest.cpp:262`)

### 3a. Creating the OpenBW game

```cpp
BW::GameOwner gameOwner;                        // OpenBW engine instance
BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
h->setCharacterName(opponent ? "Opponent" : "Tests");
h->setGameType(BWAPI::GameTypes::Melee);
BWAPI::BroodwarImpl.bwgame.setMapFileName(map->filename);
h->createMultiPlayerGame([&]() { ... });        // 2 slots, races, seed, startGame()
```

The lambda in `createMultiPlayerGame` (`test/BWTest.cpp:270`): if the slot is
already assigned — enforce the race; if not — `switchToPlayer(getPlayer(1 or 0))`.
Once the `Player/Computer` slot count reaches 2: `setRandomSeed(randomSeed)` +
`startGame()`. Hence determinism: same map + same seed = same game.

### 3b. Attaching the bots

- Opponent: `opponentModule()` → `UAlbertaBotModule`, `afterOnStart` hook:
  `setLocalSpeed(0)` (max speed, headless) plus optional `onStartOpponent`.
- Us: `myModule == nullptr` → `new DemoAIModule()` (`test/BWTest.cpp:329`),
  `afterOnStart` hook + `Log::SetOutputToConsole(true)` — that is why logs show
  on the console. (Note: the `demoModule->frameSkip` field set here is currently
  dead — `DemoAIModule::onFrame` never reads it; its only use is the declaration
  in `src/DemoAIModule.h:9`.)
- One `h->update()` to fire `onStart` in both modules.

`DemoAIModule` in this test is a simple worker bot: `onStart` initializes `Log`
and `CherryVis` (+ a buildability heatmap), `onFrame` sends idle workers to
minerals and trains more workers from the base, `onUnitCreate/onUnitDestroy` log
(`src/DemoAIModule.cpp:275-289`; `Unit lost` goes to the console in gray and
to the file as plain text — see `src/Instrumentation/Log.cpp`).

### 3c. Injecting scenario units

```cpp
for (frame = 0; frame <= initialUnitFrames; frame++) {
    if (frame > 0) h->update();
    for (unit : initialUnitsByFrame[frame])
        h->createUnit(h->self(), unit.type, unit.getCenterPosition());
    gameOwner.getGame().nextFrame();
}
frameLimit += initialUnitFrames;
```

`createUnit` outside normal production works thanks to the CherryVis patch
in OpenBW (trigger-based unit creation). Creation order matters
(worker → Pylon → buildings → rest), hence the schedule from §2.
`getCenterPosition()` (`test/BWTest.cpp:175`) converts Tile/Walk/Position
into the unit/building center.

### 3d. Main loop

```cpp
while (!gameOwner.getGame().gameOver()) {
    try {
        h->update();                 // dispatch bot onFrame
        onFrameMine / onFrameOpponent (if set)
        if (frameCount == frameLimit) { leaveGame(); }   // here: 5000
        if (elapsed > timeLimit)      { leaveGame(); }
        gameOwner.getGame().nextFrame();
    } catch (std::exception &ex) { log + backtrace + leaveGame(); }
}
```

Headless OpenBW spins thousands of frames per second (typical run of this
scenario: ~2600 frames in ~2 s). A per-frame exception does not kill the
process — it ends the game via `leaveGame()`.

### 3e. Game end and artifacts

- `"Game over after N frames"` → `h->update()` → `onEndMine(won)` or
  `onEndOpponent(won)` → `h->onGameEnd()` in try/catch.
- Our side only (`!opponent`): `Total game time`, conditional
  `EXPECT_TRUE(won)` (skipped here since `expectWin=false`), `gameId` build:
  `RushDefense_Steamhammer9PoolSpeed_<timestamp>_<PASS|FAIL>` (the suffix
  depends on the gtest state — i.e. on `hasAProbe`, not on winning the game).
- With `writeReplay=true`: `replays/<gameId>.rep` (`saveReplay`),
  `replays/<gameId>.rep.cvis` (moved `bwapi-data/write/cvis`),
  `replays/<gameId>.rep.log/` (moved `DemoAI_log_*.txt`).
- The opponent side moves learning files to `bwapi-data/read`
  (`om_Startest.txt` for Steamhammer) — so the next run starts with a
  clean `write/`.

## 4. Filtering and files

- Binary: `build/test/tests-steamhammer` (after our refactor in
  `test/CMakeLists.txt:53` — previously a single `tests` for both bots, which
  caused a `Config::*` symbol collision and a segfault on exit).
- `--gtest_filter=RushDefense.*` selects only this `TEST()`; other patterns:
  `Steamhammer.4PoolHard`, `Locutus.*` (separate `tests-locutus` binary).
- Maps: `test/3rdparty/maps` copied to `build/test/maps` at CMake configure
  time; `bwapi-data/AI/*.json` are bot configs.
- Test exit code `1` on a failed `hasAProbe` assertion (the bot lost the
  scenario) — not a harness bug. `139/SIGSEGV` on exit should no longer happen
  after splitting the binaries.

## 5. Atlantis: ChaosLauncher today vs StardustEnv tomorrow

### Today (Atlantis `master`)

Atlantis is Java on JBWAPI 2.1.0 (`src/main/Main.java`): `main()` reads env,
`localAtlantisSetup()` kills processes, patches `bwapi.ini` (race/map),
starts ChaosLauncher, and `new Atlantis().run()` enters the JBWAPI loop.
Everything runs on Windows: real StarCraft + injected BWAPI 4.4.0 DLL,
GUI, one game at a time, manual control. Requires an SC player profile
and a BWAPI install per the SSCAIT tutorial.

### Tomorrow (migration to StardustEnv)

Architectural difference: StardustEnv uses neither ChaosLauncher nor a live
StarCraft — it uses OpenBW (engine reimplementation) fed by three MPQs and
a C++ `BWTest` harness that creates the game itself, injects units, and spins
frames as fast as the CPU allows. Atlantis (Java) cannot be linked into
a C++ `AIModule`, so a bridge is needed:

1. **Target option:** Atlantis in the JVM + JBWAPI talks to OpenBW as a
   separate process (JBWAPI has an OpenBW-capable variant — see
   `JavaBWAPI/JBWAPI: build_with_openbw.md`), while on the StardustEnv side
   a thin runner appears (a third binary next to `tests-steamhammer` and
   `tests-locutus`, e.g. `tests-atlantis`) which, instead of linking a bot,
   spawns/welcomes the Java process and plays the same game against it that
   `fork()` plays today for C++ bots. This matches the comment in
   `test/CMakeLists.txt:42` (DIP: the harness depends on a module factory,
   not on a concrete bot).
2. **Stepping-stone option (dev):** write a minimal C++ `AIModule` that only
   forwards observations/decisions to Java (JNI/socket), keeping the Atlantis
   logic in Java.

### Where to keep the Atlantis code for local dev

**Recommendation: sibling, i.e. `/ravaelles/JAVA/starcraft-ai/Atlantis`
(next to `StardustDevEnvironment/`), NOT inside it.**

- Separate repo (VCS root), separate build (Gradle/IntelliJ vs CMake) — CMake
  `file(GLOB ...)` and CLion indexing should not see Java sources, and the
  `.gitignore` files of both projects should not mix.
- `StardustDevEnvironment/` stays clean and easy to update from upstream
  (`bmnielsen`), with no conflicts from hundreds of Atlantis files.
- The MPQs in `/ravaelles/JAVA/starcraft-ai/*.MPQ` are already shared at that
  level — Atlantis as a sibling uses them naturally.
- The only touchpoint inside the env: a small adapter, e.g.
  `StardustDevEnvironment/test/atlantis/` (runner + `CMakeLists` + config
  pointing at the sibling via an environment variable or the relative path
  `../../Atlantis`), never a full copy of the framework.

Target layout:

```
/ravaelles/JAVA/starcraft-ai/
├── BROODAT.MPQ / STARDAT.MPQ / patch_rt.mpq   # shared game data
├── StardustDevEnvironment/                     # clean C++ env + thin test/atlantis/ adapter
│   └── build/test/                            # run working dir (maps/, bwapi-data/, replays/)
└── Atlantis/                                  # full Java framework (sibling, separate repo)
```
