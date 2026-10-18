# CENTRIX - Cents <-> Hz calculator (VST3) - Designed by Reyaan.

## Build (no local setup needed)
1. Push this folder to a GitHub repo.
2. Open the **Actions** tab -> *Build CENTRIX* -> download the `CENTRIX-VST3-windows-latest` artifact.
3. Windows: download `CENTRIX-Installer` and run `CENTRIX_Setup.exe` (installs to `C:\Program Files\Common Files\VST3\`).
4. In FL Studio: Options -> Manage plugins -> *Find installed plugins*. Add it as an effect (Mixer slot).

## Local build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target CENTRIX_VST3

## Using it
- CENTS > Hz: type cents + reference Hz -> result Hz.
- Hz > CENTS: type target Hz + reference Hz -> result cents.
- Pick ROOT KEY + SCALE to see the key after the pitch change (nearest semitone + cents detune).
- FINAL SCALE: SAME SCALE highlights the shifted key; RELATIVE highlights the relative major/minor of the shifted key.
- Arrows / mouse wheel on a field step by 1 (Shift = 10, Alt = 0.1).
- Window is resizable.
