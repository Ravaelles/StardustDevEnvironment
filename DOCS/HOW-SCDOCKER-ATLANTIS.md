# Atlantis via sc-docker — cheat sheet

Status: works. First full match: `GAME_391B86A3`, Benzene, `AtlantisP` vs
`Steamhammer`, 122 s, Atlantis played (unit score 5950), Steamhammer won.
Replays: `/home/rav/.scbw/games/GAME_*/player_*.rep`.

## Bots

`starcraft-ai/bots/`: `AtlantisP` (Protoss) and `AtlantisT` (Terran) — the same
fat jar (`Atlantis.jar`, Java 8), race from `bot.json` + slot autodetection
(`AtlantisConfigChanger.modifyRacesInConfigFileIfNeeded`). Both passed
`BotPlayer` validation (JAVA / 4.4.0).

## Running

```bash
scbw.play --bot_dir /ravaelles/JAVA/starcraft-ai/bots --bots "AtlantisP" "CherryPi" --headless
scbw.play --bot_dir /ravaelles/JAVA/starcraft-ai/bots --bots "AtlantisT:T" "Steamhammer:Z" --map "sscai/(4)Python.scx" --seed_override 42 --timeout_at_frame 20000
```

Results: `/home/rav/.scbw/games/GAME_*/` (`result.json`, `scores.json`, `*.rep`).

## Pitfalls already fixed (do not touch)

1. `pipx` venv `scbw`: added `4.4.0` to `bwapi.py`
   (`versions_md5s` + `supported_versions`) — pipx 1.0.4 did not know 4.4.0.
   Repeat the patch after reinstalling `scbw`.
2. Images `starcraft:game-1.0.4` and `starcraft:java`: added
   `/app/tm/4.4.0.dll` (from `sc-docker/docker/tm/`) — the `java` image lost it,
   and `prepare_tm` kills the container before start without it.
3. `~/.docker/config.json`: removed `credsStore: desktop` (broke every pull).
4. Game for the `game` image: `/home/rav/.scbw/docker/starcraft.zip` repacked
   from the local `starcraft-ai/starcraft` (1.16.1) instead of the dead
   `files.theabyss.ru` mirror.
5. SSCAIT Steamhammer works (the old `tested_bots.md` warned about a Wine page
   fault — outdated for the current build).
