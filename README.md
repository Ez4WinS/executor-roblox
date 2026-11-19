# ExecutorLab — Windows Luau Test Executor

Ein eigenständiges Windows-x64-Projekt mit nativer C++-GUI, lokaler Prozesskommunikation, dynamischer DLL und der offiziellen Open-Source-Luau-VM.

**Wichtig:** Kein Injector für RobloxPlayerBeta.exe. Der TestHost lädt die DLL bewusst in den *eigenen* Prozess. Die Objekte unter `game.Players` sind lokale Testdaten, keine realen Roblox-Objekte.

## EXE herunterladen (GitHub Actions)

1. GitHub-Repository öffnen und **Actions** auswählen.
2. Workflow **Build Windows EXE** öffnen.
3. Den neuesten **grünen** Lauf anklicken.
4. Unter **Artifacts** die Datei **ExecutorLab-Windows-x64** herunterladen.
5. ZIP entpacken; `ExecutorGUI.exe`, `TestHost.exe` und `ExecutorCore.dll` im selben Ordner belassen.

**Start:** Zuerst `TestHost.exe` starten (Konsolenfenster offen lassen), dann `ExecutorGUI.exe`. Im Editor Luau-Code eingeben und **Ausfuehren** wählen. Bei Bedarf Luau-Skripte über **Oeffnen** und **Speichern** laden/sichern. TestHost mit Strg+C beenden.

Ein Artefakt wird nur erstellt, wenn der Windows-Build und alle Tests erfolgreich sind. Die Artefakte bleiben in dieser Konfiguration 30 Tage verfügbar.

## Selber kompilieren

Windows 10/11 (x64), Visual Studio 2022 mit Desktopentwicklung für C++, CMake und Git:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\bin\Release\TestHost.exe
```

In einem zweiten Terminal:

```powershell
.\build\bin\Release\ExecutorGUI.exe
```

CMake lädt beim ersten Konfigurieren die offizielle Luau-Version **0.747** herunter.

## Architekturschema

```text
ExecutorGUI.exe (Editor + Konsole)
    |
    | lokale Named Pipe: \\.\pipe\ExecutorLab_v1
    | Request: u32 byteCount, UTF-8 code
    | Reply: u32 status, u32 byteCount, UTF-8 text
    v
TestHost.exe
    |
    | LoadLibraryExW — eigene DLL im eigenen Prozess
    v
ExecutorCore.dll
    |
    + Luau::compile(...) + luau_load(...) + lua_pcall(...)
    + print, Host.multiply, Host.greet
    + game.Players.LocalPlayer (nur Mock)
```

## Testskript

```luau
print("Hallo aus ExecutorLab!")
print(Host.multiply(6, 7))
print(game.Players.LocalPlayer.Name)
return Host.greet("Entwickler")
```

## Limits & Sicherheit

- Höchstens 128 KiB UTF-8 pro Skript und etwa 64 KiB Ausgabetext.
- 64 MiB pro Luau-VM verwalteter Speicher. Dies ist **kein** vollständiges Prozess-RAM-Limit.
- Etwa 1,5 s Ausführungsfrist durch VM-Interrupt. Kein harter OS-Limitierer für alle denkbaren nativen Funktionen.
- Für jede Ausführung wird eine neue Luau-VM erstellt. Die Luau-Sandbox verhindert den üblichen Zugriff auf Datei- und Betriebssystemfunktionen durch die Beispiel-API.
- Named Pipe ist lokal und für diesen einfachen TestHost ausgelegt; kein produktionsreifer Sicherheitskanal.
- Die GUI ist beim kurzen synchronen Skriptlauf zeitweise blockiert.
- CTest prüft Rückgabewerte, erfolgreiche Skripte, Syntax-/Laufzeitfehler, Timeout und isolierte Zustände.

## Weiterentwicklung

Für eigene Roblox-Spiele sind autorisierte Server-/Studio-Schnittstellen geeigneter als eine Manipulation des Roblox-Clients. Erweiterungen: asynchrones UI, bessere Zugriffskontrolle der Named Pipe, signierte Release-Binaries und automatisierte IPC-End-to-End-Tests.

Luau ist Open Source: https://github.com/luau-lang/luau
