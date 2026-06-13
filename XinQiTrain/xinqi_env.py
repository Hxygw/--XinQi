"""XinQi Training Environment — ctypes wrapper around XinQiTrain.dll

Usage:
    from xinqi_env import XinQiEnv
    env = XinQiEnv(board_size=5)
    board = env.get_board()          # (C, N, N, N) numpy array
    policy = env.mcts_policy(800)    # (N^3,) numpy array
    env.step(action_idx)             # apply move
"""

import ctypes
import numpy as np
from pathlib import Path
import os

# ── Load DLL ──
_dll_paths = [
    Path(__file__).parent / "XinQiTrain.dll",           # 同目录
    Path(__file__).parent / "x64" / "Release" / "XinQiTrain.dll",
    Path(__file__).parent / "x64" / "Debug" / "XinQiTrain.dll",
    Path.cwd() / "XinQiTrain.dll",
]
_dll = None
for p in _dll_paths:
    if p.exists():
        _dll = ctypes.CDLL(str(p))
        break
if _dll is None:
    raise RuntimeError(f"XinQiTrain.dll not found. Tried: {[str(p) for p in _dll_paths]}")

# ── Set up argument/return types ──
_dll.Train_Create.argtypes = [ctypes.c_int]
_dll.Train_Create.restype = ctypes.c_void_p

_dll.Train_Destroy.argtypes = [ctypes.c_void_p]
_dll.Train_Destroy.restype = None

_dll.Train_Clone.argtypes = [ctypes.c_void_p]
_dll.Train_Clone.restype = ctypes.c_void_p

_dll.Train_GetBoardSize.argtypes = [ctypes.c_void_p]
_dll.Train_GetBoardSize.restype = ctypes.c_int

_dll.Train_GetCurrentPlayer.argtypes = [ctypes.c_void_p]
_dll.Train_GetCurrentPlayer.restype = ctypes.c_int

_dll.Train_GetMoveCount.argtypes = [ctypes.c_void_p]
_dll.Train_GetMoveCount.restype = ctypes.c_int

_dll.Train_IsTerminal.argtypes = [ctypes.c_void_p]
_dll.Train_IsTerminal.restype = ctypes.c_int

_dll.Train_GetWinner.argtypes = [ctypes.c_void_p]
_dll.Train_GetWinner.restype = ctypes.c_int

_dll.Train_GetBoard.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int8), ctypes.c_int]
_dll.Train_GetBoard.restype = ctypes.c_int

_dll.Train_Place.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
_dll.Train_Place.restype = ctypes.c_int

_dll.Train_Shift.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                              ctypes.c_int, ctypes.c_int, ctypes.c_int]
_dll.Train_Shift.restype = ctypes.c_int

_dll.Train_GetLegalPlaces.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32), ctypes.c_int]
_dll.Train_GetLegalPlaces.restype = ctypes.c_int

_dll.Train_GetLegalShifts.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32), ctypes.c_int]
_dll.Train_GetLegalShifts.restype = ctypes.c_int

_dll.Train_HasLegalMove.argtypes = [ctypes.c_void_p]
_dll.Train_HasLegalMove.restype = ctypes.c_int

_dll.Train_MCTS_Policy.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int]
_dll.Train_MCTS_Policy.restype = ctypes.c_int

_dll.Train_SetAllowShift.argtypes = [ctypes.c_void_p, ctypes.c_int]
_dll.Train_SetAllowShift.restype = None
_dll.Train_GetAllowShift.argtypes = [ctypes.c_void_p]
_dll.Train_GetAllowShift.restype = ctypes.c_int
_dll.Train_SetAllowInvasionWin.argtypes = [ctypes.c_void_p, ctypes.c_int]
_dll.Train_SetAllowInvasionWin.restype = None
_dll.Train_GetAllowInvasionWin.argtypes = [ctypes.c_void_p]
_dll.Train_GetAllowInvasionWin.restype = ctypes.c_int


class XinQiEnv:
    """芯棋训练环境"""

    CELL_EMPTY = 0
    CELL_BLACK = 1
    CELL_WHITE = 2
    CELL_BLACK_VACANCY = 3
    CELL_WHITE_VACANCY = 4

    RESULT_OK = 0
    WIN_CLEAR_BOARD = 1
    WIN_CORE_INVASION = 2
    WIN_NO_LEGAL_MOVE = 3

    # 结果码: 0=成功, >=1=胜利, <0=错误
    RESULT_CODES = {
        0: "OK",
        1: "WIN_CLEAR_BOARD (清台)",
        2: "WIN_CORE_INVASION (侵入)",
        3: "WIN_NO_LEGAL_MOVE (无棋)",
        -1: "ERR_OCCUPIED",
        -2: "ERR_SUICIDE",
        -3: "ERR_SUPERKO",
        -4: "ERR_FIRST_MOVE_CENTER",
        -5: "ERR_NOT_CORE",
        -6: "ERR_NOT_ADJACENT",
        -7: "ERR_CORE_VACANCY",
    }

    def __init__(self, board_size: int = 5):
        self.size = board_size
        self._gs = _dll.Train_Create(board_size)
        if not self._gs:
            raise RuntimeError("Train_Create failed")
        self._total_cells = board_size ** 3

    def __del__(self):
        if hasattr(self, '_gs') and self._gs:
            _dll.Train_Destroy(self._gs)
            self._gs = None

    # ── Properties ──

    @property
    def current_player(self) -> int:
        return _dll.Train_GetCurrentPlayer(self._gs)

    @property
    def move_count(self) -> int:
        return _dll.Train_GetMoveCount(self._gs)

    @property
    def terminal(self) -> bool:
        return bool(_dll.Train_IsTerminal(self._gs))

    @property
    def winner(self) -> int:
        """0=no, 1=Black, 2=White"""
        return _dll.Train_GetWinner(self._gs)

    @property
    def has_legal_move(self) -> bool:
        return bool(_dll.Train_HasLegalMove(self._gs))

    # ── Board ──

    def get_board(self) -> np.ndarray:
        """Return raw board as int8 array shape (N^3,)"""
        buf = (ctypes.c_int8 * self._total_cells)()
        n = _dll.Train_GetBoard(self._gs, buf, self._total_cells)
        if n < 0:
            raise RuntimeError("GetBoard failed")
        return np.ctypeslib.as_array(buf).copy()

    def get_board_encoded(self) -> np.ndarray:
        """Return 3-channel board encoding suitable for CNN input.

        Shape: (3, N, N, N) — channel-first (torch conv3d default).

        Channels (from current player's perspective):
            0: own stones                    (1.0 or 0.0)
            1: opponent's stones             (1.0 or 0.0)
            2: own core vacancies            (1.0 or 0.0)

        Extension note:
            If training a network to predict Shift actions,
            add channel 3 = own cores (spots where shift is possible).
            This can be computed by calling XinQi_IsCore per cell.

        Normalization: raw binary 0/1. No further normalization needed.
        """
        N = self.size
        raw = self.get_board().reshape(N, N, N)
        current = self.current_player
        opp = 2 if current == 1 else 1

        own = (raw == current).astype(np.float32)
        opp_stones = (raw == opp).astype(np.float32)
        own_vac = (raw == (current + 2)).astype(np.float32)  # 1→3, 2→4

        return np.stack([own, opp_stones, own_vac], axis=0)

    # ── Legal moves ──

    def get_legal_places(self) -> np.ndarray:
        """Return flat indices of legal Place moves (shape: (n,))"""
        buf = (ctypes.c_int32 * self._total_cells)()
        n = _dll.Train_GetLegalPlaces(self._gs, buf, self._total_cells)
        return np.ctypeslib.as_array(buf)[:n].copy()

    def get_legal_mask(self) -> np.ndarray:
        """Return boolean mask of legal Place moves (shape: (N^3,))"""
        mask = np.zeros(self._total_cells, dtype=np.bool_)
        places = self.get_legal_places()
        mask[places] = True
        return mask

    # ── Moves ──

    def step(self, flat_idx: int) -> int:
        """Place at flat index. Returns result code."""
        N = self.size
        x = flat_idx // (N * N)
        y = (flat_idx // N) % N
        z = flat_idx % N
        return _dll.Train_Place(self._gs, x, y, z)

    def step_shift(self, src_idx: int, dir_idx: int) -> int:
        """Shift from src_idx in direction dir_idx (0..5: +x,-x,+y,-y,+z,-z).
        Returns result code."""
        N = self.size
        fx = src_idx // (N * N)
        fy = (src_idx // N) % N
        fz = src_idx % N

        dx = [1, -1, 0, 0, 0, 0]
        dy = [0, 0, 1, -1, 0, 0]
        dz = [0, 0, 0, 0, 1, -1]

        tx = fx + dx[dir_idx]
        ty = fy + dy[dir_idx]
        tz = fz + dz[dir_idx]
        return _dll.Train_Shift(self._gs, fx, fy, fz, tx, ty, tz)

    # ── MCTS ──

    def mcts_policy(self, simulations: int = 800) -> np.ndarray:
        """Run MCTS, return Place policy as float array (shape: (N^3,)).
        Entries sum to 1 over legal places only."""
        buf = (ctypes.c_float * self._total_cells)()
        r = _dll.Train_MCTS_Policy(self._gs, buf, self._total_cells, simulations)
        if r < 0:
            raise RuntimeError("MCTS_Policy failed")
        return np.ctypeslib.as_array(buf).copy()

    # ── Clone ──

    def clone(self) -> 'XinQiEnv':
        """Return a deep copy of the environment."""
        env = XinQiEnv.__new__(XinQiEnv)
        env.size = self.size
        env._total_cells = self._total_cells
        env._gs = _dll.Train_Clone(self._gs)
        if not env._gs:
            raise RuntimeError("Clone failed")
        return env

    # ── Runtime flags (for simplified training) ──

    def set_allow_shift(self, allow: bool) -> None:
        """Enable/disable Shift moves. Disabled by default for simplified training."""
        _dll.Train_SetAllowShift(self._gs, 1 if allow else 0)

    def get_allow_shift(self) -> bool:
        """Check whether Shift moves are currently enabled."""
        return bool(_dll.Train_GetAllowShift(self._gs))

    def set_allow_invasion_win(self, allow: bool) -> None:
        """Enable/disable Core Invasion victory condition.
        When disabled, placing on opponent's core vacancy is a normal move (no instant win)."""
        _dll.Train_SetAllowInvasionWin(self._gs, 1 if allow else 0)

    def get_allow_invasion_win(self) -> bool:
        """Check whether Core Invasion victory is currently enabled."""
        return bool(_dll.Train_GetAllowInvasionWin(self._gs))
