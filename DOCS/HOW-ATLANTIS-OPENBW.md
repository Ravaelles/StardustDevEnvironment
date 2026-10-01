# Atlantis (Java) + StardustDevEnvironment (OpenBW) — backend Strategy

Stan: zaimplementowany i zweryfikowany częściowo (szczegóły w §5).
Ostatnia weryfikacja: `javac` całego Atlantis przechodzi; banner OpenBW
i polling JBWAPI potwierdzone w locie; handshake klient↔serwer do dokończenia.

## 1. Problem

Atlantis umiał grać tylko tak: Windows + ChaosLauncher wstrzykujący BWAPI
do żywego StarCrafta (`Main.localAtlantisSetup` robił `taskkill`, `cmd /c`,
patch `bwapi.ini`, hook klawiatury, start ChaosLaunchera). Na Linuksie
z OpenBW model jest odwrotny: zewnętrzny serwer (`BWAPILauncher`) stawia grę,
a bot (Javy `BWClient`) tylko się do niej przyłącza przez pamięć dzieloną.
Stary kod wołałby Windowsowe komendy na Linuksie i wywalał się w
`AKeyboard` (przy braku hooka natywnego robi `System.exit(1)`).

## 2. Rozwiązanie: Strategy `GameLauncher` (Atlantis, Java)

Pakiet `src/atlantis/config/launcher/`:

| Klasa | Rola |
|---|---|
| `GameLauncher` | Interfejs strategii: `void launch(String[] args)` — przygotowuje backend, bota startuje potem `Atlantis.run()` |
| `ChaosGameLauncher` | Stary flow Windows 1:1 (mapa, klawiatura, kill procesów, `bwapi.ini`, ChaosLauncher). Bez zmian zachowania |
| `OpenBWGameLauncher` | Backend linuksowy: wybiera mapę (doradczo), drukuje banner, **nie** rusza procesów/klawiatury/`bwapi.ini` |
| `GameLauncherFactory` | Jedyny punkt decyzji: `Env.isOpenBW()` → OpenBW, inaczej Chaos (domyślnie, wstecznie kompatybilne) |

Wspiera to:

- `Env` (`src/atlantis/config/env/Env.java`): nowy klucz `GAME_LAUNCHER`
  w `bwapi-data/AI/ENV` (`OPENBW` albo cokolwiek innego = Chaos).
  Domyślnie Chaos — istniejące setupy Windows działają nietknięte (OCP).
- `Main.localAtlantisSetup` (`src/main/Main.java`): jedna linijka —
  `GameLauncherFactory.forCurrentEnv().launch(args)`. Cała logika Windows
  mieszka w `ChaosGameLauncher`, cała linuksowa w `OpenBWGameLauncher` (SRP).
- Logika gry (`Atlantis.run()` → `BWClient.startGame()`) nie wie o backendzie
  (DIP) — blokuje na serwerze niezależnie od tego, kto go postawił.

Szablony ENV (czułe na wielkość klucza `GAME_LAUNCHER`, wartość case-insensitive):

- `bwapi-data/AI/ENV LOCAL-EXAMPLE` — dopisany `GAME_LAUNCHER=CHAOS` z komentarzem.
- `bwapi-data/AI/ENV OPENBW-EXAMPLE` (nowy) — gotowy do skopiowania do
  (git-ignorowanego) `bwapi-data/AI/ENV`.
- Żywy `bwapi-data/AI/ENV` jest git-ignorowany — nie ruszamy go w repo.

## 3. Strona serwera (StardustDevEnvironment, C++)

Zero zmian w istniejącym kodzie (OCP) — tylko nowe pliki:

- `scripts/run-openbw-server.sh` — stawia `BWAPILauncher` z katalogu
  `build/test` (MPQ + `maps/` + `bwapi-data/`), mapa/rasa z argumentów lub
  `BWAPI_CONFIG_AUTO_MENU__*`. Sprawdza obecność mapy i MPQ przed startem.
- Ten dokument.

## 4. Przepis na odpalenie (docelowy)

```bash
# Terminal 1 — serwer (StardustDevEnvironment):
./scripts/run-openbw-server.sh "maps/sscai/(4)Python.scx" Protoss

# Terminal 2 — bot (Atlantis, GAME_LAUNCHER=OPENBW w bwapi-data/AI/ENV):
cd /ravaelles/JAVA/starcraft-ai/Atlantis
java -jar Atlantis.jar
```

Oczekiwane na kliencie: banner `[Atlantis] Backend: OpenBW...`, potem
`BWClient.startGame()` wisi aż serwer postawi grę i gra rusza.
Mapa/rasa po stronie serwera są wiążące; wybór mapy w `Main` jest doradczy.

## 5. Status weryfikacji i co zostało

Zweryfikowane wykonaniem:

1. `javac` całego Atlantis (1380 plików, classpath `lib/*`): `EXIT:0`.
   Jedyny wyłączony plik to `src/tests/unit/ATargetingTest.java` — pre-existing
   błąd (import usuniętego `jdk.nashorn.internal`, tryb diff: tylko mode,
   0 linii treści), nietknięty tą zmianą.
2. Tryb OpenBW w locie: banner drukuje się, `BWClient.startGame()` przechodzi
   w polling (`Game table mapping not found` w pętli) zamiast walić
   Windowsowe komendy — strategia działa.
3. Serwer: `BWAPILauncher` z `build/test` żyje (`kill -0` po 4 s) przy mapie
   podanej przez env.

Nie działa jeszcze: handshake klient↔serwer. Ustalenia z kodu (twarde):

- `JBWAPI-Rav.jar` gada protokołem POSIX: `ClientConnectionPosix`
  (AF_UNIX socket + `PosixShm`, pakiet `org.newsclub.net.unix` w jarze).
- Ten fork OpenBW/BWAPI nie ma serwera unix-socketowego: `Server::checkForConnections()`
  w `3rdparty/openbw/bwapi/bwapi/BWAPI/Source/BWAPI/Server.cpp:149` jest **pusty**,
  bloki `if (serverEnabled)` w konstruktorze też. `Main.cpp:31` wymaga
  `externalModuleConnected`, ustawianego w `GameUpdate.cpp:363` tylko gdy
  `server.isConnected()` — na Linuksie ta ścieżka jest martwa.
- Wniosek: brakuje mostu klienckiego po stronie serwera. Opcje:
  **(a)** serwer z brancha `linux-client-support` forka `basil-ladder/bwapi`
  (do tego odnosi się `JavaBWAPI/.../build_with_openbw.md`; jego README
  wprost mówi: *"Using a client bot — Currently, only JBWAPI bots are
  supported"*),
  **(b)** mini-host C++ w tym repo (np. `tests-atlantis`) osadzający most
  kompatybilny z `ClientConnectionPosix`, z API jak `BWTest` (fork + sloty).
  Dopiero wtedy możliwa pełna gra Atlantis vs Steamhammer/Locutus.

Protokół, którego serwer musi dochować (z dekompilacji `JBWAPI-Rav.jar`,
`bwapi/ClientConnectionPosix`): segment `/bwapi_shared_memory_game_list`
(JBWAPI polluje go w pętli — stąd komunikat `Game table mapping not found`),
dalej `/bwapi_shared_memory_<id>` na grę oraz gniazdo AF_UNIX
`/tmp/bwapi_socket_<id>` do synchronizacji klatek i komend. Ten fork nie
tworzy żadnego z nich.

## 6. Gdzie mieszka Atlantis

Sibling: `/ravaelles/JAVA/starcraft-ai/Atlantis` (obok `StardustDevEnvironment/`),
**nie** wewnątrz niego. Osobne repo, osobny build (IntelliJ vs CMake —
CMake-owe `file(GLOB)` nie powinno widzieć Javy), czysty upstream env,
współdzielone MPQ na poziomie `starcraft-ai/`. Jedyny styk w env to ten
dokument + skrypt serwerowy.
