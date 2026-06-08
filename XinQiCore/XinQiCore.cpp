#include "XinQiCore.h"
#include <cstring>
#include <cstdio>
#include <new>
#include <cstdlib>
#include <cassert>

// ============================================================
// Internal helpers
// ============================================================

static inline int32_t N(const GameState* gs) { return gs->size; }
static inline int32_t NN(const GameState* gs) { return gs->size * gs->size; }
static inline int32_t Idx(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    return (x * N(gs) + y) * N(gs) + z;
}
static inline int8_t* Board(GameState* gs) {
    return reinterpret_cast<int8_t*>(gs + 1);
}
static inline const int8_t* Board(const GameState* gs) {
    return reinterpret_cast<const int8_t*>(gs + 1);
}
static inline int8_t Opp(int8_t color) { return (color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK; }

static inline bool IsFriend(int8_t cell, int8_t color) { return cell == color; }
static inline bool IsOpponent(int8_t cell, int8_t color) { return cell == Opp(color); }
static inline bool IsOutOfBounds(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    return x < 0 || x >= gs->size || y < 0 || y >= gs->size || z < 0 || z >= gs->size;
}

// ============================================================
// Zobrist hashing
// ============================================================

static uint64_t zobTable[MAX_BOARD_SIZE * MAX_BOARD_SIZE * MAX_BOARD_SIZE][CELL_COUNT];

static uint64_t splitmix64(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void InitZobrist() {
    static bool initDone = false;
    if (initDone) return;
    initDone = true;
    uint64_t seed = 0xDEADBEEFCAFEBABEULL;
    for (int i = 0; i < MAX_BOARD_SIZE * MAX_BOARD_SIZE * MAX_BOARD_SIZE; ++i) {
        for (int c = 0; c < CELL_COUNT; ++c) {
            zobTable[i][c] = splitmix64(seed);
        }
    }
}

// Update hash when cell at flat-index `idx` changes from `oldVal` to `newVal`
static inline void UpdateHash(uint64_t& hash, int32_t idx, int8_t oldVal, int8_t newVal) {
    hash ^= zobTable[idx][oldVal];
    hash ^= zobTable[idx][newVal];
}

// ============================================================
// 3D connected component (6-neighbor) BFS — for shift adjacency
// ============================================================

static bool IsAdjacentToComponent(const GameState* gs,
                                   int8_t sx, int8_t sy, int8_t sz,
                                   int8_t tx, int8_t ty, int8_t tz) {
    // BFS from (sx,sy,sz) restricted to stones of the same color.
    // Returns true if (tx,ty,tz) is within Chebyshev-1 of any stone in the component.
    const int32_t total = gs->size * gs->size * gs->size;
    int8_t color = XinQi_GetCell(gs, sx, sy, sz);
    if (color == CELL_EMPTY || color >= CELL_BLACK_VACANCY) return false;

    // Use a simple visited byte-array on the heap
    // Since N is small, we can just do a stack-based DFS or use a bool array
    bool* visited = (bool*)calloc(total, sizeof(bool));
    if (!visited) return false;

    // Stack for DFS
    struct St { int8_t x, y, z; };
    St* stack = (St*)malloc(total * sizeof(St));
    if (!stack) { free(visited); return false; }

    int sp = 0;
    stack[sp++] = { sx, sy, sz };
    visited[Idx(gs, sx, sy, sz)] = true;

    const int dx[6] = { 1, -1, 0, 0, 0, 0 };
    const int dy[6] = { 0, 0, 1, -1, 0, 0 };
    const int dz[6] = { 0, 0, 0, 0, 1, -1 };

    bool found = false;
    while (sp > 0 && !found) {
        St cur = stack[--sp];
        // Check if target is adjacent to cur
        for (int d = 0; d < 6; ++d) {
            if (cur.x + dx[d] == tx && cur.y + dy[d] == ty && cur.z + dz[d] == tz) {
                found = true;
                break;
            }
        }
        for (int d = 0; d < 6; ++d) {
            int8_t nx = (int8_t)(cur.x + dx[d]);
            int8_t ny = (int8_t)(cur.y + dy[d]);
            int8_t nz = (int8_t)(cur.z + dz[d]);
            if (IsOutOfBounds(gs, nx, ny, nz)) continue;
            int32_t idx = Idx(gs, nx, ny, nz);
            if (!visited[idx] && XinQi_GetCell(gs, nx, ny, nz) == color) {
                visited[idx] = true;
                if (sp < total) stack[sp++] = { nx, ny, nz };
            }
        }
    }

    free(stack);
    free(visited);
    return found;
}

// ============================================================
// Section-based capture helpers
// ============================================================

// BFS in a 2D section slice.
// sectionType: 0 = X (YZ-plane), 1 = Y (XZ-plane), 2 = Z (XY-plane)
// fixedCoord: the constant coordinate for the slice (e.g., x for X-section)
// Returns true if the component containing (y1,z1) [interpreted as section-coords] has no liberties.

static bool HasNoLibertiesInSection(const GameState* gs,
                                     int sectionType, int fixedCoord,
                                     int a, int b,  // (a,b) = section coordinates
                                     int8_t color,
                                     bool* visited) {
    const int Nn = N(gs);
    int sliceCells = Nn * Nn;

    // Stack-based DFS
    struct Pt { int a, b; };
    Pt* stack = (Pt*)malloc(sliceCells * sizeof(Pt));
    if (!stack) return false;

    int sp = 0;
    stack[sp++] = { a, b };

    // Mark visited in section coordinates: visited[a * Nn + b]
    visited[a * Nn + b] = true;

    const int da[4] = { 1, -1, 0, 0 };
    const int db[4] = { 0, 0, 1, -1 };

    bool hasLiberty = false;

    while (sp > 0 && !hasLiberty) {
        Pt cur = stack[--sp];
        for (int d = 0; d < 4; ++d) {
            int na = cur.a + da[d];
            int nb = cur.b + db[d];
            if (na < 0 || na >= Nn || nb < 0 || nb >= Nn) continue;

            // Convert section coordinates back to 3D to read the cell
            int8_t cellVal;
            if (sectionType == 0) { // X-section, YZ-plane: (fixedCoord, na, nb)
                cellVal = XinQi_GetCell(gs, (int8_t)fixedCoord, (int8_t)na, (int8_t)nb);
            } else if (sectionType == 1) { // Y-section, XZ-plane: (na, fixedCoord, nb)
                cellVal = XinQi_GetCell(gs, (int8_t)na, (int8_t)fixedCoord, (int8_t)nb);
            } else { // Z-section, XY-plane: (na, nb, fixedCoord)
                cellVal = XinQi_GetCell(gs, (int8_t)na, (int8_t)nb, (int8_t)fixedCoord);
            }

            // Any vacancy is an empty cell and counts as a liberty for either color.
            if (cellVal == CELL_EMPTY || cellVal == CELL_BLACK_VACANCY || cellVal == CELL_WHITE_VACANCY) {
                hasLiberty = true;
                break;
            } else if (cellVal == color && !visited[na * Nn + nb]) {
                visited[na * Nn + nb] = true;
                if (sp < sliceCells) stack[sp++] = { na, nb };
            }
            // opponent stones or own core vacancy block the liberty
        }
    }

    free(stack);
    return !hasLiberty;
}

// Perform capture checks for ALL three sections.
// Returns the number of stones captured, or -1 if the move would be suicide for `moveColor`.
// When `dryRun` is true, doesn't modify the board — just counts.
// When `dryRun` is false, actually removes captured stones (updates board and hash).
static int DoCapture(GameState* gs, int8_t moveColor, bool dryRun) {
    const int Nn = N(gs);
    const int total = Nn * Nn * Nn;
    int8_t opp = Opp(moveColor);

    // Captured flat-index set (max `total` entries)
    bool* captured = (bool*)calloc(total, sizeof(bool));
    if (!captured) return 0;

    // Visited array for BFS, sized Nn*Nn (reused per slice)
    bool* visited = (bool*)calloc(Nn * Nn, sizeof(bool));
    if (!visited) { free(captured); return 0; }

    // Temporary list of component stones
    struct Pt { int a, b; };
    Pt* comp = (Pt*)malloc(Nn * Nn * sizeof(Pt));
    if (!comp) { free(captured); free(visited); return 0; }

    int captureCount = 0;

    for (int sectionType = 0; sectionType < 3; ++sectionType) {
        for (int fixed = 0; fixed < Nn; ++fixed) {
            // Clear visited for each slice
            memset(visited, 0, Nn * Nn * sizeof(bool));

            for (int a = 0; a < Nn; ++a) {
                for (int b = 0; b < Nn; ++b) {
                    if (visited[a * Nn + b]) continue;

                    // Read cell value
                    int8_t cellVal;
                    if (sectionType == 0)
                        cellVal = XinQi_GetCell(gs, (int8_t)fixed, (int8_t)a, (int8_t)b);
                    else if (sectionType == 1)
                        cellVal = XinQi_GetCell(gs, (int8_t)a, (int8_t)fixed, (int8_t)b);
                    else
                        cellVal = XinQi_GetCell(gs, (int8_t)a, (int8_t)b, (int8_t)fixed);

                    if (cellVal != opp) continue; // not opponent in this slice

                    // Find the connected component via BFS
                    int compSize = 0;
                    int head = 0;
                    comp[compSize++] = { a, b };
                    visited[a * Nn + b] = true;

                    const int da[4] = { 1, -1, 0, 0 };
                    const int db[4] = { 0, 0, 1, -1 };

                    while (head < compSize) {
                        Pt cur = comp[head++];
                        for (int d = 0; d < 4; ++d) {
                            int na = cur.a + da[d];
                            int nb = cur.b + db[d];
                            if (na < 0 || na >= Nn || nb < 0 || nb >= Nn) continue;
                            if (visited[na * Nn + nb]) continue;

                            int8_t ncell;
                            if (sectionType == 0)
                                ncell = XinQi_GetCell(gs, (int8_t)fixed, (int8_t)na, (int8_t)nb);
                            else if (sectionType == 1)
                                ncell = XinQi_GetCell(gs, (int8_t)na, (int8_t)fixed, (int8_t)nb);
                            else
                                ncell = XinQi_GetCell(gs, (int8_t)na, (int8_t)nb, (int8_t)fixed);

                            if (ncell == opp && compSize < Nn * Nn) {
                                visited[na * Nn + nb] = true;
                                comp[compSize++] = { na, nb };
                            }
                        }
                    }

                    // Check if component has ANY liberty in this 2D slice
                    bool hasLiberty = false;
                    for (int i = 0; i < compSize && !hasLiberty; ++i) {
                        Pt cur = comp[i];
                        for (int d = 0; d < 4; ++d) {
                            int na = cur.a + da[d];
                            int nb = cur.b + db[d];
                            if (na < 0 || na >= Nn || nb < 0 || nb >= Nn) continue; // boundary = friendly, no liberty

                            int8_t ncell;
                            if (sectionType == 0)
                                ncell = XinQi_GetCell(gs, (int8_t)fixed, (int8_t)na, (int8_t)nb);
                            else if (sectionType == 1)
                                ncell = XinQi_GetCell(gs, (int8_t)na, (int8_t)fixed, (int8_t)nb);
                            else
                                ncell = XinQi_GetCell(gs, (int8_t)na, (int8_t)nb, (int8_t)fixed);

                            if (ncell == CELL_EMPTY || ncell == CELL_BLACK_VACANCY || ncell == CELL_WHITE_VACANCY) {
                                hasLiberty = true;
                                break;
                            }
                        }
                    }

                    if (!hasLiberty) {
                        // Mark all stones in this component for capture
                        for (int i = 0; i < compSize; ++i) {
                            Pt cur = comp[i];
                            int32_t idx3d;
                            if (sectionType == 0)
                                idx3d = Idx(gs, (int8_t)fixed, (int8_t)cur.a, (int8_t)cur.b);
                            else if (sectionType == 1)
                                idx3d = Idx(gs, (int8_t)cur.a, (int8_t)fixed, (int8_t)cur.b);
                            else
                                idx3d = Idx(gs, (int8_t)cur.a, (int8_t)cur.b, (int8_t)fixed);
                            if (!captured[idx3d]) {
                                captured[idx3d] = true;
                                captureCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    // Apply captures
    if (!dryRun && captureCount > 0) {
        int8_t* board = Board(gs);
        for (int32_t i = 0; i < total; ++i) {
            if (captured[i]) {
                int8_t oldVal = board[i];
                board[i] = CELL_EMPTY;
                UpdateHash(gs->hash, i, oldVal, CELL_EMPTY);
            }
        }
    }

    free(comp);
    free(visited);
    free(captured);
    return captureCount;
}

// Check if placing `color` at (x,y,z) results in suicide.
static bool IsSuicide(const GameState* gs, int8_t x, int8_t y, int8_t z, int8_t color) {
    // Copy board, place stone, run capture, check own liberties
    GameState* copy = XinQi_Clone(gs);
    if (!copy) return true; // safe assumption

    int8_t* board = Board(copy);
    int32_t idx = Idx(copy, x, y, z);
    board[idx] = color;
    // Capture is done on the copy's board directly
    // We need a version of DoCapture that works on raw board
    // Let me use a simpler approach

    // Actually, let me just do a local BFS check per section directly on the placed stone's group

    // For suicide, after placing and opponent captures:
    // Check each section — if ANY section has the own group with 0 liberties → suicide

    // First do opponent capture
    int32_t nn = N(copy);
    int8_t* cpBoard = Board(copy);

    // Mark captured stones
    // ... this is getting complex. Let me use a simpler approach.

    // Since I have DoCapture, let me use it by calling the version that works on a copy
    // Wait, DoCapture modifies the hash too. Let me write a simpler helper.

    // Actually, let me just implement a direct 3-section liberty check on the copy after capture
    // Use the same capture logic

    // For simplicity, let me call DoCapture on the clone (this is a local copy anyway)
    DoCapture(copy, color, false);

    // Now check if the placed stone's group has liberties in all 3 sections
    // Check X-section at x, Y-section at y, Z-section at z

    bool* visited = (bool*)calloc(nn * nn, sizeof(bool));
    if (!visited) { XinQi_Destroy(copy); return true; }

    bool anySectionNoLiberty = false;

    // X-section
    memset(visited, 0, nn * nn * sizeof(bool));
    if (HasNoLibertiesInSection(copy, 0, x, y, z, color, visited)) {
        anySectionNoLiberty = true;
    }

    // Y-section
    if (!anySectionNoLiberty) {
        memset(visited, 0, nn * nn * sizeof(bool));
        if (HasNoLibertiesInSection(copy, 1, y, x, z, color, visited)) {
            anySectionNoLiberty = true;
        }
    }

    // Z-section
    if (!anySectionNoLiberty) {
        memset(visited, 0, nn * nn * sizeof(bool));
        if (HasNoLibertiesInSection(copy, 2, z, x, y, color, visited)) {
            anySectionNoLiberty = true;
        }
    }

    free(visited);
    XinQi_Destroy(copy);
    return anySectionNoLiberty;
}

// ============================================================
// Public API — Lifecycle
// ============================================================

GameState* XinQi_Create(int8_t size) {
    if (size < 3 || size > MAX_BOARD_SIZE) return nullptr;
    InitZobrist();

    int32_t total = size * size * size;
    // Allocate: GameState + board data
    void* mem = malloc(sizeof(GameState) + total * sizeof(int8_t));
    if (!mem) return nullptr;

    GameState* gs = new(mem) GameState;
    gs->size = size;
    gs->current = COLOR_BLACK;
    gs->moveCount = 0;
    gs->hash = 0;
    gs->prevHash = 0;
    gs->lastCaptureCount = 0;

    int8_t* board = Board(gs);
    memset(board, CELL_EMPTY, total);

    return gs;
}

void XinQi_Destroy(GameState* gs) {
    if (gs) {
        gs->~GameState(); // not strictly needed but clean
        free(gs);
    }
}

GameState* XinQi_Clone(const GameState* gs) {
    if (!gs) return nullptr;
    int32_t total = gs->size * gs->size * gs->size;
    void* mem = malloc(sizeof(GameState) + total * sizeof(int8_t));
    if (!mem) return nullptr;

    GameState* copy = new(mem) GameState;
    memcpy(copy, gs, sizeof(GameState));
    memcpy(Board(copy), Board(gs), total);
    return copy;
}

// ============================================================
// Public API — Accessors
// ============================================================

int8_t XinQi_GetCell(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    if (!gs || IsOutOfBounds(gs, x, y, z)) return CELL_EMPTY;
    return Board(gs)[Idx(gs, x, y, z)];
}

int32_t XinQi_CellCount(const GameState* gs) {
    if (!gs) return 0;
    int32_t n = gs->size;
    return n * n * n;
}

const int8_t* XinQi_BoardData(const GameState* gs) { return Board(gs); }
int8_t* XinQi_BoardData(GameState* gs) { return Board(gs); }

// ============================================================
// Public API — Queries
// ============================================================

bool XinQi_IsCore(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    if (!gs) return false;
    int8_t cell = XinQi_GetCell(gs, x, y, z);
    if (cell != COLOR_BLACK && cell != COLOR_WHITE) return false;

    int8_t color = cell;
    int dx[6] = { 1, -1, 0, 0, 0, 0 };
    int dy[6] = { 0, 0, 1, -1, 0, 0 };
    int dz[6] = { 0, 0, 0, 0, 1, -1 };

    for (int d = 0; d < 6; ++d) {
        int8_t nx = (int8_t)(x + dx[d]);
        int8_t ny = (int8_t)(y + dy[d]);
        int8_t nz = (int8_t)(z + dz[d]);

        if (IsOutOfBounds(gs, nx, ny, nz)) continue; // boundary = friend
        int8_t ncell = XinQi_GetCell(gs, nx, ny, nz);
        if (ncell != color) return false; // not friend → not a core
    }
    return true;
}

bool XinQi_HasAnyCore(const GameState* gs, int8_t color) {
    if (!gs) return false;
    int32_t total = XinQi_CellCount(gs);
    const int8_t* board = Board(gs);
    int32_t n = gs->size;

    // We could call IsCore for each stone, but that would be O(N³ × 6).
    // Let me precompute the adjacency for efficiency.
    // Actually for N <= 13, N³ = 2197, and IsCore is 6 checks = ~13k ops, fine.

    for (int32_t idx = 0; idx < total; ++idx) {
        if (board[idx] != color) continue;
        int8_t x = (int8_t)(idx / (n * n));
        int8_t y = (int8_t)((idx / n) % n);
        int8_t z = (int8_t)(idx % n);
        if (XinQi_IsCore(gs, x, y, z)) return true;
    }
    return false;
}

// ============================================================
// Pre-checks
// ============================================================

int8_t XinQi_CheckPlace(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    if (!gs) return ERR_OCCUPIED;
    if (IsOutOfBounds(gs, x, y, z)) return ERR_OCCUPIED;

    int8_t cell = XinQi_GetCell(gs, x, y, z);
    int8_t color = gs->current;

    // 1. Must be empty
    if (cell != CELL_EMPTY) {
        // Can we place on opponent's core vacancy? Yes — that's a winning move!
        if ((color == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
            (color == COLOR_WHITE && cell == CELL_BLACK_VACANCY)) {
            // This is actually a legal place (core invasion), but the legality
            // will be verified by the full Place function.
            return RESULT_OK; // proceed, Place will handle the win
        }
        if ((color == COLOR_BLACK && cell == CELL_BLACK_VACANCY) ||
            (color == COLOR_WHITE && cell == CELL_WHITE_VACANCY)) {
            return ERR_CORE_VACANCY;
        }
        return ERR_OCCUPIED;
    }

    // 2. First move center ban
    if (gs->moveCount == 0 && color == COLOR_BLACK) {
        int8_t n = gs->size;
        int8_t centerMin = (int8_t)((n - 1) / 2);
        int8_t centerMax = (int8_t)(n / 2); // for even N, center range
        if (centerMax < centerMin) centerMax = centerMin;

        if (x >= centerMin && x <= centerMax &&
            y >= centerMin && y <= centerMax &&
            z >= centerMin && z <= centerMax) {
            return ERR_FIRST_MOVE_CENTER;
        }
    }

    // 3. Suicide check
    if (IsSuicide(gs, x, y, z, color)) {
        return ERR_SUICIDE;
    }

    // 4. Super KO check is done in Place (need hash calculation)
    return RESULT_OK;
}

int8_t XinQi_CheckShift(const GameState* gs,
                         int8_t fx, int8_t fy, int8_t fz,
                         int8_t tx, int8_t ty, int8_t tz) {
    if (!gs) return ERR_OCCUPIED;
    if (IsOutOfBounds(gs, fx, fy, fz) || IsOutOfBounds(gs, tx, ty, tz)) return ERR_OCCUPIED;

    int8_t color = gs->current;
    int8_t srcCell = XinQi_GetCell(gs, fx, fy, fz);
    int8_t dstCell = XinQi_GetCell(gs, tx, ty, tz);

    // 1. Source must be player's stone
    if (srcCell != color) return ERR_NOT_CORE;

    // 2. Source must be a core
    if (!XinQi_IsCore(gs, fx, fy, fz)) return ERR_NOT_CORE;

    // 3. Target must be empty (or opponent's core vacancy)
    if (dstCell == CELL_EMPTY) {
        // OK
    } else if ((color == COLOR_BLACK && dstCell == CELL_WHITE_VACANCY) ||
               (color == COLOR_WHITE && dstCell == CELL_BLACK_VACANCY)) {
        // Shifting into opponent's core vacancy — this would be a win, handled in Shift
    } else if ((color == COLOR_BLACK && dstCell == CELL_BLACK_VACANCY) ||
               (color == COLOR_WHITE && dstCell == CELL_WHITE_VACANCY)) {
        return ERR_CORE_VACANCY;
    } else {
        return ERR_OCCUPIED;
    }

    // 4. Target must be adjacent to source's 3D connected component
    if (!IsAdjacentToComponent(gs, fx, fy, fz, tx, ty, tz)) {
        return ERR_NOT_ADJACENT;
    }

    // 5. Suicide check after shift (shift = stone moves, old becomes vacancy)
    // The suicide check after shift is: after removing from source and placing at target,
    // and after opponent captures, does the player's group have any section with 0 liberties?
    // We'll check this in Shift() via full simulation.
    // For the pre-check, skip the full suicide check (it's done in Shift).

    return RESULT_OK;
}

// ============================================================
// Moves — Place
// ============================================================

int8_t XinQi_Place(GameState* gs, int8_t x, int8_t y, int8_t z) {
    if (!gs) return ERR_OCCUPIED;
    if (IsOutOfBounds(gs, x, y, z)) return ERR_OCCUPIED;

    int8_t color = gs->current;
    int8_t cell = XinQi_GetCell(gs, x, y, z);

    // Check core vacancy / occupied
    if (cell != CELL_EMPTY) {
        if ((color == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
            (color == COLOR_WHITE && cell == CELL_BLACK_VACANCY)) {
            // Core invasion target — proceed with placement
        } else if ((color == COLOR_BLACK && cell == CELL_BLACK_VACANCY) ||
                   (color == COLOR_WHITE && cell == CELL_WHITE_VACANCY)) {
            return ERR_CORE_VACANCY;
        } else {
            return ERR_OCCUPIED;
        }
    }

    // First move center ban
    if (gs->moveCount == 0 && color == COLOR_BLACK) {
        int8_t n = gs->size;
        int8_t centerMin = (int8_t)((n - 1) / 2);
        int8_t centerMax = (int8_t)(n / 2);
        if (centerMax < centerMin) centerMax = centerMin;
        if (x >= centerMin && x <= centerMax &&
            y >= centerMin && y <= centerMax &&
            z >= centerMin && z <= centerMax) {
            return ERR_FIRST_MOVE_CENTER;
        }
    }

    // Super KO check — clone, make move, compare hash with prevHash
    // Create a temporary clone, execute the full move, then check KO
    GameState* sim = XinQi_Clone(gs);
    if (!sim) return ERR_OCCUPIED;

    // Store hash before the move
    uint64_t hashBefore = gs->hash;

    // Place stone on the clone
    int32_t idx = Idx(sim, x, y, z);
    int8_t* sboard = Board(sim);
    int8_t oldVal = sboard[idx];
    sboard[idx] = color;
    uint64_t newHash = hashBefore;
    UpdateHash(newHash, idx, oldVal, color);
    sim->hash = newHash;

    // Check core invasion (before captures — placing on opponent's core vacancy)
    bool coreInvasion = (color == COLOR_BLACK && oldVal == CELL_WHITE_VACANCY) ||
                        (color == COLOR_WHITE && oldVal == CELL_BLACK_VACANCY);

    // Perform opponent captures
    DoCapture(sim, color, false);
    newHash = sim->hash;

    // Check suicide
    bool suicide = false;
    {
        int32_t nn = gs->size;
        bool* visited = (bool*)calloc(nn * nn, sizeof(bool));
        if (visited) {
            if (HasNoLibertiesInSection(sim, 0, x, y, z, color, visited)) suicide = true;
            if (!suicide) {
                memset(visited, 0, nn * nn * sizeof(bool));
                if (HasNoLibertiesInSection(sim, 1, y, x, z, color, visited)) suicide = true;
            }
            if (!suicide) {
                memset(visited, 0, nn * nn * sizeof(bool));
                if (HasNoLibertiesInSection(sim, 2, z, x, y, color, visited)) suicide = true;
            }
            free(visited);
        }
    }

    if (suicide) {
        XinQi_Destroy(sim);
        return ERR_SUICIDE;
    }

    // Super KO: new state matches state from 2 moves ago?
    if (gs->moveCount >= 2 && newHash == gs->prevHash) {
        XinQi_Destroy(sim);
        return ERR_SUPERKO;
    }

    // Move is legal — apply to real game state
    XinQi_Destroy(sim);

    int8_t* board = Board(gs);
    oldVal = board[idx];
    board[idx] = color;
    UpdateHash(gs->hash, idx, oldVal, color);

    // Perform captures on real board, track count
    int captured = DoCapture(gs, color, false);
    gs->lastCaptureCount = captured;

    // Update prevHash and hash (DoCapture already updated hash)
    gs->prevHash = hashBefore;
    gs->moveCount++;

    // Check win conditions
    // 1. Core invasion (placing on opponent's core vacancy)
    if (coreInvasion) {
        gs->current = Opp(color);
        return WIN_CORE_INVASION;
    }

    // 2. Clear board — only when captures actually happened this move
    if (captured > 0 && !XinQi_HasAnyCore(gs, Opp(color))) {
        gs->current = Opp(color);
        return WIN_CLEAR_BOARD;
    }

    // 3. No legal moves → current player wins (stuck = their position prevailed)
    if (!XinQi_HasAnyLegalMove(gs)) {
        return WIN_NO_LEGAL_MOVE;
    }

    gs->current = Opp(color);
    return RESULT_OK;
}

// ============================================================
// Moves — Shift
// ============================================================

int8_t XinQi_Shift(GameState* gs,
                    int8_t fx, int8_t fy, int8_t fz,
                    int8_t tx, int8_t ty, int8_t tz) {
    if (!gs) return ERR_OCCUPIED;
    if (IsOutOfBounds(gs, fx, fy, fz) || IsOutOfBounds(gs, tx, ty, tz)) return ERR_OCCUPIED;

    int8_t color = gs->current;
    int8_t srcCell = XinQi_GetCell(gs, fx, fy, fz);
    int8_t dstCell = XinQi_GetCell(gs, tx, ty, tz);

    // 1. Source must be player's stone and a core
    if (srcCell != color) return ERR_NOT_CORE;
    if (!XinQi_IsCore(gs, fx, fy, fz)) return ERR_NOT_CORE;

    // 2. Target must be empty or opponent's core vacancy
    bool coreInvasion = false;
    if (dstCell == CELL_EMPTY) {
        // OK
    } else if ((color == COLOR_BLACK && dstCell == CELL_WHITE_VACANCY) ||
               (color == COLOR_WHITE && dstCell == CELL_BLACK_VACANCY)) {
        coreInvasion = true;
    } else if ((color == COLOR_BLACK && dstCell == CELL_BLACK_VACANCY) ||
               (color == COLOR_WHITE && dstCell == CELL_WHITE_VACANCY)) {
        return ERR_CORE_VACANCY;
    } else {
        return ERR_OCCUPIED;
    }

    // 3. Target must be adjacent to source's 3D component
    if (!IsAdjacentToComponent(gs, fx, fy, fz, tx, ty, tz)) {
        return ERR_NOT_ADJACENT;
    }

    // 4. Simulate the full shift on a clone for suicide/KO checks
    GameState* sim = XinQi_Clone(gs);
    if (!sim) return ERR_OCCUPIED;

    uint64_t hashBefore = gs->hash;
    int8_t* sboard = Board(sim);
    int32_t srcIdx = Idx(sim, fx, fy, fz);
    int32_t dstIdx = Idx(sim, tx, ty, tz);

    // Remove stone from source, create core vacancy
    int8_t vacancyType = (color == COLOR_BLACK) ? CELL_BLACK_VACANCY : CELL_WHITE_VACANCY;
    int8_t oldSrcVal = sboard[srcIdx];
    sboard[srcIdx] = vacancyType;
    // We don't update hash incrementally here; we recalculate after all changes

    // Place stone at target
    int8_t oldDstVal = sboard[dstIdx];
    sboard[dstIdx] = color;

    // Recalculate hash from scratch after the two changes
    // (safer than trying to track all XOR updates through captures)
    uint64_t newHash = hashBefore;
    UpdateHash(newHash, srcIdx, oldSrcVal, vacancyType);
    UpdateHash(newHash, dstIdx, oldDstVal, color);
    sim->hash = newHash;

    // Perform captures
    DoCapture(sim, color, false);
    newHash = sim->hash;

    // Check suicide on the cloned board
    bool suicide = false;
    {
        int32_t nn = gs->size;
        bool* visited = (bool*)calloc(nn * nn, sizeof(bool));
        if (visited) {
            // Check each section for the placed stone's group
            if (HasNoLibertiesInSection(sim, 0, tx, ty, tz, color, visited)) suicide = true;
            if (!suicide) {
                memset(visited, 0, nn * nn * sizeof(bool));
                if (HasNoLibertiesInSection(sim, 1, ty, tx, tz, color, visited)) suicide = true;
            }
            if (!suicide) {
                memset(visited, 0, nn * nn * sizeof(bool));
                if (HasNoLibertiesInSection(sim, 2, tz, tx, ty, color, visited)) suicide = true;
            }
            free(visited);
        }
    }

    if (suicide) {
        XinQi_Destroy(sim);
        return ERR_SUICIDE;
    }

    // Super KO check
    if (gs->moveCount >= 2 && newHash == gs->prevHash) {
        XinQi_Destroy(sim);
        return ERR_SUPERKO;
    }

    XinQi_Destroy(sim);

    // Apply the shift to the real board state
    int8_t* board = Board(gs);
    oldSrcVal = board[srcIdx];
    board[srcIdx] = vacancyType;
    oldDstVal = board[dstIdx];
    board[dstIdx] = color;

    // Update hash directly without DoCapture (captures haven't happened yet on real board)
    gs->prevHash = hashBefore;
    UpdateHash(gs->hash, srcIdx, oldSrcVal, vacancyType);
    UpdateHash(gs->hash, dstIdx, oldDstVal, color);

    // Perform captures on real board, track count
    int captured = DoCapture(gs, color, false);
    gs->lastCaptureCount = captured;

    gs->moveCount++;

    // Win conditions
    if (coreInvasion) {
        gs->current = Opp(color);
        return WIN_CORE_INVASION;
    }

    if (captured > 0 && !XinQi_HasAnyCore(gs, Opp(color))) {
        gs->current = Opp(color);
        return WIN_CLEAR_BOARD;
    }

    // No legal moves → current player wins
    if (!XinQi_HasAnyLegalMove(gs)) {
        return WIN_NO_LEGAL_MOVE;
    }

    gs->current = Opp(color);
    return RESULT_OK;
}

// ============================================================
// HasAnyLegalMove — scan for at least one legal move
// ============================================================

bool XinQi_HasAnyLegalMove(const GameState* gs) {
    if (!gs) return false;
    int8_t color = gs->current;
    int8_t n = gs->size;

    // Check if there's at least one empty cell (for placing)
    bool hasEmpty = false;
    bool hasCore = false;
    const int8_t* board = Board(gs);
    int32_t total = n * n * n;

    for (int32_t i = 0; i < total; ++i) {
        if (board[i] == CELL_EMPTY ||
            (color == COLOR_BLACK && board[i] == CELL_WHITE_VACANCY) ||
            (color == COLOR_WHITE && board[i] == CELL_BLACK_VACANCY)) {
            hasEmpty = true;
        }
        if (board[i] == color) {
            int8_t x = (int8_t)(i / (n * n));
            int8_t y = (int8_t)((i / n) % n);
            int8_t z = (int8_t)(i % n);
            if (XinQi_IsCore(gs, x, y, z)) {
                hasCore = true;
            }
        }
        if (hasEmpty && hasCore) break;
    }

    if (!hasEmpty && !hasCore) return false; // truly no moves

    // Try to find a legal Place
    if (hasEmpty) {
        for (int32_t i = 0; i < total; ++i) {
            int8_t cell = board[i];
            bool canPlace = (cell == CELL_EMPTY) ||
                (color == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
                (color == COLOR_WHITE && cell == CELL_BLACK_VACANCY);
            if (!canPlace) continue;

            int8_t x = (int8_t)(i / (n * n));
            int8_t y = (int8_t)((i / n) % n);
            int8_t z = (int8_t)(i % n);

            int8_t result = XinQi_CheckPlace(gs, x, y, z);
            if (result == RESULT_OK) return true;
        }
    }

    // Try to find a legal Shift
    if (hasCore) {
        for (int32_t i = 0; i < total; ++i) {
            if (board[i] != color) continue;
            int8_t x = (int8_t)(i / (n * n));
            int8_t y = (int8_t)((i / n) % n);
            int8_t z = (int8_t)(i % n);

            if (!XinQi_IsCore(gs, x, y, z)) continue;

            // Check all adjacent empty cells
            int dx[6] = { 1, -1, 0, 0, 0, 0 };
            int dy[6] = { 0, 0, 1, -1, 0, 0 };
            int dz[6] = { 0, 0, 0, 0, 1, -1 };

            for (int d = 0; d < 6; ++d) {
                int8_t nx = (int8_t)(x + dx[d]);
                int8_t ny = (int8_t)(y + dy[d]);
                int8_t nz = (int8_t)(z + dz[d]);
                if (IsOutOfBounds(gs, nx, ny, nz)) continue;
                if (XinQi_GetCell(gs, nx, ny, nz) != CELL_EMPTY) continue;

                int8_t result = XinQi_CheckShift(gs, x, y, z, nx, ny, nz);
                if (result == RESULT_OK) return true;
            }
        }
    }

    return false;
}
