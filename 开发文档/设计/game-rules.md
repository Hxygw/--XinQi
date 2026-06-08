# XinQi (芯棋) Complete Rules

> Finalized: 2026-06-06

---

## Board

An N×N×N 3D lattice board (N ≥ 3). Each intersection can hold one stone. The outer walls of the board are treated as **friendly** — the board boundary counts as your own stone for encirclement purposes.

---

## Basic Moves

On each turn, a player must choose **one** of the following actions:

### Action 1: Place

Place your stone on any legal empty intersection.

**Legality check (all must pass):**

1. **Target is empty** (or an opponent's core vacancy — see Win Condition)
2. **No suicide**: After placing and completing captures, if the placed stone's connected group has zero liberties in *any* of the three sections (X/Y/Z), the move is illegal
3. **No super-KO**: The move must not immediately recreate the previous global board state
4. **First-move center ban**: On the very first move (Black, moveCount=0), the center cell (for odd N) or the innermost 2×2×2 core (for even N) cannot be played

### Action 2: Shift (挪子)

Move an **inner core** stone to a new position. Costs one turn.

**Definition of Inner Core:**

A stone is an inner core if all six directions (±X, ±Y, ±Z) are **either occupied by your own stones or are the board boundary**. The board boundary counts as friendly.

Minimum stones required to create a core at various positions:

| Position | Natural walls | Stones to add | Total stones |
|----------|--------------|---------------|-------------|
| Corner   | 3            | 3             | 4           |
| Edge     | 2            | 4             | 5           |
| Face     | 1            | 5             | 6           |
| Interior | 0            | 6             | 7           |

**Shift conditions (all must be met):**

- The stone being moved must be an inner core
- Target position must be empty
- Target must be adjacent to the source stone's connected component
- Placing at the target must be legal (no suicide, no super-KO)

**Shift effects:**

- Stone is removed from its current position and placed at the target
- The original position becomes a **core vacancy**
- If the shift causes opponent captures, process captures normally

**Irreversible scar:**

The **original owner** may never place a stone back into a core vacancy they created — regardless of whether via Place or Shift. Only the opponent can occupy that intersection. This constraint is permanent, leaving a structural weakness each time a core is shifted.

---

## Capture Rules

After a Place or the placement part of a Shift, captures are checked **independently** in each of the three 2D sections (X/Y/Z slices). If a connected group of opponent stones has **zero liberties in any single section**, that entire group is captured — even if it has liberties in other sections or is connected to friendly stones across sections.

---

## Endgame & Victory — Core Invasion System

The game ends immediately when either condition is met:

### Condition A: Clear Board (清台终局)

After you **trigger a capture** (via Place or Shift), if the opponent has **no inner cores remaining** on the board → you win immediately.

This is the safety-net victory path. A skilled defender can hide their last core in a well-protected structure, making this harder to achieve than Condition B.

### Condition B: Core Invasion (内芯侵入 — primary win path)

You legally occupy (via **Place or Shift**) an opponent's **core vacancy** → you win immediately.

**Typical path:** Opponent creates a core vacancy → you gradually break open the surrounding encirclement via section captures → the vacancy is no longer suicidal to enter → you legally place into it → victory

**Shift invasion:** Rare but possible — if one of your inner cores is adjacent to the opponent's core vacancy, you can shift your core into it, consuming your own core but winning the game on the spot.

### Board-full tiebreaker

The board is finite (N³ cells). Every turn either consumes an empty cell (Place) or creates a new core vacancy (Shift). The board will eventually fill completely. If neither player has won by the time the board is full, the player who made the last move without triggering a win loses.

---

## Strategic Notes (Observations from the rules, unverified)

### Opening — Four stones for a corner

The board-as-friendly rule makes corners extremely efficient: only **4 stones create a corner core** (3 board walls + 3 of your own stones = full encirclement). Both sides race to claim corners and build core stockpiles.

### Midgame Phase 1 — Production vs Probing

Two strategic choices:
- **Fortify**: Add stones around existing cores to increase core count and structural stability
- **Attack**: Probe opponent core structures to force the first strategic interaction

Defenders face a core trade-off: losing a core to capture = lost reserves; proactively shifting a core = creates a vulnerability (core vacancy) but can save the surrounding structure by giving it breathing room.

### Midgame Phase 2 — Core Vacancy Siege

After the first core is shifted, its vacancy appears:
- **Attacker**: Open gaps in the encirclement via section captures, aiming to legally enter the vacancy
- **Defender**: Protect the encirclement. Core dilemma — reinforce existing structures, or create another core vacancy (new scar) to relieve pressure?

### Role Swap

Attackers trade structural security for initiative. If the assault fails to break through, roles reverse — the former defender gets a counterattack window.

### Endgame

As the board fills, building stable structures becomes increasingly difficult. The game ends when one side successfully invades a core vacancy, or when a player has no legal moves left.

---

## Design Philosophy

- **Inner core = strategic reserve = latent weakness**: More cores = more movable resources, but also more potential vulnerabilities when shifted
- **Shift = irreversible**: Each shift leaves a permanent scar, preventing runaway "shift everything into an immortal fortress"
- **No artificial limits**: No formulas, decay functions, or move limits — the game state itself defines termination
- **Boundary = friendly**: Naturally creates "corner-value > edge-value > center-value" intuition without extra rules
