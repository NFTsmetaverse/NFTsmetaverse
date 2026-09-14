# Handing this to a coding agent on your machine

Paste the block below into any coding agent running **locally on the machine that
has UE 5.8 installed** — Codex CLI, Claude Code, Cursor, whatever you use. Not a
browser chat: the whole job is running a compiler and reading what it says, and an
agent without the engine can only guess, which is exactly the position this project
has been stuck in.

The code is already written and in git. Nothing needs pasting except this.

---

```
You are picking up an Unreal Engine 5 project that has never been compiled.

REPO
  git clone https://github.com/NFTsmetaverse/NFTsmetaverse
  The default branch has everything. No branch switching needed.

WHAT IT IS
  "Chain of Witnesses" — a historically grounded action-RPG about the transmission
  of the New Testament. What exists is the simulation layer: nine UGameInstance
  subsystems (campaign timeline, codex of testimony fragments, dialogue, reputation
  with word-of-mouth news travel, campaign map with a Roman road/sea graph, debate,
  witness missions, combat encounters), plus a playable layer on top of it
  (UChainGameFlowSubsystem, AChainGameMode, AChainPlayerController, AChainHUD) that
  makes the whole thing driveable from the keyboard with no editor-built assets.
  ~15,500 lines of C++ in 58 files, 14 JSON DataTables with 196 rows, and 59
  automation tests across ten suites.

  Read README.md and docs/SETUP.md first. docs/MASTER_PROMPT.md is the original
  design brief and is authoritative on intent.

YOUR JOB, IN THIS ORDER
  1. Get it to compile on UE 5.8. It was written against 5.4 by an agent with no
     compiler, so expect a few dozen errors. This is the whole task; everything
     else is secondary.
  2. Get the automation tests running (Session Frontend → Automation, filter
     "ChainOfWitnesses"). They have never run. Report what fails.
  3. Report back: what was version drift, what was a real bug, what is still red.

BEFORE YOU START
  python3 Tools/verify.py
  Three static checkers — UHT reflection rules, JSON tables against their row
  structs, cross-table ID references. They pass. If you change C++ or content,
  they must still pass. Tools/README.md explains what each covers.

HARD RULES
  - Do NOT delete, skip, or #if 0 a test to make the build green. A failing test is
    information; this project has never had any.
  - Do NOT rewrite the architecture. If a subsystem's design seems wrong, say so
    and leave it. The design is deliberate and documented.
  - Separate version drift from real bugs in your report. Some errors will be UE
    5.4→5.8 API changes; some will be genuine mistakes. Say which is which. If you
    cannot tell, say that instead of guessing.
  - Two known hedges already applied, both unverified against a real 5.8:
    Target.cs uses EngineIncludeOrderVersion.Latest (the pinned Unreal5_4 value may
    no longer exist), and the automation test flags are centralised in
    Private/Tests/ChainAutomationFlags.h as CHAIN_TEST_FLAGS. If EAutomationTestFlags
    moved again, fix it there once rather than in 55 places.
  - Content rule, non-negotiable: NEVER write text presented as a biblical verse, a
    patristic quotation, or a manuscript reading. Cite real sources by reference
    only (e.g. "Papias, via Eusebius, Hist. eccl. 3.39.15"). Original dialogue for
    characters is fine. Fabricated primary sources are not. This is the project's
    central claim and the reason it exists.
  - Flag any historical claim you believe is inaccurate or overstated before
    writing code around it.

WHAT DOES NOT EXIST YET, AND IS NOT YOUR JOB RIGHT NOW
  No level, pawn, widget, mesh, animation or audio — Content/ holds only JSON.
  No melee layer (docs/COMBAT.md documents the seam it plugs into).
  Do not start building these until it compiles.

DONE LOOKS LIKE
  The editor opens, the module compiles, the automation tests run, and you can tell
  me which failures are mine and which are the engine's.

  If you get that far and want to prove it end to end: follow docs/SETUP.md to
  import the tables and set Default GameMode to ChainGameMode, press Play, open the
  console and type ChainSlice. That plays the authored Rome AD 65 scene from the
  keyboard. If that works, the project is real.
```

---

## After it compiles

The next steps are in `docs/SETUP.md`: import the fourteen DataTables, assign them
in **Project Settings → Game → Chain of Witnesses Content**, make an empty level,
set **Default GameMode** to `ChainGameMode`, and press Play. You are looking for
this in the Output Log:

```
LogChainBootstrap: Registered 196 rows of campaign content.
LogChainBootstrap: Content validated with no problems.
LogChainFlow: Ready. Open the console (~) and type ChainHelp, or press H.
```

That is the whole simulation coming up. `ChainSlice` then plays the Rome AD 65
scene from the keyboard.

## If you would rather not run an agent locally

Build it yourself in the editor and send the build log back to whoever is helping
you. A UE compile log is long and looks like noise, but sorting it into "real bug"
and "version drift" is straightforward work for anyone with the source in front of
them. The log is the thing that has been missing from this project since the first
line was written.
