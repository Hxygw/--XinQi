/**
 * 音效模块 — Web Audio API 合成音。
 *
 * 共享一个惰性 AudioContext，防止快速连续点击导致浏览器限制。
 */

let _ctx: AudioContext | null = null;
function getCtx(): AudioContext {
  if (!_ctx) _ctx = new AudioContext();
  if (_ctx.state === 'suspended') _ctx.resume();
  return _ctx;
}

/** 提子：C大调和弦 + 高频闪光 */
export function playCapture() {
  const ctx = getCtx();
  const t = ctx.currentTime;

  // 核心：C大调和弦（523/659/784Hz）
  [523.25, 659.25, 783.99].forEach((f, i) => {
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.value = f;
    g.gain.setValueAtTime([0.25, 0.2, 0.15][i], t);
    g.gain.exponentialRampToValueAtTime(0.001, t + [0.35, 0.3, 0.25][i]);
    osc.connect(g);
    g.connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.4);
  });

  // 爽感点睛：快速上升的高频闪光
  const sp = ctx.createOscillator();
  const spg = ctx.createGain();
  sp.type = 'triangle';
  sp.frequency.setValueAtTime(1568, t);
  sp.frequency.exponentialRampToValueAtTime(3136, t + 0.08);
  spg.gain.setValueAtTime(0.12, t);
  spg.gain.exponentialRampToValueAtTime(0.001, t + 0.12);
  sp.connect(spg);
  spg.connect(ctx.destination);
  sp.start(t);
  sp.stop(t + 0.12);
}

/** 落子：短促清脆，像晶体轻叩 */
export function playPlace() {
  const ctx = getCtx();
  const t = ctx.currentTime;
  const osc = ctx.createOscillator();
  const g = ctx.createGain();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(880, t);
  g.gain.setValueAtTime(0.2, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + 0.08);
  osc.connect(g);
  g.connect(ctx.destination);
  osc.start(t);
  osc.stop(t + 0.08);
}

/** 挪子：机械感，稍沉，暗示结构移动 */
export function playShift() {
  const ctx = getCtx();
  const t = ctx.currentTime;
  const osc = ctx.createOscillator();
  const g = ctx.createGain();
  osc.type = 'triangle';
  osc.frequency.setValueAtTime(440, t);
  osc.frequency.exponentialRampToValueAtTime(220, t + 0.15);
  g.gain.setValueAtTime(0.15, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + 0.15);
  osc.connect(g);
  g.connect(ctx.destination);
  osc.start(t);
  osc.stop(t + 0.15);
}

/** 侵入获胜：精准刺穿，像晶体裂开 */
export function playVictoryIntrusion() {
  const ctx = getCtx();
  const t = ctx.currentTime;

  // 高频琶音 C5-E5-G5-B5
  [523.25, 659.25, 783.99, 987.77].forEach((f, i) => {
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.value = f;
    g.gain.setValueAtTime(0, t + i * 0.08);
    g.gain.linearRampToValueAtTime(0.2, t + i * 0.08 + 0.05);
    g.gain.exponentialRampToValueAtTime(0.001, t + i * 0.08 + 0.6);
    osc.connect(g);
    g.connect(ctx.destination);
    osc.start(t + i * 0.08);
    osc.stop(t + i * 0.08 + 0.7);
  });

  // 尾音 C6 闪光
  const flash = ctx.createOscillator();
  const flashG = ctx.createGain();
  flash.type = 'sine';
  flash.frequency.setValueAtTime(2093, t + 0.35);
  flashG.gain.setValueAtTime(0, t + 0.35);
  flashG.gain.linearRampToValueAtTime(0.15, t + 0.4);
  flashG.gain.exponentialRampToValueAtTime(0.001, t + 0.9);
  flash.connect(flashG);
  flashG.connect(ctx.destination);
  flash.start(t + 0.35);
  flash.stop(t + 0.9);
}

/** 清台/无棋获胜：全局碾压，虚空崩塌 */
export function playVictoryAnnihilation() {
  const ctx = getCtx();
  const t = ctx.currentTime;

  // 低音和弦 C3-E3-G3-C4 — 像巨钟齐鸣
  [130.81, 164.81, 196.00, 261.63].forEach((f, i) => {
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.value = f;
    g.gain.setValueAtTime(0, t + i * 0.2);
    g.gain.linearRampToValueAtTime(0.4, t + i * 0.2 + 0.8);
    g.gain.exponentialRampToValueAtTime(0.001, t + i * 0.2 + 2.5);
    osc.connect(g);
    g.connect(ctx.destination);
    osc.start(t + i * 0.2);
    osc.stop(t + i * 0.2 + 2.5);
  });

  // Sub bass C1→C2 下沉
  const sub = ctx.createOscillator();
  const subG = ctx.createGain();
  sub.type = 'triangle';
  sub.frequency.setValueAtTime(65.41, t);
  sub.frequency.exponentialRampToValueAtTime(32.70, t + 2);
  subG.gain.setValueAtTime(0, t);
  subG.gain.linearRampToValueAtTime(0.5, t + 0.5);
  subG.gain.exponentialRampToValueAtTime(0.001, t + 2.5);
  sub.connect(subG);
  subG.connect(ctx.destination);
  sub.start(t);
  sub.stop(t + 2.5);
}
