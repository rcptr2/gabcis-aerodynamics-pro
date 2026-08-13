# Gabci's AERO-DYNAMICS PRO -- FEJLESZTÉSI TERV ÉS BLUEPRINT (V1.0)

## 1. Projekt Áttekintés és Koncepció (Koncepció A)
Az **Gabci's AeroDynamics** (plugin filename AeroDynamicsPro) egy áramlástanon és folyadékmechanikán (Fluid Dynamics) alapuló audio effekt, amely a bejövő hanghullámokat nem pusztán elektronikus jelként, hanem egy "szélcsatornában áramló folyadékként/gázként" kezeli.
A szoftver a Navier-Stokes és a Burgers-egyenletek egyszerűsített, egydimenziós (1D) és valós idejű megoldásait használja a dinamika és a szaturáció vezérlésére. Célközönsége a Neurofunk, Modern DnB és indusztriális Techno producerek, akik teljesen organikus, mozgó és "szétpattanó" torzítást keresnek.

---

## 2. Tudományos Háttér és Működési Elv
A DSP mag egy nem-lineáris differenciálegyenlet-megoldó (pl. Runge-Kutta módszerrel), amely az 1D áramlást szimulálja. A viszkozitás tompítja a tranzienseket (mint egy kompresszor/low-pass), míg a nem-lineáris konvekciós tag turbulenciát (harmonikus torzítást) okoz.

**Matematikai alap (1D Burgers-egyenlet):**
$$ rac{\partial u}{\partial t} + u rac{\partial u}{\partial x} = 
u rac{\partial^2 u}{\partial x^2} $$
Ahol:
*   $u$ = a hanghullám pillanatnyi amplitúdója (áramlási sebesség).
*   $
u$ = viszkozitás (Kinematic viscosity).
*   $u rac{\partial u}{\partial x}$ = a turbulenciát és a hullámtörést okozó nem-lineáris tag.

---

## 3. Főbb Paraméterek (APVTS)
1.  **Pressure (Input Drive):** A "szélcsatorna" bemeneti nyomása (0 - 36 dB).
2.  **Viscosity (Viszkozitás):** 0% - 100%. Magas értéknél a hang sűrű, "szirupos" (brutális kompresszió és magasfrekvenciás csillapítás).
3.  **Turbulence (Reynolds-szám szimuláció):** 0% - 100%. Mennyire törjön meg a hullám. Magas értéknél káotikus, áramlási zajjal dúsított szaturációt ad.
4.  **Flow Rate (Oszcilláció sebessége):** A konvekciós kamra mérete (késleltetés / hullámtér hossza).

---

## 4. Fejlesztési Fázisok

### Fázis 1: Scaffolding és Túlmintavételezés (Oversampling)
*   **Feladat:** JUCE projekt felállítása. Mivel a nem-lineáris egyenletek extrém harmonikusokat generálnak (aliasing), **kötelező** a minimálisan $4	imes$, de inkább $8	imes$ oversampling implementálása (`juce::dsp::Oversampling`).
*   **Kockázat:** Ha az oversampling lemarad, az aliasing teljesen tönkreteszi a turbulencia organikus jellegét.

### Fázis 2: DSP Mag - A Fluid Engine (Differenciálegyenlet-megoldó)
*   **Feladat:** Egy véges differenciák módszerével (FDM - Finite Difference Method) működő audio-rate hullámszimulátor írása. A bejövő PCM mintákat peremfeltételként tápláljuk az 1D gridbe.
*   **Kockázat (Kritikus!):** Numerikus robbanás (NaN / Infinity). A nem-lineáris differenciálegyenletek könnyen instabillá válnak. Szigorú clampinget és stabilitási feltételeket (pl. Courant–Friedrichs–Lewy feltétel - CFL) kell beépíteni a kódba.

### Fázis 3: Biztonság és Keverés
*   **Feladat:** DC filter (egypólusú felüláteresztő 10 Hz-en) a kimeneten, mert az aszimmetrikus hullámtörések masszív DC ofszetet (egyenáramú eltolódást) okoznak. Száraz/Nedves (Dry/Wet) keverés.

### Fázis 4: UI / UX Tervezés (Vizuális Tudomány)
*   **Design:** Mélykék/Sötét szürke alap.
*   **Vizualizáció:** Egy valós idejű részecskeszimuláció (`juce::OpenGLContext` vagy optimalizált `juce::Path`), ahol a részecskék áramlását a kimenő hang energiája és a Turbulencia paraméter zavarja meg (Kármán-féle örvénysor grafikája).
*   **Cél:** Ne egy eq-görbét mutasson, hanem áramló "szelet/folyadékot".

### Fázis 5: Tesztelés és Elvárások
*   **Teszt:** Szimpla szinuszhullám ráküldése maximum turbulenciával.
*   **Elvárt eredmény:** A kimenet nem egy digitális négyszögjel, hanem egy fokozatosan "előredőlő" (sawtooth-szerű), majd mikro-káoszba omló textúra, kattanások és NaN nélkül.

---
