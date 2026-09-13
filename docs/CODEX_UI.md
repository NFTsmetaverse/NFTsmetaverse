# Building the Codex screens

Task 10.3. The C++ half is done: the projection from Codex state into display-ready
views, and three widget bases that keep themselves current. The visual tree is
yours, and this document is the contract between them.

**Why it is split this way.** UMG widgets are binary assets and cannot be authored
outside the editor, so writing "the screen" here was never possible. What *is*
possible — and what you should not have to redo — is the part that decides what a
score means, what a link is called, which objections stand, and when to redraw.

## What to make

Three Widget Blueprints, each reparented to a C++ base:

| Widget Blueprint | Reparent to | Implement |
|---|---|---|
| `WBP_CodexChainScreen` | `CodexChainScreenBase` | `On Chain View Refreshed` |
| `WBP_FragmentInspector` | `FragmentInspectorBase` | `On Fragment View Refreshed` |
| `WBP_ChallengeResponse` | `ChallengeResponseViewBase` | `On Challenges Refreshed` |

There are deliberately **no `BindWidget` properties**. Name your widgets whatever
you like; the contract is the view struct and the one event.

## How each screen works

Call `Set Anchor Event` (or `Set Fragment`) once. After that the screen refreshes
itself — the bases subscribe to the Codex's `OnChainChanged` and
`OnFragmentRecovered`, so slotting a fragment on one screen updates another without
anyone remembering to. Your event fires; you redraw from the struct.

### Chain-building screen

`ChainView` (`FCodexChainView`) has everything:

- `Links` — **always six**, in transmission order, even when empty. Draw all six;
  the empty ones are the point.
- `Link.Band` — `Empty` / `Thin` / `Serviceable` / `Strong`. Drive colour from this
  rather than from the raw float, so every screen describes a score the same way.
- `Link.bIsWeakestLink` — exactly one link carries this. Mark it. This is where an
  opponent will attack, and the player should find that out here rather than in a
  debate.
- `Link.LinkDescription` — one line saying what the link is *for*. The chain's shape
  is the thing the player is learning; show it rather than assuming it.
- `AttestationStrength` (0–100) and `Band` for the whole chain.
- `CorroborationCount`, `bHasHostileConfirmation` — the two things that raise a
  chain beyond its links.
- `UnknownFragmentIDs` — non-empty means a content error. Show it loudly in
  development rather than drawing a blank row.

For the "what can go here" list, call `Get Slottable Fragments` with a link.
`bOnlyTierAppropriate = true` gives what belongs there; `false` gives everything,
because slotting a mismatch is allowed — it scores at half, and that is how a
player learns what a chain of custody is shaped like. Consider offering both.

Slot through `Slot Fragment` / `Unslot Fragment` on the widget, not through the
subsystem directly, so the screen is never showing a chain it has already changed.

### Fragment inspector

`FragmentView` (`FCodexFragmentView`). **`SourceReference` is the reason this screen
exists** — the real citation, e.g. *Papias, via Eusebius, Hist. eccl. 3.39.15*. Give
it prominence, not a footnote. `ScholarlyNote` says where the dispute actually lies
and should be visible without a second click; `bIsContested` marks fragments whose
own authenticity is disputed.

`Corroborations` and `Challenges` let the inspector be navigated outward rather than
read in isolation — make the entries clickable into the same widget.

> **Not yet built:** the sourced reader that shows the actual text behind a
> citation. It needs public-domain editions of the sources (Eusebius, Josephus,
> Tacitus, and a public-domain scripture translation) added as content. That is a
> sourcing task, not a code one, and it is the last piece of Pillar 4 — cite by
> reference, and let the player read the real thing.

### Challenge and response

`Challenges` (`FCodexChallengeView`), covering answered and unanswered alike so the
screen can show what has been met as well as what is outstanding.

The distinction that matters, and the one thing not to collapse:

- `bIsAnswered == false` and `bHasNoKnownAnswer == false` → the player has not found
  the answer. `MissingAnswers` names it. **This is a mission hook** — treat it as a
  reading list.
- `bHasNoKnownAnswer == true` → nothing in the record answers this. The longer
  ending of Mark is the case in point. Say so plainly. Sending the player looking
  for something that does not exist would be a lie, and this project's whole claim
  is that it does not do that.

`UnansweredCount` and `UnanswerableCount` are counted separately for exactly this
reason.

## Where the numbers come from

Nowhere in the UI layer. `UCodexViewLibrary` reads `FChainScoreBreakdown` from
`UCodexSubsystem` and formats it. If a screen recomputed anything, the number the
player sees and the number he is judged on in a debate could drift apart.

Band thresholds (`BandForLinkScore`) and the link names and descriptions
(`GetLinkName`, `GetLinkDescription`) are the tunable presentation surface. Scoring
weights are elsewhere — `FCodexScoringRules` on the subsystem.
