/**
 * 音效模块 — Web Audio API 合成音。
 *
 * 每次调用创建独立 AudioContext，在用户手势中触发无阻塞风险。
 */

/** 提子：C大调和弦 + 高频闪光 */
export function playCapture() {
  const ctx = new AudioContext();
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
  const ctx = new AudioContext();
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
  const ctx = new AudioContext();
  const t = ctx.currentTime;
  const osc = ctx.createOscillator();
  const g = ctx.createGain();
  osc.type = 'square';
  osc.frequency.setValueAtTime(440, t);
  osc.frequency.exponentialRampToValueAtTime(220, t + 0.2);
  g.gain.setValueAtTime(0.2, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + 0.25);
  osc.connect(g);
  g.connect(ctx.destination);
  osc.start(t);
  osc.stop(t + 0.25);
}
