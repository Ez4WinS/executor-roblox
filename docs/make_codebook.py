#!/usr/bin/env python3
"""Erstellt ein Nachschlagewerk mit allen aktuellen Projektcodes."""
from pathlib import Path
from datetime import datetime, timezone

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/"docs"/"generated"
BT=chr(96)

FILES=[
 ("CMakeLists.txt","CMake-Bauplan","Definiert die Windows-Programme, Tests und die Luau-Abhängigkeit."),
 ("src/Protocol.h","Kommunikationsprotokoll","Named-Pipe-Adresse, Nachrichtenformat sowie Größenlimits."),
 ("src/ExecutorCore.cpp","Luau-Execution-Core","DLL mit Luau-Compiler, eigener VM, API-Testfunktionen, Speicherverwaltung und Fehlerbehandlung."),
 ("src/TestHost.cpp","Host-Prozess","Lädt die DLL in seinen eigenen Prozess und verarbeitet die IPC-Anfragen."),
 ("src/ExecutorGUI.cpp","Windows-Oberfläche","Skript-Editor, Open/Save, Konsolenausgabe und automatischer Start des TestHost."),
 ("tests/CoreTests.cpp","Unit-Tests","Prüft erfolgreiche Skripte, Syntaxfehler, Laufzeitfehler, Timeout und Sandbox."),
 ("tests/ProcessTests.cpp","Integrationstest","Prüft die vollständige Kette von TestHost über Named Pipe bis zur Luau-Ausführung."),
 ("scripts/demo.luau","Beispielskript","Kurzes Beispiel für print, Host.multiply und simulierte Roblox-ähnliche Objekte."),
 (".github/workflows/build-windows.yml","Windows-Build","Kompiliert Programme, startet Tests und lädt das fertige ZIP hoch."),
 (".github/workflows/source-codebook.yml","Quellcode-Export","Erstellt und veröffentlicht dieses vollständige Nachschlagewerk automatisch."),
 ("docs/make_codebook.py","Dokumentationsgenerator","Liest die aktuellen Dateien aus dem Repository und fügt sie mit Erklärungen zusammen."),
 (".gitignore","Git-Ausschlüsse","Verhindert das Einchecken von lokalen Build- und Zwischendateien."),
]

def code_lang(path):
 if path.endswith((".cpp",".h")): return "cpp"
 if path.endswith(".luau"): return "lua"
 if path.endswith(".yml"): return "yaml"
 if path.endswith(".py"): return "python"
 if path=="CMakeLists.txt": return "cmake"
 return "text"

def inline(s):
 return BT+s+BT

def generate():
 parts=[
 "# ExecutorLab – Alle Codes und ihre Funktionen","",
 "Hier stehen die vollständigen aktuellen Projektdateien aus Ez4WinS/executor-roblox, jeweils mit einer Erklärung.",
 "",
 "Stand (UTC): "+datetime.now(timezone.utc).strftime("%d.%m.%Y %H:%M"),
 "",
 "## 1. Wie funktioniert der Executor?","",
 "Die Windows-Oberfläche ExecutorLab.exe startet TestHost.exe automatisch im Hintergrund.",
 "TestHost.exe lädt ExecutorCore.dll in den EIGENEN Prozess. Die GUI schickt Luau-Code über",
 "eine lokale Windows Named Pipe an TestHost; dort wird er in einer eigenen Luau-VM ausgeführt.",
 "Die Ergebnisse und Fehler werden über die Pipe an die GUI zurückgegeben.",
 "",
 "Wichtig: Dies ist eine kontrollierte TESTUMGEBUNG. game.Players.LocalPlayer ist nur ein Mock-Objekt.",
 "Das Programm injiziert keinen Code in RobloxPlayerBeta.exe und umgeht keinen Roblox-Schutz.",
 "",
 "## 2. Übersicht","",
 "| Datei | Wofür wird sie gebraucht? |","|---|---|",
 ]
 for path,title,description in FILES:
  parts.append("| "+inline(path)+" | "+title+": "+description+" |")
 parts += [
 "",
 "## 3. Was muss ich starten?","",
 "1. ZIP des erfolgreichen Windows-Builds entpacken.",
 "2. ExecutorLab.exe, TestHost.exe und ExecutorCore.dll zusammen im Ordner lassen.",
 "3. Nur ExecutorLab.exe öffnen: der TestHost startet automatisch unsichtbar.",
 "4. Luau-Code eingeben und auf Ausführen klicken.",
 "",
 "Beispiel:","",
 BT*3+"lua",
 'print("Hallo aus ExecutorLab!")',
 'print("6 mal 7 =", Host.multiply(6, 7))',
 'print("Mock-Spieler:", game.Players.LocalPlayer.Name)',
 'return Host.greet("Entwickler")',
 BT*3,"",
 "## 4. Vollständige Quellcodes","",
 "Die folgenden Codeblöcke geben die aktuellen GitHub-Dateien vollständig wieder.",
 "",
 ]
 for index,(path,title,description) in enumerate(FILES,1):
  file=ROOT/path
  if not file.is_file():
   raise FileNotFoundError(path)
  source=file.read_text(encoding="utf-8").rstrip("\n")
  parts += [
    "### 4."+str(index)+" "+inline(path),"",
    "**Aufgabe:** "+title+". "+description,"",
    BT*4+code_lang(path),source,BT*4,"",
  ]
 parts += [
 "## 5. Fragen für die Projektvorstellung","",
 "**Warum eine DLL?** Die Luau-Runtime ist eine separate C++-Bibliothek, die der eigene TestHost lädt.",
 "",
 "**Warum zwei Prozesse?** Die GUI und das Ausführen der Skripte sind getrennt, obwohl nur eine EXE manuell gestartet wird.",
 "",
 "**Wo kommt der RAM ins Spiel?** Luau verwaltet seinen VM-Zustand im Speicher des eigenen TestHost-Prozesses.",
 "",
 "**Ist dies ein Injector für Roblox?** Nein. Die Roblox-ähnliche Spiel-API ist simuliert.",
 "",
 "**Was ist bei einem Fehler?** Fehlermeldungen werden an die GUI zurückgesendet; Endlosschleifen werden zeitlich begrenzt.",
 "",
 "GitHub-Projekt: https://github.com/Ez4WinS/executor-roblox","",
 ]
 return "\n".join(parts)

if __name__=="__main__":
 OUT.mkdir(parents=True,exist_ok=True)
 document=generate()
 (OUT/"ALLE_CODES_ERKLAERT.md").write_text(document,encoding="utf-8")
 (OUT/"ALLE_CODES_ERKLAERT.txt").write_text(document,encoding="utf-8-sig")
 print("Codebook generated:",len(document),"characters,",len(FILES),"source files")
