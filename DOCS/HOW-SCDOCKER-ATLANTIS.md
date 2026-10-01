# Atlantis via sc-docker — cheat sheet

Stan: działa. Pierwszy pełny mecz: `GAME_391B86A3`, Benzene, `AtlantisP` vs
`Steamhammer`, 122 s, grała (unit score 5950), wygrał Steamhammer.
Replaye: `/home/rav/.scbw/games/GAME_*/player_*.rep`.

## Boty

`starcraft-ai/bots/`: `AtlantisP` (Protoss) i `AtlantisT` (Terran) — ten sam
fat jar (`Atlantis.jar`, Java 8), rasa z `bot.json` + autodetekcja ze slotu
(`AtlantisConfigChanger.modifyRacesInConfigFileIfNeeded`). Oba przeszły
walidację `BotPlayer` (JAVA / 4.4.0).

## Uruchamianie

```bash
scbw.play --bot_dir /ravaelles/JAVA/starcraft-ai/bots --bots "AtlantisP" "CherryPi" --headless
scbw.play --bot_dir /ravaelles/JAVA/starcraft-ai/bots --bots "AtlantisT:T" "Steamhammer:Z" --map "sscai/(4)Python.scx" --seed_override 42 --timeout_at_frame 20000
```

Wyniki: `/home/rav/.scbw/games/GAME_*/` (`result.json`, `scores.json`, `*.rep`).

## Pułapki, które już załatano (nie ruszać)

1. `pipx` venv `scbw`: dopisane `4.4.0` do `bwapi.py`
   (`versions_md5s` + `supported_versions`) — pipx 1.0.4 nie znał 4.4.0.
   Po reinstalacji `scbw` patch trzeba powtórzyć.
2. Obrazy `starcraft:game-1.0.4` i `starcraft:java`: dołożony
   `/app/tm/4.4.0.dll` (z `sc-docker/docker/tm/`) — obraz `java` go zgubił,
   `prepare_tm` bez niego ubija kontener przed startem.
3. `~/.docker/config.json`: usunięte `credsStore: desktop` (waliło każde pull).
4. Gra do obrazu `game`: `/home/rav/.scbw/docker/starcraft.zip` przepakowany
   z lokalnego `starcraft-ai/starcraft` (1.16.1) zamiast martwego mirroru
   `files.theabyss.ru`.
5. Steamhammer z SSCAIT działa (stare `tested_bots.md` straszyło Wine page
   fault — nieaktualne dla obecnego builda).
