# Changelog -- Gabci's AeroDynamics

Verziozási séma ehhez a projekthez: `MAJOR.MINOR.PATCH`. A `MAJOR` `0` marad, amíg a
plugin el nem éri a kész, minden-fázis-lezárva állapotot (ugyanaz a konvenció, mint a
testvér-projekteknél ebben a családban). A `MINOR` minden lezárt blueprint-fázisnál nő
("nagy" lépés). A `PATCH` egy fázison belüli kisebb javításoknál nő.

## v0.1.0 -- 2026-08-04 -- Fázis 1: Scaffolding és Túlmintavételezés

Vadonatúj projekt indul az `AeroDynamics_Blueprint.md` alapján (5 fázisos terv egy
folyadékmechanikai/Burgers-egyenlet-alapú torzító VST3-hoz). A user kifejezett kérésére
a Fázis 1 és Fázis 2 egy körben készült el, de a konvenciónak megfelelően külön
verziólépésben dokumentálom mindkettőt.

- **`CMakeLists.txt`**: JUCE 9.0.0 rögzítve `FetchContent`-tel (GitHub, `GIT_TAG 9.0.0`,
  `GIT_SHALLOW TRUE` -- ugyanaz a minta, mint a testvér-projekteknél), C++20,
  `project(... LANGUAGES CXX C)` (JUCE 9 CMake-je explicit C fordítót igényel).
  Célok: VST3 + Standalone. A build-mappa apostróf-ellenőrzése (`CMAKE_BINARY_DIR
  MATCHES "'"` -> `FATAL_ERROR`) már a legelső sortól benne van, mert ez a hiba a
  testvér-projekteknél (`Gabci's PhaseLockSub`, `Gabci's Smart Mask Network`,
  `SpectralCarve Pro`) többször is előjött -- JUCE VST3 POST_BUILD shell-lépései nem
  escape-elik az apostrófot. A `PRODUCT_NAME` ezért apostróf nélküli: **"AeroDynamics
  Pro"** a becsomagolt bundle neve, míg **"Gabci's AeroDynamics"** a képernyőn
  megjelenő név (editor wordmark, később az About panel) -- ugyanaz a bundle-név
  vs. képernyő-név szétválasztás, mint a testvéreknél.
- **`Source/OversamplingManager.h/.cpp`** (új fájlok): két, `prepareToPlay()`-ben
  előre felépített `juce::dsp::Oversampling<float>` lánc -- **4x** (`factor=2`) és
  **8x** (`factor=3`), mindkettő `filterHalfBandFIREquiripple`, `isMaxQuality=true`.
  A blueprint kifejezetten hangsúlyozza, hogy elmaradó/gyenge oversampling esetén az
  aliasing tönkreteszi a turbulencia organikus jellegét -- ezért FIR equiripple lett a
  választás (a JUCE két szűrőtípusa közül ez adja a legmeredekebb stopband-elnyomást),
  a magasabb latencia árán, amit a plugin a hoszt felé korrekt módon jelent. A
  minőségváltás (`setQuality()`) valós idő-biztos: csak az aktív, már felépített lánc
  pointerét cseréli, nem épít újat a hangszálon.
- **Mért latencia** (`Tests/OversamplingManagerTests.cpp`, `numChannels=2`,
  `maximumBlockSize=512`): **4x = 59.5 minta, 8x = 64.25 minta** (alap
  mintavételi rátán mérve, nem feltételezve). A két érték érdemben eltér egymástól,
  ami megerősíti, hogy a két lánc valóban külön van felépítve, nem oszt meg állapotot.
- **`Source/PluginProcessor.h/.cpp`**: sztereó Main be/ki busz (mono szándékosan nem
  támogatott -- mind az `OversamplingManager`, mind a `FluidEngine`-tömb 2 csatornára
  van fixen méretezve). A latencia-jelentés a testvér-projekteknél már bevált
  `juce::AsyncUpdater` mintát követi: a hangszálon futó `processBlock()` csak egy
  olcsó index-összehasonlítást végez az Oversampling-minőség paraméteren, és ha
  változást észlel, `notifyLatencyChanged()`-en keresztül trigger-eli az
  AsyncUpdate-et, ami majd az üzenetszálon hívja a tényleges `setLatencySamples()`-t.
- **`Source/PluginEditor.h/.cpp`**: átméretezhető placeholder (400x300 -- 2000x1400,
  alapértelmezett 700x480), mélykék háttér (`0xff0a1420`, a blueprint Fázis 4
  palettájának előlegzése) és a "Gabci's AeroDynamics" wordmark középen. A valódi GUI
  (Kármán-örvény részecskeszimuláció, About panel) Fázis 4-ben készül el.
- **Build ellenőrizve élesben**: konfigurálva és lefordítva (VST3 + Standalone, Debug,
  Ninja) egy apostróf-mentes scratch-mappában, a projektmappán kívül. Mindkét cél
  hibátlanul linkel és csomagol (a VST3 `moduleinfo.json` generálása is sikeres volt).
  A gépen Intel Mac fut (`x86_64`), ugyanaz a környezet, mint a testvér-projekteknél.
- Elemzés: ebben a fázisban még nincs DSP a Fluid Engine-en kívül -- lásd lent.

## v0.2.0 -- 2026-08-04 -- Fázis 2: DSP Mag -- A Fluid Engine

- **`Source/FluidEngine.h/.cpp`** (új fájlok): az 1D Burgers-egyenlet
  (`du/dt + u*du/dx = nu*d2u/dx2`) explicit véges differenciás (FDM) megoldója --
  upwind (Godunov-irányú) differenciálás a nemlineáris konvekciós tagra, centrális
  differenciálás a viszkózus diffúziós tagra, egy fix méretű 1D rácson
  (`kMaxGridSize = 64`, `kMinGridSize = 4`, sosem allokál a hangszálon). A bejövő
  audio minta Dirichlet-peremfeltételként kerül a rács 0. cellájába (befúvó nyílás),
  a kimenet a rács utolsó aktív cellája (kifúvó nyílás, Neumann/zero-gradient perem).
- **Numerikus stabilitás -- CFL-feltétel a paraméterekbe kódolva, nem futásidejű
  ellenőrzésként**: mivel a rácstávolság (`dx`) és az időlépés (`dt`) mindkettő
  rögzítve van 1.0-ra (egy rácscella egy motor-mintánként), a két klasszikus explicit
  FDM stabilitási korlát -- diffúzióra (parabolikus, von Neumann/FTCS):
  `dt*nu/dx^2 <= 0.5`, konvekcióra (hiperbolikus, CFL/Courant): `dt*|u|*c/dx <= 1` --
  egyenesen a `setParameters()`-ben alkalmazott paraméter-skálázásba van beépítve:
  `viscosityCoeff` a `[0, 0.45]` tartományba skálázva (10% biztonsági ráhagyás az
  0.5-ös elméleti határ alatt), `turbulenceCoeff` a `[0, 0.8/kMaxAmplitude]`
  tartományba (20% ráhagyás, mert a két tag ugyanabban az explicit lépésben hat
  egymásra). A `kMaxAmplitude = 4.0` egyszerre a **kemény clamp** (minden cella minden
  lépés után erre van clampelve, és NaN/Inf esetén nullázva) ÉS az analitikus korlát,
  amiből a `turbulenceCoeff` felső határa származik -- ez a két szám szándékosan
  ugyanaz, nem véletlen egybeesés.
- **Tesztelve, nem feltételezve** (`Tests/FluidEngineTests.cpp`, Catch2 v3.7.1, új
  `Tests/` suite `AeroDynamicsProTests_BUILD_TESTS` mögé kapcsolva): **8 tesztecset,
  1 394 327 assertion, mind zöld.**
  - Legszélsőségesebb paraméterek (Viszkozitás=100%, Turbulencia=100%) mellett
    200 000 mintás négyszögjel (±40.0, azaz `kMaxAmplitude` 10x-ese) és 200 000 mintás
    fehérzaj (ugyanolyan amplitúdóval): minden egyes kimeneti minta véges és
    `|kimenet| <= kMaxAmplitude`.
  - Minimum rácsméret (`kMinGridSize=4`) külön tesztelve ugyanezekkel a szélsőséges
    paraméterekkel -- szintén stabil.
  - **Élő rácsméret-váltás közben** (Flow Rate szinuszosan söpörve a teljes
    tartományon, mintánként) 100 000 mintán át, szélsőséges Viszkozitás/Turbulencia
    mellett: stabil marad, még akkor is, ha korábban inaktív cellák (elavult, de már
    korábban is clampelt értékekkel) újra aktívvá válnak.
  - **Csend csendet ad** -- nulla bemenetre pontosan nulla kimenet 1000 mintán át.
- **Mérési eredmény, amit előre nem lehetett volna kitalálni**: a Burgers-egyenletnek
  nincs önálló lineáris advekciós tagja -- a jel egyedüli "szállító" mechanizmusa a
  nemlineáris konvekció (Turbulencia). Ha a Turbulencia pontosan 0, a rács kizárólag
  diffundál, ami NEM szállítja a jelet a befúvótól a kifúvóig (a diffúzió csak
  lokálisan simít, véges idő alatt elhanyagolható energia jut el nagy rácsokon
  keresztül). Mérve: `flowRate01=0.5` (34 cellás rács), `viscosity01=0.05`,
  `turbulence01=0` mellett egy 220 Hz-es szinusz 4000 mintája után a kimeneti
  jelenergia gyakorlatilag nulla (`~6.7e-6`). Ugyanez `turbulence01=1.0`-nál a két
  motor kimenete (azonos Viszkozitás/Flow Rate mellett, hogy a közös
  terjedési-késleltetés és a diffúziós simítás ne zavarja a mérést) mérhetően
  szétválik: `interEngineDiffEnergy = 18.18`, ami >2700-szorosa az 5%-os
  küszöbnek, amit a teszt megkövetel. Ez fizikailag helyes a blueprint saját
  egyenlete szerint, de fontos UX-észrevétel a Fázis 3/4 felé: nagy rácsméret +
  nagyon alacsony Turbulencia esetén a plugin szinte elnémul -- az alapértelmezett
  paraméterek (Viszkozitás 30%, Turbulencia 30%, Flow Rate 50%) ettől messze vannak,
  tehát alapból nem probléma, de a Fázis 4 UI-nak érdemes lehet erre figyelmeztetni
  vagy minimum Turbulencia-értéket beállítani.
- **`Source/PluginParameters.h/.cpp`** (új fájlok): `createParameterLayout()` a 6
  paraméterhez: `pressure` (0-36 dB, alap 0 dB), `viscosity` (0-100%, alap 30%),
  `turbulence` (0-100%, alap 30%), `flowRate` (0-100%, alap 50%), `oversampling`
  (választó: "4x"/"8x", alap 8x), `bypass` (bool, alap ki). Versionezett
  `juce::ParameterID{id, 1}` mindenhol.
- **`Source/PluginProcessor.cpp`**: a `processBlock()` jelfolyama: száraz másolat
  mentése (`dryBuffer`, `prepareToPlay()`-ben előre méretezve, nem allokál a
  hangszálon) -> feltúlmintavételezés -> egy `FluidEngine` csatornánként a
  túlmintavételezett rátán, `Pressure` előerősítéssel hajtva -> visszamintavételezés
  -> `Bypass` crossfade a mentett száraz jellel szemben (20 ms-os `SmoothedValue`
  ramp, kattanásmentes). A Fázis 3 (DC-szűrő, valódi Dry/Wet keverő) még nincs kész --
  a mai `Bypass` egy egyszerű be/ki crossfade, nem a blueprint kevert Dry/Wet gombja.
- **Vizuálisan is ellenőrizve**: a testvér-projekteknél bevált módszerrel -- ideiglenesen
  `juce::GenericAudioProcessorEditor`-ra cserélve a `createEditor()`-t, újrafordítva,
  elindítva a Standalone appot, majd screenshotolva (visszaállítva utána). Mind a 6
  paraméter a helyes címkével, mértékegységgel, tartománnyal és alapértékkel jelent
  meg: Pressure 0.00 dB, Viscosity 30.0%, Turbulence 30.0%, Flow Rate 50.0%,
  Oversampling "8x" kiválasztva, Bypass kipipálatlanul. Utána a valódi placeholder
  editor is elindítva és screenshotolva -- átméretezhető ablak, sötét háttér, wordmark
  középen, összeomlás nélkül.
- Elemzés: a kód tiszta ebben a fázisban. Ismert, szándékosan elhalasztott hiányosság
  a Fázis 3 felé: nincs DC-szűrő (a blueprint szerint az aszimmetrikus hullámtörések
  DC-eltolódást okozhatnak -- ma ez a nyers `FluidEngine`-kimeneten még jelen lehet) és
  nincs valódi Dry/Wet keverő, csak Bypass. A rácsméret-váltáskor fellépő "visszatérő
  elavult energia" jelenség (lásd a stabilitási teszt megjegyzését) hallhatóan
  pattanást okozhat Flow Rate gyors automatizálásakor -- ez finomítható Fázis 3/4-ben,
  simítással a Flow Rate paraméteren, de numerikusan nem instabil, csak esztétikai.

## v0.2.1 -- 2026-08-04 -- Fázis 2 javítócsomag: lineáris advekció + pattanásmentes Flow Rate

A tervet készítő AI (Gemini) átnézte a v0.2.0 changelogot, és két érdemi észrevételt
tett a `FluidEngine`-nel kapcsolatban. Mindkettőt megalapozottnak találtam --
ugyanazokra a jelenségekre mutattak rá, amiket a v0.2.0 changelog saját maga is
"ismert hiányosságként" rögzített -- és mindkettőt kijavítottam.

- **A csend-probléma megszüntetve: rögzített lineáris advekciós tag (`c`)**. A tiszta
  Burgers-egyenletnek nincs olyan tagja, ami erősítés/amplitúdó nélkül is szállítaná a
  jelet -- a diffúzió önmagában csak lokálisan simít, nem "fúj át" semmit a rácson. Az
  egyenlet mostantól `du/dt + (c + alpha*u)*du/dx = nu*d2u/dx2` (`Source/FluidEngine.h`
  / `.cpp`), ahol `c = kBaseFlowSpeed = 0.3` egy rögzített alap-áramlási sebesség,
  `alpha = turbulenceCoeff` pedig a Turbulencia-vezérelt nemlineáris tag. A CFL-
  költségvetést újraosztottam: az együttes advekciós Courant-szám határa változatlanul
  0.8 maradt (20% ráhagyás az 1.0-s elméleti határ alatt), ebből 0.3-at a rögzített
  `c` visz el, a maradék 0.5-öt a Turbulencia-tag kapja (`turbulenceCoeff` felső
  határa `0.5/kMaxAmplitude`, a korábbi `0.8/kMaxAmplitude` helyett). A `c = 0.3`
  választás azért biztonságos: a legnagyobb rácsot (`kMaxGridSize = 64` cella) ezzel a
  sebességgel `64/0.3 ~= 213` motor-mintányi idő alatt keresztezi a jel, ami 8x
  túlmintavételezésnél és 44.1 kHz-es hoszt-rátánál kb. 0.6 ms -- hallhatatlanul rövid
  terjedési késleltetés, tehát `c` értékét a CFL-tartalék, nem a hallható "sebesség
  -jelleg" határozta meg.
  - **Mérve, nem feltételezve** (`Tests/FluidEngineTests.cpp`, új regressziós teszt):
    `viscosity01=0.05`, `turbulence01=0`, `flowRate01=1.0` (legnagyobb rács, a legrosszabb
    eset szállítás szempontjából), 220 Hz-es szinusz, 4000 minta: a javítás előtt a
    kimeneti jelenergia gyakorlatilag nulla volt (`~6.7e-6`, lásd v0.2.0 bejegyzés), a
    javítás után **`inputEnergy=501.066`, `outputEnergy=268.675`** -- a bemeneti
    energia kb. 53.6%-a jut át a kimenetre (a maradékot a viszkozitás nyeli el), a
    korábbi gyakorlatilag nulla helyett.
  - A meglévő stabilitási tesztek (szélsőséges Viszkozitás/Turbulencia,
    négyszögjel/fehérzaj, min/max rácsméret, élő rácsméret-söprés) mind újra lefutottak
    és zöldek maradtak az új egyenlettel is -- a CFL-újraskálázás nem gyengítette a
    numerikus stabilitást.
- **Pattanásmentes Flow Rate váltás, két külön intézkedéssel**:
  1. `FluidEngine::setParameters()` mostantól azonnal nullázza az éppen inaktívvá váló
     rács-cellákat, amikor a Flow Rate csökken (`std::fill` a `[newSize, oldSize)`
     tartományon, mind a `grid`, mind a `nextGrid` tömbön). Mivel ezeket a cellákat
     inaktív állapotban sosem olvassa ki a motor, a nullázásnak a pillanatában nincs
     hallható hatása -- csak azt biztosítja, hogy egy későbbi növekedéskor ne
     "beragadt" régi energia jelenjen meg újra, hanem csendből induljon a cella. Új
     teszt (`Tests/FluidEngineTests.cpp`) bizonyítja: erősen meghajtott rácsot
     lekicsinyítve majd azonnal visszanövelve, a kimenet nullát ad vissza csendes
     bemenetre -- nincs "beragadt" energia.
  2. **`Source/PluginProcessor.h/.cpp`**: a `flowRate` paraméter mostantól
     `juce::SmoothedValue<float>`-en keresztül fut (20 ms-os ramp, a túlmintavételezett
     rátán mérve -- `sampleRate * oversampling.getFactor()`, lásd `OversamplingManager`
     új `getFactor()` metódusa). A `processBlock()` belső ciklusa csatorna-majorról
     minta-majorra lett átszervezve, hogy a Flow Rate pontosan egyszer haladjon előre
     mintánként, csatornák között megosztva (nem egyszer csatornánként) --
     `FluidEngine::setParameters()` mostantól mintánként hívódik (korábban blokkonként
     egyszer), ami olcsó (néhány szorzás), mert maga a rács-feldolgozás úgyis
     mintánkénti O(N) művelet. A Viscosity/Turbulence paraméterek nem lettek simítva
     (nem kértem, és nincs is rá szükség: ezek folytonos együtthatók, nem diszkrét
     állapotváltást okoznak, így nincs pattanás-kockázatuk, szemben a rácsméret
     egész-szám jellegű ugrásával).
- **Build + tesztek újrafuttatva**: VST3 + Standalone + `AeroDynamicsProTests` mind
  hibátlanul fordulnak (Debug, Ninja, ugyanaz az apostróf-mentes scratch-mappa). **10
  tesztecset, 1 398 329 assertion, mind zöld** (a v0.2.0-beli 1 394 327-ről nőtt a két
  új regressziós teszttel). A Standalone app újraindítva és futásban ellenőrizve
  (`pgrep`) -- nem omlik össze a módosított `processBlock()`-kal sem.
- Elemzés: mindkét, az AI-tervező által jelzett probléma valós volt, és mindkettőt a
  saját v0.2.0-s changelog-om is már "ismert hiányosságként" rögzítette -- a javítás
  tehát nem egy addig ismeretlen hibát tárt fel, hanem a már dokumentált, tudatosan
  elhalasztott elemeket oldotta meg, a Fázis 3 megkezdése előtt. A DC-szűrő és a
  valódi Dry/Wet keverő továbbra is hiányzik -- ezek maradnak Fázis 3-nak.

## v0.3.0 -- 2026-08-04 -- Fázis 3: Biztonság és Keverés

A tervet készítő AI (Gemini) ismét review-zta a munkát, és a Fázis 3 elindításakor
három konkrét feladatot adott: DC-szűrő, Dry/Wet Mix paraméter, és -- ez volt az
igazán éles észrevétel -- a száraz jel latencia-kompenzációja a keverés előtt. Mindhárom
technikailag megalapozott volt, mindhárom elkészült.

- **`Source/DcBlocker.h/.cpp`** (új fájlok): klasszikus egypólusú DC-blokkoló
  (`y[n] = x[n] - x[n-1] + R*y[n-1]`), pontosan a blueprint saját specifikációja
  szerint ("DC filter (egypólusú felüláteresztő 10 Hz-en)"). `R` a tényleges
  mintavételi rátából számolva (`1 - 2*pi*10/fs`), nem hardkódolva -- helyes marad
  44.1/48/96 kHz-en is. Matematikailag stabil bármely `0<=R<1`-re (a pólus szigorúan
  az egységkörön belül van). A `PluginProcessor::processBlock()`-ban a **visszamintavételezett**
  (nem a túlmintavételezett) wet jelre kerül, a Turbulencia okozta aszimmetrikus
  hullámtörésből eredő DC-eltolódás ellen, közvetlenül a keverés előtt.
  - **Mérve** (`Tests/DcBlockerTests.cpp`): egy tartós 1.0 DC-jel 10 000 minta után
    `6.44e-7`-re csillapodik (az analitikus becsléssel egyező nagyságrend). Egy 1 kHz
    -es szinusz jelenergiája a beállási szakasz (2000 minta) után **100.132%**-on marad
    -- azaz a hallható tartomány gyakorlatilag érintetlen, csak a DC tűnik el.
- **`Source/PluginParameters.h/.cpp`**: új `mix` paraméter (0-100%, alapérték 100% =
  teljesen wet, a blueprint saját specifikációja szerint).
- **`Source/PluginProcessor.h/.cpp`**: egyenlő teljesítményű (equal-power) keverés
  (`sin`/`cos` görbe a lineáris helyett, hogy 50%-nál ne legyen hallható hangerő-esés).
  A korábbi `bypassMix` át lett nevezve `mixSmoothed`-re, és most a Bypass ÉS a Mix
  paraméter együttes hatását simítja (`Bypass` bekapcsolva a keverést 0%-ra kényszeríti,
  függetlenül a Mix csúszka állásától).
- **Kritikus javítás: száraz jel latencia-kompenzáció** (`juce::dsp::DelayLine<float,
  Lagrange3rd>`, max. 128 minta): az `OversamplingManager` túlmintavételezési szűrői
  59.5 (4x) / 64.25 (8x) mintányi -- **törtszámú** -- latenciát visznek be a wet jelbe
  (lásd v0.1.0 bejegyzés). A `setLatencySamples()` csak a hoszt felé jelzi ezt (több
  sáv közötti PDC-hez), a plugin SAJÁT `processBlock()`-ján belüli Dry/Wet keverésnél
  semmit nem old meg -- enélkül a késleltetett wet és a késleltetetlen dry jel
  összekeverése fésűszűrést/fáziskioltást okozott volna. A mentett száraz másolat
  mostantól pontosan az aktuális túlmintavételezési lánc latenciájára van
  késleltetve, Lagrange-interpolációval a törtszámú rész miatt. A késleltetési idő az
  Oversampling-minőség váltásakor frissül (ugyanott, ahol a `notifyLatencyChanged()`
  is triggerelődik).
- **Tesztelve, nem feltételezve**: **13 tesztecset, 1 506 332 assertion, mind zöld**
  (a v0.2.1-beli 1 398 329-ről nőtt a 3 új `DcBlocker`-teszttel). Build (VST3 +
  Standalone + `AeroDynamicsProTests`) hibátlanul fordul, figyelmeztetés nélkül. A
  Mix paraméter vizuálisan is ellenőrizve a bevált `GenericAudioProcessorEditor`
  -cserés módszerrel: mind a 7 paraméter helyesen jelenik meg (Mix 100.0% alapértéken),
  utána a valódi placeholder editor is elindítva és `pgrep`-pel ellenőrizve --
  összeomlás nélkül fut, mielőtt bezártam.
- Elemzés: mindhárom, az AI-tervező által kért funkció valós, a blueprint saját
  specifikációjában szereplő igényt fedett le (a latencia-kompenzáció pedig egy
  olyan hibát előzött meg, ami enélkül biztosan hallható lett volna Dry/Wet keveréskor).
  Ismert, tudatosan Fázis 4-re hagyott hiányosság: a Mix és a Bypass paraméter jelenleg
  nincs vizuálisan megkülönböztetve a placeholder editorban (a valódi UI még nem
  létezik) -- ez a Fázis 4 UI/UX munka része lesz.

## v0.4.0 -- 2026-08-04 -- Fázis 4: UI és Vizualizáció

A tervet készítő AI (Gemini) egy részletes UI-architektúra brief-et küldött (paletta,
arányos átméretezés, kis felbontású offscreen renderelő, paraméter-reaktív stílus,
réteg-szétválasztás). A tervet egy külön körben visszaigazoltam (ld. beszélgetés),
majd a userrel egyeztetve nekiálltam a tényleges kódnak.

- **`Source/AeroDynamicsLookAndFeel.h/.cpp`** (új fájlok): a brief palettája --
  obszidián háttér (`#0a110d`), sötét mágikus zöld nyugalmi állapotban
  (`idleGlowColour = #2fbd77`), izzó amber/arany aktív állapotban (`activeGlowLow
  = #ff9100` -> `activeGlowHigh = #ffb300`), éles krémszínű tipográfia (`#f5ecd8`).
  A rotary sliderek értékíve a nyugalmi zöldből az aktív amberbe interpolál az
  érték függvényében (`Colour::interpolatedWith`), kétrétegű (széles+halvány, majd
  keskeny+éles) "olcsó glow" trükkel -- nincs valódi blur/OpenGL. A mutató-vonal és a
  szöveges értékdoboz mindig sima, éles színnel rajzolódik, sosem a glow-rétegben --
  ez adja a brief kért olvashatósági garanciáját.
- **Pressure Glow (>60%)**: a `Pressure` slider `setName("pressure")`-jét a
  LookAndFeel felismeri, és 60% fölött egy radiális gradiens amber aurát rajzol a
  gomb köré, alfa/sugár skálázva `(pos-0.6)/0.4`-del -- pontosan a brief kérése.
- **`Source/VisualizationPublisher.h`** (új fájl, JUCE-független, mint a
  `FluidEngine`/`DcBlocker`): lock-free egy-író/egy-olvasó pillanatkép-átadó a
  hangszál (`FluidEngine` rács-állapota) és az UI szál (vizualizáció) között.
  - **Első terv (dupla-puffer + atomi index) HIBÁSNAK bizonyult, mérve, nem csak
    papíron**: egy valódi többszálas Catch2 stressz-teszt (`VisualizationPublisherTests.cpp`,
    200 000 iteráció, szoríthatatlan gyártó szál) SIGABRT-tal (kupac-korrupció)
    omlott össze -- gyors gyártó szál mellett a 2 puffer nem elég, mert az író
    "körbeérhet" és felülírhatja pont azt a puffert, amit az olvasó még másol.
    Javítva **seqlock**-ra váltva (a `std::atomic_thread_fence` referencia
    dokumentáció saját seqlock-mintáját követve pontosan -- egy korábbi,
    `store(..., memory_order_release)`-alapú változat még mindig hibás volt, mert az
    nem tiltja a rákövetkező nem-atomi írás korábbra sorolását; az explicit
    `atomic_thread_fence` a helyes forma).
  - **Második, különálló hiba, szintén a teszt futtatása közben derült ki**: bármely
    `REQUIRE` bukása esetén a Catch2 kivétellel bontja a vermet, de a `producer`
    `std::thread` emiatt sosem lett `join()`-olva -- egy még joinolható
    `std::thread` destruktora `std::terminate()`-et hív, ami elfedte volna a valódi
    hibát egy összeomlással. Javítva egy RAII `JoinOnExit` guard-dal a tesztben.
  - **Mérve, a javítás után**: 200 000 egyidejű publish/read ciklus, minden egyes
    olvasott pillanatkép belsőleg konzisztensnek bizonyult (nincs "szakadt" adat) --
    ez adja a teszt-csomag hatalmas, **27 106 337 assertion**-os számát (a
    64-elemű rácsok minden cellája külön ellenőrizve, ciklusonként).
- **`Source/FluidVisualizerComponent.h/.cpp`** (új fájlok): a brief "Low-Res FBO"
  pontja -- **tudatosan NEM valódi OpenGL FBO**, hanem egy sima szoftveres
  `juce::Image` (200x120, ARGB), amit egy 60Hz-es `juce::Timer` rajzol újra, majd
  `Graphics::drawImage()` + `highResamplingQuality` (lineáris) nagyítja fel a
  komponens tényleges (átméretezhető) méretére -- ugyanaz a CPU/GPU-spórolás, OpenGL
  -driver-kockázat nélkül gyenge integrált GPU-kon. A `VisualizationPublisher`-ből
  olvasott rács-pillanatkép "szalagokként" (streamlines) jelenik meg, cellánkénti
  amplitúdó (normálva `kMaxAmplitude`-dal) vezérli a függőleges kitérést és a
  zöld->amber színátmenetet; egy ritka pont-rács a háttérben `sin`/`cos`-alapú
  UV-warpot kap a Turbulencia paraméterből (**Turbulence Background UV-Warping**,
  a brief kérése szerint).
- **`Source/AboutPanel.h/.cpp`** (új fájlok): végre a valódi About panel, ugyanazzal
  az aláírással, mint minden fájl fejlécében, plusz a mért verzió- és teszt-sorral
  ("Tested on Intel Mac OS 15.7.7, Standalone" -- FL Studio-s élő teszt még nem
  történt ehhez a projekthez, ezért ez a sor egyelőre ezt, nem többet állít).
- **`Source/PluginEditor.h/.cpp`** (teljes átírás): `setResizeLimits(400,300,1600,1200)`
  + `setResizable(true,true)` + `getConstrainer()->setFixedAspectRatio(800.0/600.0)`,
  alapméret 800x600. `resized()` egyetlen `scaleFactor = getWidth()/800.0f`-ot számol,
  és ezt alkalmazza minden betűméretre és layout-méretre -- vektoros rajzolás és JUCE
  Font mérete skálázva éles marad, nincs raszter-elmosódás. Az 5 rotary slider
  (Pressure/Viscosity/Turbulence/FlowRate/Mix), az Oversampling ComboBox és a Bypass
  toggle mind APVTS attachment-tel kötve, ugyanaz a minta, mint Fázis 2-ben. A
  vizualizáció és a gombok/címkék szigorúan külön útvonalon frissülnek (a brief
  "Hybrid Layering" kérése): a gombok sima `juce::Component`-ek, amik alapból is
  csak interakcióra repaint-elnek, a vizualizátor saját 60Hz Timer-rel fut --
  semmilyen globális "repaint mindent" ciklus nincs.
- **`Source/PluginProcessor.h/.cpp`**: új `visualizationPublisher` tag, a
  `processBlock()` a per-mintás FDM ciklus után, blokkonként egyszer publikál egy
  pillanatképet mindkét `FluidEngine` rácsából (olcsó, két 64-elemű tömb-másolás).
- **Build + tesztek**: **15 tesztecset, 27 106 337 assertion, mind zöld** (a
  v0.3.0-beli 1 506 332-ről nőtt elsősorban a `VisualizationPublisher` stressz
  -tesztje miatt). VST3 + Standalone hibátlanul fordul, figyelmeztetés nélkül.
- **Élesben is ellenőrizve**: a Standalone app elindítva és screenshotolva -- a
  wordmark, az About gomb, mind az 5 gomb (helyes címke/érték/szín-gradiens), az
  Oversampling/Bypass sor és a vizualizáció (két zöld szalag, csendben egyenes
  vonalként, ahogy várható "Audio input is muted" mellett) mind helyesen jelenik
  meg. Az About panel megnyitva és screenshotolva: helyes cím, verzió (0.4.0),
  aláírás, teszt-sor, "Click anywhere to close" -- majd bezárva. Az ablak-átméretezés
  sarok-húzásos tesztje a screenshot-sandbox Finder-tiltása miatt nem volt
  elvégezhető ebben a körben (nem a plugin hibája) -- ezt egy jövőbeli, Finder
  -hozzáféréssel rendelkező körben vagy élő DAW-tesztben érdemes megerősíteni.
- Elemzés: a `VisualizationPublisher` két, egymást követő, valódi tesztfuttatással
  feltárt hibája (a dupla-puffer versenyhelyzete, majd a teszt saját szál-kezelési
  hibája) pontosan azt igazolja, amit a user brief-je előírt: "ne következtess, hanem
  tesztelj, mérj" -- papíron mindkét hibás verzió helyesnek tűnt volna. Ismert,
  tudatosan később hagyott hiányosság: a sarok-húzásos átméretezés és az élő FL
  Studio-s vizuális/hallási ellenőrzés még nem történt meg -- ezekre a Fázis 5
  (Tesztelés és Elvárások) vagy egy közbenső élő-DAW kör ad majd lehetőséget.

## v0.5.0 -- 2026-08-04 -- Fázis 5: Tesztelés és Véglegesítés

A tervet készítő AI (Gemini) a v0.4.0 changelog alapján három konkrét feladatot adott:
DAW-integrációs (APVTS host-szerződés) ellenőrzés, a vizualizáció CPU-terhelésének
tényleges mérése (a szoftveres FBO-döntés miatt), és a hátralévő edge case-ek (átméretezés
szélsőértékei, DSP-váltások futás közben) lezárása egy FL Studio-s élő teszt előtt.

- **`Tests/PluginStateTests.cpp`** (új fájl): egy minimális, fejnélküli `AudioProcessor`
  -leszármazottal (`StubProcessor`) ellenőrzi a `createParameterLayout()` +
  `getStateInformation()`/`setStateInformation()` host-szerződést, JUCE 9-ben ehhez
  is a `juce_audio_processors_headless` modul (JUCE 9 új, a GUI-tól elválasztott
  `AudioProcessor` bázisosztálya) kell, amit a `juce_audio_processors` linkelése
  automatikusan magával hoz. **Mérve**: minden paraméter helyes alapértékkel
  jelentkezik, mindegyik `isAutomatable()`, és egy teljes ír/ment/betölt/olvas
  körben minden érték bitpontosan visszakapható -- beleértve az `oversampling`
  választót (index 0/4x vs. 1/8x). Egy garbage (nem-XML) state visszaállítása
  bizonyítottan nem omlik össze és nem módosítja az alapértékeket.
  - **Saját tesztezési hiba, útközben feltárva és javítva**: az alapérték-ellenőrzés
    eredetileg szigorú `==`-t használt `30.0f`-fel szemben -- ez időnként (a
    folyamat ELSŐ `NormalisableRange`-konverzióján) egy ~2e-6 relatív lebegőpontos
    eltérést mért (`30.000001907f`), zeneileg/hangban tökéletesen jelentéktelen, de
    szigorú egyenlőségre bukó. A tényleges ír/olvas kör (reader vs. writer
    összehasonlítás) mindig bitpontosan egyezett, tehát ez a saját tesztem
    hibája volt, nem a state-mentésé -- javítva `Catch::Approx`-ra.
- **`Tests/PluginProcessorTests.cpp`** (új fájl): a VALÓDI `AeroDynamicsProAudioProcessor`
  -t hajtja végig, nem egy egyszerűsített helyettesítőt -- ehhez a `Tests/CMakeLists.txt`
  most már belinkeli a teljes `PluginProcessor.cpp`/`PluginEditor.cpp`/`AboutPanel.cpp`
  /`AeroDynamicsLookAndFeel.cpp`/`FluidVisualizerComponent.cpp` láncot, és kézzel
  definiálja a `JucePlugin_Name`/`JucePlugin_VersionString` makrókat (amiket
  normál esetben a `juce_add_plugin()` generál, egy sima `juce_add_console_app()`
  célhoz nem). **Mérve, nem feltételezve**:
  - Oversampling-minőség váltása hang közben (4x<->8x, 40 blokkon át, ötödik
    blokkonként váltva, folyamatos szinuszjel közepén) -- minden blokk végig véges.
  - Bypass be/ki kapcsolása hang közben, maximális Turbulencia/Pressure mellett
    (hogy a bypassolt és nem-bypassolt kimenet ténylegesen különbözzön) -- stabil.
  - Mintavételi ráta váltása (44.1 -> 48 -> 96 -> 44.1 kHz, mindegyiken újra
    `prepareToPlay()` + 5 blokk) -- minden modul (oversampling, mindkét
    `FluidEngine`, mindkét `DcBlocker`, a dry-késleltető vonal, mindkét
    `SmoothedValue`) helyesen újraindul, nincs "beragadt" méretezés az előző
    rátáról.
  - Állapot-mentés/-visszaállítás a valódi processoron át (nem csak a stub-on).
- **`Tests/PluginEditorTests.cpp`** (új fájl) -- **a v0.4.0-ban elhalasztott
  sarok-húzásos átméretezési edge case, végre lezárva, de nem törékeny
  egér-automatizálással**: mivel az átméretezés VÉGEREDMÉNYE (a `resized()` lefutása
  utáni layout) ugyanaz, akár egér-húzás, akár egy hoszt/session által visszaállított
  ablakméret váltja ki, egy determinisztikus teszt közvetlenül hívja
  `editor->setSize(400,300)` (minimum) és `setSize(1600,1200)` (maximum) mellett, és
  rekurzívan bejárja MINDEN gyerek-komponenst, ellenőrizve, hogy egyiknek sincs
  nulla/negatív szélessége vagy magassága -- mindkét szélsőértéknél, az About
  panellel nyitva is. **Mind zöld, mindkét szélsőértéknél.** A nagy méretű eset
  élesben is megerősítve: a user saját screenshotja egy jóval alapértelmezettnél
  nagyobb ablakban mutatta az összes elemet helyesen elrendezve.
- **`Source/FluidVisualizerComponent.h/.cpp`**: Debug-only teljesítménymérés
  (`JUCE_DEBUG`-mögé zárva) a `renderLowResBuffer()` tényleges lefutási idejére --
  minden egyes futás azonnal logol, ha túllépi az 5 ms-os keretet, plusz egy
  gördülő maximum minden ~1 másodperces ablakra (60 képkocka). **Mérve, a Standalone
  binárist közvetlenül (nem `open`-nel) elindítva, a stderr-t elfogva, képernyő
  nélkül**: a valós gördülő maximum **1.5-2.3 ms** volt, jóval az 5 ms-os
  keret alatt (46-70%-os tartalék). A tervező AI aggálya ("per-pixel matek
  blokkolhatja az UI szálat 60 FPS-en") a saját implementációnkra nézve túlzó volt
  -- nem per-pixel ciklus fut a 24000 pixelen, hanem egy ritka 20x12-es pontrács
  (240 pont) plusz néhány vektoros útvonal --, de az elv (mérni, nem feltételezni)
  helyes volt, és most mérve is van.
- **Harmadik hiba a `VisualizationPublisher`-ben, végleg lezárva mutex-re váltva**:
  a v0.4.0-ban "javítottnak" hitt seqlock a Fázis 5 alatt, a teljes teszt-csomagot
  többször egymás után futtatva, **időszakosan** (kb. 5 futtatásból 1-ben) egy
  off-by-one-generációs "szakadt" olvasást produkált
  (`REQUIRE( v == expectedGeneration )` -- `12908511.0f == 12908512.0f`), annak
  ellenére, hogy a fence-elhelyezés a tankönyvi mintát követte. Mivel ez már a
  MÁSODIK, valódi teszttel feltárt hiba ugyanabban a kézzel írt lock-free
  mechanizmusban, és az adat kizárólag kozmetikus (a vizualizációt táplálja, nem
  hangkritikus), a helyes mérnöki döntés az egyszerűbb, bizonyíthatóan helyes
  megoldásra váltás volt: `std::mutex` + `try_lock()` a hangszálon (sosem blokkol --
  ritka ütközés esetén egyszerűen kimarad az adott blokk pillanatképe, ami 60Hz-en
  észrevehetetlen), teljes `lock_guard` az UI szálon. **Mérve, a javítás után: 20
  egymást követő teljes tesztfuttatásból 0 hiba** (szemben a javítás előtti kb.
  20%-os hibaaránnyal).
- **Vizuális hiba, a userrel közösen élőben megtalálva és javítva**: a Pressure gomb
  60% fölötti amber "aurája" majdnem 100%-os Pressure-nél tömör, kitöltött korongnak
  látszott a tervezett halvány izzás helyett. Ok: a gradiens sugara majdnem a gomb
  sugarának kétszeresére nőtt, a JUCE viszont a komponens saját határainál vágja a
  rajzolást -- így a látható területen csak a gradiens KÖZEPI, még majdnem teljesen
  átlátszatlan része jelent meg. Javítva: a sugár felső határa `2.0x`-ról `1.4x`-re,
  a maximális alfa `0.5`-ről `0.35`-re csökkentve, hogy az elhalványodás ténylegesen
  a gomb határán belül/közelében történjen -- élőben screenshottal megerősítve, a
  másik 4 gombhoz hasonlóan tiszta gyűrűként jelenik meg, finom izzással.
- **Build + tesztek**: **26 tesztecset, 27 106 658 assertion, mind zöld.** VST3 +
  Standalone hibátlanul fordul. Ismert, ártalmatlan teszt-környezeti furcsaság (nem
  a plugin hibája): a fejnélküli konzolalkalmazásban `AudioProcessorValueTreeState`/
  `Timer`/`AsyncUpdater`/GUI-osztályok létrehozásakor JUCE figyelmeztetéseket ír a
  stderr-re ("Assertion failure in juce_Timer.cpp", leak-detector üzenetek), mert
  nincs futó `MessageManager`/eseményhurok -- egy valódi DAW hoszt ezt mindig
  biztosítja, ezért ez sosem jelentkezik éles használatban, csak a teszt-binárisban.
- Elemzés: mind a három, a tervező AI által kért terület (DAW-szerződés, teljesítmény,
  edge case-ek) valós, tesztelhető kérdés volt, és mindegyiket mérve zártuk le. A
  `VisualizationPublisher` története (dupla-puffer -> seqlock -> mutex, két külön
  valódi hibával útközben) jó példa arra, hogy kézzel írt lock-free kód esetén a
  "tankönyvi minta követése" önmagában nem elég -- csak az ismételt, valódi
  többszálas tesztfuttatás bizonyít.

### Élő FL Studio teszt -- ellenőrzőlista a userhez

A pluginon belüli automatizált tesztelés itt lezárult; a következő lépés egy élő
DAW-teszt (ezt a projekthez még nem végeztük el -- lásd az About panel "Tested on ..."
sorát, ami ezt jelenleg is jelzi). Amikor a `AeroDynamics Pro.vst3`-at betöltöd FL
Studio-ba, érdemes végigmenni ezen:

1. **Betöltés és alapállapot**: a plugin hibátlanul betöltődik, az alapértékek
   megegyeznek a itt dokumentáltakkal (Pressure 0dB, Viscosity/Turbulence 30%,
   Flow Rate 50%, Oversampling 8x, Mix 100%, Bypass ki).
2. **Minden gomb hangzásra**: Pressure, Viscosity, Turbulence, Flow Rate, Mix -- mind
   hallhatóan és fokozatosan hat, nincs kattanás/ugrás normál (nem szélsőséges)
   automatizálás közben.
3. **Oversampling váltás hang közben**: 4x <-> 8x váltás lejátszás közben nem okoz
   kattanást/hallható zökkenőt (a latencia-jelentés frissülését az FL Studio PDC-je
   kezeli, ezt nem kell hallanod, csak azt, hogy a Dry/Wet keverék nem csúszik szét).
4. **Mix + Bypass**: Mix 0%-nál pontosan a száraz jelet halljuk (nincs
   fésűszűrés/fázis-kioltás -- ez pont a v0.3.0-s latencia-kompenzáció próbája),
   Bypass be/ki kattanásmentes.
5. **Vizualizáció**: a folyadék-szalagok követik a hangot (zöld nyugalmi, amber
   aktív), a Pressure gomb izzása csak Pressure > 60%-nál jelenik meg, finoman (nem
   tömör korongként). Nincs látható akadozás/fagyás a képen lejátszás közben.
6. **Átméretezés**: a plugin ablakát a sarkánál húzva az arány rögzített marad, és
   minden elem (gombok, vizualizáció, About/Oversampling/Bypass sor) arányosan nő
   /zsugorodik, kicsi és nagy méretnél is olvasható marad.
7. **CPU-terhelés**: FL Studio CPU-kijelzőjén a plugin nem tűnik ki feltűnően a
   többi közül -- a mért 1.5-2.3ms/60Hz vizualizáció-render ehhez képest bőven
   elfér, de az élő szám az igazi próba.
8. **Session mentés/visszatöltés**: FLP mentése, bezárása, újranyitása után minden
   paraméter (és az Oversampling/Bypass választás is) pontosan úgy tér vissza, ahogy
   elmentve volt.

Ha mindez rendben van, ez a sor frissül az About panelben, és jöhet a Release
(v1.0.0) záró kör.

## v0.5.1 -- 2026-08-05 -- Első élő FL Studio kör: 3 valós hiba javítva

A user az ELSŐ élő FL Studio tesztet elvégezte v0.5.0-val, és három konkrét,
mérésekkel alátámasztott problémát talált -- pontosan azt igazolva, amit egy
szoftveres teszt-csomag önmagában sosem tudott volna feltárni.

- **CPU-terhelés drámai növekedése Flow Rate-tel** (mérve élőben: 24% -> 141% a
  plugin performance monitorban, ugyanannál a beállításnál csak Flow Rate-et emelve
  0-ról ~40%-ra, 8x oversamplingnél). **Gyökér ok, saját méréssel megerősítve**: a
  `FluidEngine::processSample()` költsége O(aktív rácsméret), a rácsméretet pedig
  közvetlenül a Flow Rate vezérli -- egy önálló diagnosztikai méréssel (nem
  feltételezve): `gridSize=4` -> 35.0 ns/hívás, `gridSize=64` -> 262.6 ns/hívás,
  azaz kb. 7.5x-es szórás pusztán a Flow Rate csúszka pozíciójától.
- **Viscosity/Turbulence alig hallható alacsony Flow Rate-nél**. Ugyanaz a gyökér
  ok: `gridSize=4`-nél (Flow Rate=0%) a belső FDM-hurok csak 2 cellán fut
  (`for i=1;i<n-1`), mert cella 0 a közvetlenül hajtott bemenet, az utolsó cella
  pedig csak másolja az előzőt -- 2 cellányi nemlineáris feldolgozás egy 4000+
  mintás jelen gyakorlatilag nem halmozódik fel hallhatóan.
  - **Javítás mindkettőre egyszerre**: `Source/FluidEngine.h` --
    `kMinGridSize`/`kMaxGridSize` `4`/`64`-ről `16`/`32`-re szűkítve. Flow Rate=0%-nál
    mostantól 14 belső cella dolgozik (a korábbi 2 helyett), a legrosszabb eset CPU
    -költsége pedig kb. felére csökken. A `kBaseFlowSpeed`-hez tartozó
    dokumentáció-kommentek frissítve az új maximális rácshosszra (32 cella:
    ~0.3ms átfutási idő 8x-nál, továbbra is hallhatatlanul rövid).
  - **A meglévő tesztek közül kettő megbukott a méretváltás után, mindkettő okát
    megtaláltam és valós, indokolt javítással kezeltem, nem csak "megkerülve"**:
    (1) a Turbulencia-divergencia teszt 5%-os küszöbe a RÉGI (4-64) tartományra volt
    hangolva -- újramérve az ÚJ tartományon (16-32) a tényleges arány 1.7%
    (Flow Rate=0%) és 5.6% (Flow Rate=100%) között mozog, a küszöb 2%-ra állítva,
    ami minden mért ponton biztonságosan alatta marad. (2) a "rácsméret-zsugorítás
    nullázza az inaktív farkot" teszt feltételezte, hogy az outlet-ig tartó táv elég
    nagy ahhoz, hogy a maradék energia ne érje el -- az új, szűkebb tartománynál ez
    a táv jóval kisebb, a teszt várakozási idejét a `kMaxGridSize/kBaseFlowSpeed`
    -ből számolt, bőséges (10x) biztonsági ráhagyással megnöveltem.
- **Mix gomb: 50% fölött alig érzékelhető változás**. Ok: az equal-power (`sin`/`cos`)
  keverési görbe 50%-nál már `sin(pi/4) ~= 0.707`-en van, és a szinusz ott már
  ellaposodik a csúcsa felé -- 50-100% között a wet erősítés csak 0.707-ről 1.0-ra
  nő, alig hallhatóan. **Javítva lineáris keverésre** (`wet*mix + dry*(1-mix)`) --
  ez a szokásos Dry/Wet gomb-konvenció (az equal-power inkább sztereó pan-hoz való),
  a cserébe járó enyhe hangerő-esés a keverék közepén elfogadott, normális
  jellemzője egy ilyen gombnak.
- **Build + tesztek**: a rácsméret-változás után 2 teszt megbukott (a fenti okokból,
  nem funkcionális regresszió), mindkettő javítva és újra lefuttatva zölden. **26
  tesztecset, 14 306 658 assertion, mind zöld** (a szám a v0.5.0-beli
  27 106 658-ról csökkent, mert a `VisualizationPublisher` stressz-tesztje
  200 000 iteráción át 64 helyett most 32 elemű rácsokat ellenőriz cellánként --
  nem hiányzó lefedettség, a kisebb rács miatti várt csökkenés).
- **Release build is készült ehhez a körhöz** (`-DCMAKE_BUILD_TYPE=Release`, külön
  scratch-mappában), mert a v0.5.0-s élő teszt Debug (nem optimalizált) build-del
  történt -- a valós CPU-számokhoz ez a mérvadó, nem a Debug.
- Elemzés: mindhárom hiba egy közös témát mutat -- egy DSP-paraméter (Flow Rate)
  egyszerre szabályozott két, egymástól függetlennek tűnő dolgot (rácsméret mint
  "kamra hossz" JELLEG és rácsméret mint SZÁMÍTÁSI KÖLTSÉG/hatásterjedelem), és ez
  a összefonódás csak élő, valós zenei anyaggal derült ki, nem szintetikus
  teszthangokkal. A user saját megfogalmazása szerint pontosan ez volt a Fázis 5
  élő-DAW-kör célja.

## v0.5.2 -- 2026-08-05 -- Második élő kör: valódi numerikus instabilitás a Viscosity-ban

A v0.5.1-es javítás után azonnal jött egy ÚJABB, ezúttal súlyosabb élő visszajelzés:
Turbulencia=0 mellett Viscosity ~82%-nál a hangszer teljesen elnémul; Turbulenciával
feltekerve Viscosity 70-80%-nál "fémes rezonáns túlgerjedés" jelenik meg, afölött
megint néma lesz. Ez NEM a v0.5.1-es rácsméret-javítás mellékhatása volt, hanem egy
valódi, a projekt legelejétől jelen lévő numerikus hiba, amit a v0.5.1-es
audibilitás-javítás csak most tett elég erőssé ahhoz, hogy éles használatban
előjöjjön.

- **Gyökér ok, levezetve és számszerűen igazolva (nem csak sejtve)**: a stabilitást
  eddig két KÜLÖN, egymástól független feltétellel ellenőriztem (diffúzió:
  `nu<=0.5`; advekció: Courant `<=1`) -- mindkettő önmagában helyes volt, de a
  KETTŐ EGYÜTT, ugyanabban az explicit lépésben, destruktívan erősítheti egymást a
  rács Nyquist-frekvenciáján (`k=pi`, sakktábla-mintázat: szomszédos cellák
  felváltva +/- előjellel). Egy teljes von Neumann-analízis (a tényleges upwind
  -advekció + centrális-diffúzió séma amplifikációs tényezője,
  `z(k) = 1 - v - 2*nu + (v+nu)*e^{-ik} + nu*e^{ik}`, a valódi sémára levezetve,
  nem csak a két külön feltételre) megmutatja: `|z(pi)| = |1 - 2|v| - 4*nu|`, ami
  1 fölé nő (a sakktábla-mód NÖVEKSZIK minden lépésben) jóval a korábbi, külön
  -külön biztonságosnak hitt határok alatt is. **Számokkal**: a v0.5.1-es
  együtthatókkal (`kBaseFlowSpeed=0.3`, `viscosityCoeff` max `0.45`) Turbulencia=0,
  Viscosity=82%-nál `max|z(pi)|=1.076` -- pontosan egyezik a user által jelentett
  instabilitás-küszöbbel. Turbulencia=100%, Viscosity=100%-nál `max|z(pi)|=2.4` --
  durván instabil. A növekvő sakktábla-mód a kemény clampnél beragad, majd mivel ez
  pontosan a TÚLMINTAVÉTELEZETT Nyquist-frekvencián oszcillál (minden egyes
  túlmintavételezett mintánál előjelet vált), a leszűrő szűrő szinte teljesen
  kioltja -- ebből lesz "rezonancia, aztán csend".
- **Miért nem fogta meg ezt egyetlen korábbi teszt sem**: az összes eddigi
  stabilitási teszt csak a KORLÁTOSSÁGOT ellenőrizte (`|kimenet|<=kMaxAmplitude`,
  véges érték) -- ez a kemény clamp miatt MINDIG igaz, még akkor is, ha a jel
  ténylegesen egy oda-vissza ugráló, "beragadt" oszcillációba szaladt. A
  korlátosság szükséges, de nem elégséges feltétel a helyes működéshez.
- **Javítás**: `Source/FluidEngine.h/.cpp` -- az együtthatókat egy önálló
  numerikus programmal (`k` teljes körbesöprése, nem csak a `k=pi` zárt alak,
  PLUSZ a teljes Viscosity x Turbulence x előjel paramétertér bejárva) újra
  levezetve, közvetlenül a CSATOLT `|z(pi)|<=1` feltételből, nem két külön
  költségvetés összegéből:
  - `kBaseFlowSpeed`: `0.3` -> `0.15` (több hely marad a viszkozitásnak).
  - Turbulencia sebesség-hozzájárulásának felső korlátja: `0.5` -> `0.3` (azaz
    `turbulenceCoeff` max `0.5/kMaxAmplitude`-ről `0.3/kMaxAmplitude`-re).
  - `viscosityCoeff` felső korlátja: `0.45` -> `0.25`.
  - **Ellenőrizve**: a legrosszabb esetben (Turbulencia=100%, Viscosity=100%,
    `v=0.45`) `max|z(pi)|=0.9` -- 10%-os csillapítási tartalék lépésenként, a
    sakktábla-mód ténylegesen lecseng, nem beragad.
- **Új regressziós teszt** (`Tests/FluidEngineTests.cpp`, "no checkerboard (Nyquist)
  instability"): pontosan a user által jelentett négy beállítást reprodukálja
  (Turb=0/Visc=82%, Turb=0/Visc=100%, Turb=100%/Visc=75%, Turb=100%/Visc=100%),
  2000 mintás beállási idő + 1000 mintás megfigyelt szakasz egy 220Hz-es szinuszra,
  és a kimenet lag-1 autokorrelációját méri (a sakktábla-jel aláírása: érték közel
  -1-hez; egy sima, koherens jelé közel +1-hez). **Mérve, a javítás után**: mind a
  négy esetben `~0.9996` -- a korábban instabil pontokon is teljesen sima,
  koherens kimenet.
- **Build + tesztek**: **27 tesztecset, 14 310 662 assertion, mind zöld** (1 új
  tesztecset, 4 assertion-nel nőtt a v0.5.1-es 14 306 658-hoz képest). Debug ÉS
  Release VST3 is újraépítve, mindkettő telepítve.
- **A user további két megfigyeléséről, amiket egyelőre nem külön hibaként
  kezeltem**:
  1. *"Flow Rate most nem csinál semmit"*: valós, várt következménye a v0.5.1-es
     rácsméret-szűkítésnek (4-64 helyett 16-32) -- a "kamra hossz" jelleg
     különbsége a két szélsőérték között most csak 2x, nem 16x, tehát valóban
     kevésbé kifejezett, mint korábban. Ez tudatos csere volt (CPU/hallhatóság
     javításáért), amit a Fázis 5 élő teszt fog megmutatni, hogy elég-e még így is
     karakteresnek.
  2. *"A zene enyhén torzul hangosabb peakeknél, 4x oversamplingnál feltűnőbb, mint
     8x-nél"*: ez várhatóan RÉSZBEN a most javított instabilitás tünete volt (egy
     beragadt/oszcilláló belső állapot pontosan "kattanásszerű" torzításként
     hallatszna hangos csúcsoknál), RÉSZBEN pedig a túlmintavételezés alapvető,
     szándékos működése -- egy nemlineáris (harmonikusokat generáló) folyamatnál a
     4x lánc eleve kevesebb aliasing-elnyomást ad, mint a 8x, ez maga a blueprint
     Fázis 1-ben leírt jelenség, nem hiba. A most javított instabilitás után
     érdemes újra megfigyelni, hogy a maradék torzítás mennyire zavaró.
- Elemzés: ez egy komolyabb hiba volt, mint a v0.5.1-es három -- nem
  optimalizálási/UX kérdés, hanem valódi numerikus instabilitás, ami a v0.5.1-es
  audibilitás-javítás (erősebb Viscosity/Turbulence hatás alacsony Flow Rate-nél
  is) nélkül a gyakorlatban ritkábban jött volna elő ilyen élesen. A tanulság
  megerősíti a projekt egyik korábbi felismerését (`VisualizationPublisher`,
  Fázis 4-5): a "külön-külön helyes feltételek összegzése" nem helyettesíti a
  teljes, csatolt rendszerre elvégzett analízist -- itt is, ott is csak valódi
  teszteléssel (élő DAW, illetve többszörös tesztfuttatás) derült ki a hiba.

## v0.6.0 -- 2026-08-05 -- Operator Splitting: a DSP mag újratervezése

A v0.5.2 után a user tovább tesztelt, és két újabb, egymással összefüggő problémát
talált FabFilter Pro-Q4 spektrumelemzővel dokumentálva: (1) Flow Rate maximumra
húzva kb. 12kHz-ről 8kHz-re "vágta le" a hangot, mint egy LP-szűrő, még
Viscosity/Turbulence=0 mellett is; (2) a plugint kikapcsolva (Bypass) a hang
SZÉLESEBB volt, mint bekapcsolva minden paraméter 0-n -- vagyis a plugin SOHA nem
volt igazán semleges, még "kikapcsolt" jellegű beállításnál sem. A user kérésére a
tervet készítő AI (Gemini) három megoldási javaslatot küldött (Operator Splitting +
delay line; utólagos EQ-kompenzáció; Lax-Wendroff séma), a user pedig arra kért,
hogy szintetizáljam ezeket és a leggyorsabban a legjobb minőséget adót válasszam.

- **Gyökér ok**: a `kBaseFlowSpeed` (a v0.2.1 óta létező, Turbulence=0-nál is
  szállító állandó "alap-áramlás") elsőrendű UPWIND diszkretizációval volt
  megvalósítva -- ez egy tankönyvi, jól dokumentált tulajdonság: az upwind séma
  numerikus diffúziót visz be minden egyes cellán való áthaladáskor, függetlenül a
  Viscosity/Turbulence beállítástól. Ezért a plugin SOHA nem volt teljesen
  transzparens, és a Flow Rate (nagyobb rács = több cella = több felhalmozódó
  elkenés) egyre erősebb, akaratlan lowpass-szűrőként viselkedett minél magasabbra
  volt húzva.
- **Választott megoldás: Operator Splitting.** A Burgers-egyenlet lineáris
  (transzport, `c`) és nemlineáris (törés/simítás, `alpha*u` + `nu`) fele mostantól
  KÉT KÜLÖN, célszerszámmal megoldott lépés, nem egy közös rács:
  - **Flow Rate = valódi `juce::dsp::DelayLine`** (`PluginProcessor`-ban, a már
    bevált, tesztelt Lagrange-interpolációs minta újrahasznosítva a dry-jel
    latencia-kompenzációjából). Egy delay line-nak DEFINÍCIÓ SZERINT nulla numerikus
    diffúziója van (csak egy interpolált korábbi mintát olvas vissza), így a
    "mindig aktív" elkenődés teljesen megszűnik. A késleltetési idő 0-15ms között
    fut, a túlmintavételezett rátán, simítva (20ms ramp, ugyanaz a minta, mint
    korábban a rácsméret-simításnál).
  - **`FluidEngine` mostantól CSAK a nemlineáris fizikát oldja meg**
    (`du/dt + alpha*u*du/dx = nu*d2u/dx2`, `c` tag nélkül), egy kicsi, FIX méretű
    rácson, ami többé nem függ a Flow Rate-től.
- **Ez strukturálisan a v0.5.1-es CPU-skálázási ÉS hallhatósági problémát is
  megoldja**: a rács mérete állandó, függetlenül a Flow Rate-től, tehát a CPU-
  költség állandó, és a Viscosity/Turbulence hatáserőssége sosem függ többé a Flow
  Rate állásától.
- **Stabilitási határ újra levezetve `c` nélkül**: mivel nincs többé állandó
  sebesség-hozzájárulás, a csatolt von Neumann-analízis (ugyanaz a módszertan, mint
  v0.5.2-ben) sokkal nagyobb tartalékot ad -- Turbulence=0-nál a Viscosity majdnem a
  klasszikus 0.5-ös határig mehet (`nuMax(0)=0.455`, korábban 0.25-0.29 volt).
- **Két ÚJ, a redesign KÖZBEN mért (nem feltételezett) hiba, mindkettő javítva**:
  1. *A diffúzió csillapít, nem szállít.* A régi `kGridSize=16` (a korábbi Flow
     Rate-minimumból megörökölt érték) mellett, `c` nélkül, az alapértelmezett
     (Viscosity/Turbulence 30%) beállításnál a bemeneti energia mindössze **0.44%**-a
     jutott át a kimenetre -- gyakorlatilag ugyanolyan "néma" hiba, mint a v0.5.2-es,
     csak más beállításnál. Egy rácsméret-söprés (mérve, 2-16 cella között) megmutatta,
     hogy az átvitel a cellaszámmal drasztikusan csökken (N=2: 99%, N=4: 69%, N=8: 4%,
     N=16: 0.47%) -- `kGridSize` **16-ról 4-re csökkentve**, ami ~74%-os átvitelt ad
     ugyanazoknál a beállításoknál.
  2. *A Turbulencia önmagában nem tudja "felébreszteni" a néma cellákat.* A nemlineáris
     advekciós tag önreferenciális (`sebesség = turbulenceCoeff * u`), ezért egy
     pontosan 0-n álló cella sebessége is pontosan 0, függetlenül a szomszédoktól --
     Viscosity=0%, Turbulence=100%-nál mérve pontosan **0%** jutott át. Csak a diffúzió
     (ami a SZOMSZÉDOK értékétől függ, nem a cella sajátjától) tudja egy cellát
     "elindítani" a semmiből. Javítva egy kis, mindig aktív `kViscosityFloor=0.02`
     hozzáadásával a viszkozitás-együtthatóhoz, függetlenül a Viscosity gomb
     állásától -- a stabilitási tartalékot szándékosan 2%-kal csökkentve
     (`nuMax(0)=0.455` a lehetséges `0.475` helyett), hogy maradjon hely a padlónak;
     a teljes paramétertéren mért legrosszabb eseti tartalék így is **8%** maradt.
- **`Tests/FluidEngineTests.cpp` jelentősen átírva** az új (kétparaméteres,
  `flowRate01` nélküli) API-hoz és tervezéshez: a rácsméret-változtatáshoz kötött
  tesztek (min/max rács, zsugorítás-nullázás) törölve, mivel a rács mostantól fix
  méretű; két ÚJ regressziós teszt a fent mért két hibára (Turbulencia önmagában
  szállít-e, alapértelmezett beállítás átvitele); a Nyquist-instabilitási teszt
  (v0.5.2-ből) újramérve az új, `c`-mentes együtthatókkal -- mind zöld.
- **Build + tesztek**: **26 tesztecset, 2 914 661 assertion, mind zöld** (a szám
  csökkent a v0.5.2-beli 14 310 662-ről, mert a `VisualizationPublisher` rácsai
  4 elemesek lettek 32 helyett -- kevesebb cellát ellenőriz ciklusonként, nem
  hiányzó lefedettség). Debug + Release VST3 újraépítve, mindkettő telepítve.
- Elemzés: ez volt a legnagyobb egyszeri architektúra-változás a projektben eddig --
  a user kifejezett kérésére ("ne kérdezz, csinád végig") önállóan, kérdés nélkül
  végigvíve, de a projekt teljes eddigi fegyelmével (mérve minden lépés, nem
  feltételezve). A redesign közben talált két új hiba (diffúzió-mint-csillapítás,
  önreferenciális turbulencia-tag) pontosan azt igazolja, amit a projekt már
  többször tapasztalt: egy nagy architektúra-változtatás sosem "csak" a célzott
  problémát oldja meg elsőre -- alapos, lépésenkénti méréssel kell felderíteni az
  új tervezés saját, előre nem látott mellékhatásait is, mielőtt készre
  nyilvánítanánk. **Élő FL Studio megerősítésre vár** -- a user jelenleg nincs
  gépnél, a végleges Release (v1.0.0) az ő visszaigazolása nélkül nem történik meg.

## v0.6.1 -- 2026-08-05 -- Turbulence-tartomány szélesítése + Flow Rate karakteresebbé tétele

A tervet készítő AI egy konkrét, kiszámolt javítási javaslatot küldött (`észrevétel.md`)
a v0.6.0 utáni "csak a Pressure csinál valamit" panaszra. **A javaslat két számítási
hibát tartalmazott** -- ellenőriztem a projektben már többször bevált, igazolt
`|z(pi)| = |1 - 2v - 4*nu|` képlettel (nem a javaslat `v + 2*nu`-jával, ami hiányzó
2-es/4-es szorzót jelentett), és a default-beállításra állított `viscosityCoeff`
-értékük valójában a RÉGI, nem az új számuk volt. A cél (erősebb Turbulence, hallhatóbb
Flow Rate) viszont jogos, ezért a saját, helyes képletemmel újraszámolt, korrigált
értékekkel vittem be ugyanazt a szándékot:

- **`Source/FluidEngine.h/.cpp`**: a Turbulence sebesség-költségvetése `0.5`-ről
  `0.65`-re nőtt (`turbulenceCoeff = turbulence01 * 0.65/kMaxAmplitude`), a
  `nuMax(t) = 0.455 - 0.325*t` (volt: `0.455 - 0.25*t`) képlet ugyanazzal a ~10%-os
  csillapítási tartalékkal lett újra levezetve a szélesebb Turbulence-sávra --
  legrosszabb esetben (`v=0.65`, `nu=0.15` a paddal együtt) `|z(pi)|=0.9`, ugyanaz a
  biztonsági fegyelem, mint v0.6.0-ban.
- **`Source/PluginProcessor.h`**: a Flow Rate maximális késleltetése `15ms`-ről
  `25ms`-re nőtt -- 50%-on ez kb. 12.5ms, egyértelműen hallható "kamra-hossz" anélkül,
  hogy nyílt visszhanggá (slapback) válna.
- **`Source/PluginParameters.cpp`**: a Flow Rate elavult, még a régi rácsméret
  -alapú leírást tartalmazó kommentje frissítve a tényleges (delay line-os) működésre.
- **Tesztek**: a meglévő 26 tesztecset (2 914 661 assertion) mind zöld maradt az új
  együtthatókkal is, egyik küszöböt sem kellett módosítani -- a korábbi tartalékok
  elég nagyok voltak.
- Elemzés: a tervező AI konkrét, ellenőrizhető javaslata hasznos kiindulópont volt,
  de a végleges, bevitt számokat a saját, korábban már validált módszertannal
  számoltam újra, nem a kapott értékeket másoltam be -- ez pontosan azt a fegyelmet
  követi, amit a projekt már többször (v0.5.2, v0.6.0) bizonyítottan megkívánt: minden
  stabilitási számot ellenőrizni kell, nem elfogadni. A user szűk heti kerete miatt ez
  egyetlen, célzott, minimális lépésben történt (egy build-kör, nincs újabb
  architektúra-változás). **Élő FL Studio megerősítésre továbbra is vár.**
