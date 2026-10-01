# Jak działa `./tests-steamhammer --gtest_filter=RushDefense.*`

Cel: deterministyczna, headless symulacja obrony przed rushem na silniku OpenBW —
bez Windows, bez ChaosLaunchera, bez klikania. Ten dokument opisuje dokładnie
test `RushDefense.Steamhammer9PoolSpeed`.

## 0. TL;DR — co się dzieje po wpisaniu komendy

```
cd build/test                                   # KATALOG ROBOCZY OBOWIĄZKOWY
./tests-steamhammer --gtest_filter=RushDefense.*
```

1. `gtest` wybiera jeden test `TEST(RushDefense, Steamhammer9PoolSpeed)`
   z `test/RushDefense.cpp:4` (binarka `tests-steamhammer` zawiera tylko
   `Steamhammer.cpp + RushDefense.cpp + test_common`, patrz `test/CMakeLists.txt:53`).
2. Test buduje strukturę `BWTest` (`test/BWTest.h:36`): sztywna mapa (Python),
   sztywny seed (`30841`), rasa przeciwnika (Zerg), fabryka modułu przeciwnika
   (`UAlbertaBotModule` ze strategią `9PoolSpeed`), limit `5000` klatek,
   `expectWin = false` oraz dwie listy jednostek startowych
   (`myInitialUnits`, `opponentInitialUnits`).
3. `BWTest::run()` (`test/BWTest.cpp:187`) robi `fork()`: dziecko po 500 ms
   odpala `runGame(true)` (przeciwnik, Steamhammer), rodzic odpala
   `runGame(false)` (my, `DemoAIModule`). Dwa procesy, każdy widzi jedną stronę
   tej samej deterministycznej symulacji OpenBW (ten sam seed).
4. Każdy proces tworzy własny `BW::GameOwner` (silnik OpenBW, dane z MPQ
   w katalogu roboczym), podpina swój `AIModule`, wstrzykuje jednostki
   scenariuszowe przez `createUnit` (patch CherryVis w OpenBW), a potem kręci
   pętlę `update() → nextFrame()` aż do końca gry / limitu klatek / limitu czasu.
5. Po grze strona "nasza" sprawdza w `onEndMine` (`test/RushDefense.cpp:66`),
   czy przeżyła jakakolwiek Probka (`EXPECT_TRUE(hasAProbe)`), zapisuje
   replay + logi CherryVis do `replays/`, a rodzic sprząta proces przeciwnika
   (czekanie do 5 s, potem `SIGKILL`).

 requirements: katalog roboczy `build/test` (ścieżki względne `maps/`,
 `bwapi-data/`, MPQ: `StarDat.mpq`, `BrooDat.mpq`, `Patch_rt.mpq`).

## 1. Scenariusz: `test/RushDefense.cpp:4`

| Pole | Wartość | Znaczenie |
|---|---|---|
| `test.map` | `Maps::GetOne("Python")` | Sztywna mapa `(4)Python.scx`; brak losowania (w innym wypadku los z puli `sscai`) |
| `test.randomSeed` | `30841` | Determinizm; `-1` oznaczałby los z `1..100000` (`test/BWTest.cpp:201`) |
| `opponentRace` / `myRace` | Zerg / Protoss (domyślne) | Rasy slotów 1 / 0 |
| `opponentModule` | `new UAlbertaBotModule()` + `Config::StardustTestStrategyName = "9PoolSpeed"` | C++ Steamhammer jako wróg, wymuszona strategia (plik `test/RushDefense.cpp:10`) |
| `myModule` | `nullptr` → `new DemoAIModule()` | Nasz bot to demo z `src/DemoAIModule.*` (`test/BWTest.cpp:327`) |
| `frameLimit` / `timeLimit` | `5000` / `600` (s) | Koniec scenariusza zamiast pełnej gry (domyślnie `30000` / `600`) |
| `expectWin` | `false` | Sam wynik gry nie asertuje; asertuje tylko `onEndMine` |
| `myInitialUnits` | 2× Pylon, 2× Gateway, ~14× Probe, 1× Zealot | Nasza baza do obrony (tile/position z `test/RushDefense.cpp:19`) |
| `opponentInitialUnits` | Spawning Pool, ~9× Drone, Overlord, ~11× Zergling | Rush do odparcia (`test/RushDefense.cpp:41`) |
| `onEndMine` | `EXPECT_TRUE(hasAProbe)` | Jedyna asercja: czy ocalał choć jeden worker |

## 2. `BWTest::run()` — fork na dwie strony (`test/BWTest.cpp:187`)

```
run()
├── wybór mapy/seeda (tu: już ustawione, brak losowania)
├── scheduleInitialUnitCreation() × 2  → myInitialUnitsByFrame / opponentInitialUnitsByFrame
├── fork()
│   ├── dziecko (opponent): sleep 500 ms → runGame(true)  → _exit()
│   └── rodzic (my):        runGame(false)
└── rodzic czeka na dziecko (poll co 100 ms, po 5 s SIGKILL)
```

- `scheduleInitialUnitCreation()` (`test/BWTest.cpp:26`) rozkłada jednostki na
  klatki: workerzy/Overlordzi → klatka 0, Pylony → +1, budynki niebojowe → +1,
  budynki bojowe → +1, reszta → ostatnia. Zwraca liczbę klatek setupu;
  `run()` bierze `max()` ze stron.
- `sleep(500ms)` w dziecku to prymitywna synchronizacja: rodzic ma najpierw
  założyć grę multiplayer, dziecko dokleja się do slotu 1.
- Handlery `SIGFPE/SIGSEGV/SIGABRT` w obu procesach drukują backtrace
  (`execinfo.h`) — strona przeciwnika tylko loguje, strona nasza dodatkowo
  oblewa test (`EXPECT_FALSE(true)`).

## 3. `BWTest::runGame(bool opponent)` (`test/BWTest.cpp:262`)

### 3a. Zakładanie gry OpenBW

```cpp
BW::GameOwner gameOwner;                        // instancja silnika OpenBW
BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
h->setCharacterName(opponent ? "Opponent" : "Tests");
h->setGameType(BWAPI::GameTypes::Melee);
BWAPI::BroodwarImpl.bwgame.setMapFileName(map->filename);
h->createMultiPlayerGame([&]() { ... });        // 2 sloty, rasy, seed, startGame()
```

Lambda w `createMultiPlayerGame` (`test/BWTest.cpp:270`): jeśli slot już
przypisany — pilnuje rasy; jeśli nie — `switchToPlayer(getPlayer(1 albo 0))`.
Gdy licznik slotów `Player/Computer >= 2`: `setRandomSeed(randomSeed)` +
`startGame()`. Stąd determinizm: ta sama mapa + ten sam seed = ta sama gra.

### 3b. Podpinanie botów

- Przeciwnik: `opponentModule()` → `UAlbertaBotModule`, hook `afterOnStart`:
  `setLocalSpeed(0)` (maks. prędkość, headless) + ewentualne `onStartOpponent`.
- My: `myModule == nullptr` → `new DemoAIModule()` (`test/BWTest.cpp:329`),
  hook `afterOnStart` + `Log::SetOutputToConsole(true)` — dlatego logi widać
  na konsoli. (Uwaga: ustawiane tu pole `demoModule->frameSkip` jest obecnie
  martwe — `DemoAIModule::onFrame` go nie czyta; jedyne użycie to deklaracja
  w `src/DemoAIModule.h:9`.)
- `h->update()` raz, żeby odpalić `onStart` obu modułów.

`DemoAIModule` w tym teście to prosty worker-bot: `onStart` inicjuje `Log`
i `CherryVis` (+ heatmapa buildowalności), `onFrame` wysyła idle workerów do
minerałów i dokłada workerów z bazy, `onUnitCreate/onUnitDestroy` logują
(`src/DemoAIModule.cpp:275-289`; `Unit lost` idzie na konsolę na szaro,
do pliku czystym tekstem — patrz `src/Instrumentation/Log.cpp`).

### 3c. Wstrzykiwanie jednostek scenariuszowych

```cpp
for (frame = 0; frame <= initialUnitFrames; frame++) {
    if (frame > 0) h->update();
    for (unit : initialUnitsByFrame[frame])
        h->createUnit(h->self(), unit.type, unit.getCenterPosition());
    gameOwner.getGame().nextFrame();
}
frameLimit += initialUnitFrames;
```

`createUnit` poza normalną produkcją działa dzięki patchowi CherryVis
w OpenBW (tworzenie jednostek przez triggery). Kolejność tworzenia ma
znaczenie (worker → Pylon → budynki → reszta), stąd harmonogram z §2.
`getCenterPosition()` (`test/BWTest.cpp:175`) przelicza Tile/Walk/Position
na środek jednostki/budynku.

### 3d. Główna pętla

```cpp
while (!gameOwner.getGame().gameOver()) {
    try {
        h->update();                 // dispatch onFrame bota
        onFrameMine / onFrameOpponent (jeśli ustawione)
        if (frameCount == frameLimit) { leaveGame(); }   // tu: 5000
        if (czas > timeLimit)         { leaveGame(); }
        gameOwner.getGame().nextFrame();
    } catch (std::exception &ex) { log + backtrace + leaveGame(); }
}
```

Headless OpenBW kręci tysiące klatek na sekundę (typowy run tego scenariusza:
~2600 klatek w ~2 s). Wyjątek w klatce nie zabija procesu — kończy grę
przez `leaveGame()`.

### 3e. Koniec gry i artefakty

- `"Game over after N frames"` → `h->update()` → `onEndMine(won)` albo
  `onEndOpponent(won)` → `h->onGameEnd()` w try/catch.
- Tylko strona nasza (`!opponent`): `Total game time`, warunkowy
  `EXPECT_TRUE(won)` (tu pominięty bo `expectWin=false`), budowa `gameId`:
  `RushDefense_Steamhammer9PoolSpeed_<timestamp>_<PASS|FAIL>` (sufiks zależy
  od stanu testu gtest — czyli od `hasAProbe`, nie od zwycięstwa w grze).
- Przy `writeReplay=true`: `replays/<gameId>.rep` (`saveReplay`),
  `replays/<gameId>.rep.cvis` (przeniesiony `bwapi-data/write/cvis`),
  `replays/<gameId>.rep.log/` (przeniesiony `DemoAI_log_*.txt`).
- Strona przeciwnika przenosi pliki uczące do `bwapi-data/read`
  (`om_Startest.txt` dla Steamhammera) — żeby kolejny run startował
  z czystym `write/`.

## 4. Filtrowanie i pliki

- Binarka: `build/test/tests-steamhammer` (po naszym refaktorze
  `test/CMakeLists.txt:53` — wcześniej jeden `tests` na oba boty, co
  powodowało kolizję symboli `Config::*` i segfault przy wyjściu).
- `--gtest_filter=RushDefense.*` wybiera tylko ten `TEST()`; inne wzorce:
  `Steamhammer.4PoolHard`, `Locutus.*` (osobna binarka `tests-locutus`).
- Mapy: `test/3rdparty/maps` kopiowane do `build/test/maps` przy konfiguracji
  CMake; `bwapi-data/AI/*.json` to configi botów.
- Wyjście testu to `1` przy oblanej asercji `hasAProbe` (bot przegrał
  scenariusz) — to nie błąd harnessu. `139/SIGSEGV` przy wyjściu nie powinno
  się już zdarzać po rozdzieleniu binarek.

## 5. Atlantis: ChaosLauncher dziś vs StardustEnv jutro

### Dziś (Atlantis `master`)

Atlantis to Java na JBWAPI 2.1.0 (`src/main/Main.java`): `main()` czyta env,
`localAtlantisSetup()` ubija procesy, modyfikuje `bwapi.ini` (rasa/mapa),
startuje ChaosLaunchera, a `new Atlantis().run()` wchodzi w pętlę JBWAPI.
Całość działa na Windows: prawdziwy StarCraft + wstrzyknięta DLL BWAPI 4.4.0,
GUI, jedna gra naraz, sterowanie ręczne. Wymaga profilu gracza w SC
i instalacji BWAPI wg tutoriala SSCAIT.

### Jutro (migracja do StardustEnv)

Różnica architektoniczna: StardustEnv nie używa ChaosLaunchera ani żywego
StarCrafta — używa OpenBW (reimplementacja silnika) z danymi z trzech MPQ
i C++ harnessem `BWTest`, który sam zakłada grę, wstrzykuje jednostki
i kręci klatki tak szybko jak pozwala CPU. Atlantis (Java) nie wkompiluje się
w C++ `AIModule`, więc potrzebny jest most:

1. **Opcja docelowa:** Atlantis w JVM + JBWAPI gada z OpenBW jako osobny
   proces (JBWAPI ma wariant z obsługą OpenBW — patrz
   `JavaBWAPI/JBWAPI: build_with_openbw.md`), a po stronie StardustEnv
   powstaje cienki runner (trzecia binarka obok `tests-steamhammer`,
   `tests-locutus`, np. `tests-atlantis`), który zamiast linkować bota —
   spawnuje/wita proces Javy i gra przeciwko niemu tą samą grą co dziś
   `fork()` robi dla botów C++. To pasuje do komentarza w
   `test/CMakeLists.txt:42` (DIP: harness zależy od fabryki modułu, nie od
   konkretnego bota).
2. **Opcja pomostowa (dev):** przepisać minimalny `AIModule` w C++, który
   tylko forwarduje obserwacje/decyzje do Javy (JNI/socket), żeby logika
   Atlantis została w Javie.

### Gdzie trzymać kod Atlantis do lokalnego devu

**Rekomendacja: sibling, czyli `/ravaelles/JAVA/starcraft-ai/Atlantis`
(obok `StardustDevEnvironment/`), NIE wewnątrz niego.**

- Osobne repo (VCS root), osobny build (Gradle/IntelliJ vs CMake) — CMake
  z `file(GLOB ...)` i indeksowanie CLion nie powinny widzieć źródeł Javy,
  a `.gitignore` obu projektów nie powinny się mieszać.
- `StardustDevEnvironment/` zostaje czyste i łatwe do aktualizacji
  z upstream (`bmnielsen`), bez konfliktów z setkami plików Atlantis.
- MPQ w `/ravaelles/JAVA/starcraft-ai/*.MPQ` są już współdzielone na tym
  poziomie — Atlantis jako sibling naturalnie z nich korzysta.
- Jedyny punkt styku w środku env: mały adapter, np.
  `StardustDevEnvironment/test/atlantis/` (runner + `CMakeLists` + config
  wskazujący ścieżkę do siblinga przez zmienną środowiskową lub ścieżkę
  względną `../../Atlantis`), nigdy cała kopia frameworka.

Układ docelowy:

```
/ravaelles/JAVA/starcraft-ai/
├── BROODAT.MPQ / STARDAT.MPQ / patch_rt.mpq   # współdzielone dane gry
├── StardustDevEnvironment/                     # czysty C++ env + cienki adapter test/atlantis/
│   └── build/test/                            # katalog roboczy runów (maps/, bwapi-data/, replays/)
└── Atlantis/                                  # pełny framework Java (sibling, osobne repo)
```
