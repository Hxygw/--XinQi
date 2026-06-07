<script lang="ts">
  /**
   * Three.js 3D 棋盘 — 芯棋。
   *
   * 核心交互：
   * - 拖拽旋转棋盘（水平绕自身 Y 轴，垂直绕世界 Z 轴）
   * - 自动旋转（前倾 35.26°，绕自身 Y 轴，体对角线为旋转轴）
   * - 拖拽惯性衰减 + 俯仰回正
   * - 滚轮缩放
   * - 剖面视图、悬停高亮、挪子模式等
   */

  import { onMount, onDestroy } from "svelte";
  import * as THREE from "three";
  import { to3D, to1D } from "../lib/boardUtils";
  import { COLORS } from "../lib/colors";

  interface Props {
    board: Uint8Array;
    N: number;
    sectionAxis: string | null;
    sectionPos: number;
    /** 内芯索引集合（双方） */
    innerCoreSet: Set<number>;
    /** 内芯空位索引集合（按所有者分色） */
    vacancyBlack: Set<number>;
    vacancyWhite: Set<number>;
    /** 是否处于挪子模式 */
    moveMode: boolean;
    /** 选中的挪子源索引 */
    moveSourceIdx: number;
    /** 挪子源连通块索引集合 */
    moveBlockIndices: Set<number>;
    /** 挂载结果：可挪动到的目标索引列表 */
    validTargets: number[];
    hoverResult?: { idx: number; legal: boolean; fatalAxes?: number[]; isInnerCore?: boolean } | null;
    validTargetHover?: { idx: number; legal: boolean } | null;
    browseTick?: number;
    /** 是否启用自动旋转 */
    autoRotate?: boolean;
    /** 是否启用交互检测（默认 true） */
    raycastEnabled?: boolean;
    onhover?: (pt: { x: number; y: number; z: number }) => void;
    onleave?: () => void;
    oncellclick?: (pt: { x: number; y: number; z: number }) => void;
    oncellrightclick?: () => void;
    onSectionChange?: (axis: string | null, pos: number) => void;
  }

  let {
    board, N,
    sectionAxis, sectionPos,
    innerCoreSet, vacancyBlack, vacancyWhite,
    moveMode, moveSourceIdx, moveBlockIndices, validTargets,
    hoverResult = null, validTargetHover = null,
    browseTick = 0, autoRotate = false, raycastEnabled = true,
    onhover, onleave, oncellclick, oncellrightclick,
    onSectionChange,
  }: Props = $props();

  let container: HTMLDivElement;
  let scene: THREE.Scene;
  let camera: THREE.PerspectiveCamera;
  let renderer: THREE.WebGLRenderer;
  let resizeObserver: ResizeObserver | null = null;
  let boardPivot: THREE.Group;
  // 拖拽旋转状态
  let isDragging = false;
  let dragStartX = 0, dragStartY = 0;
  // 偏航角（绕自身 Y 轴）和俯仰角（绕世界 Z 轴），单位弧度
  let tiltYaw = 0, tiltPitch = 0, dragVel = 0;
  let snapTimer: ReturnType<typeof setTimeout> | undefined;
  let snappingTilt = false;      // 俯仰正在回正
  let snappingToCamera = false;  // 开始游戏时旋转到目标姿态
  let targetQuat = new THREE.Quaternion();
  let zoomLevel = 12;
  const DEFAULT_PITCH = -0.615; // 默认前倾35.26°（体对角线角）
  let raycaster = new THREE.Raycaster();
  let pointer = new THREE.Vector2();

  let gridLineGroup = new THREE.Group();
  let dotGroup = new THREE.Group();
  let stoneGroup = new THREE.Group();
  let innerCoreGlowGroup = new THREE.Group();
  let highlightGroup = new THREE.Group();
  let vacancyGroup = new THREE.Group();
  let moveBlockGroup = new THREE.Group();
  let validTargetGroup = new THREE.Group();
  let sectionPlaneGroup = new THREE.Group();

  let gridLineMeta: { axis: string; fixed1: number; fixed2: number; line: THREE.Line; baseOpacity: number }[] = [];
  let intersectPoints: THREE.Vector3[] = [];
  let interactSpheres: THREE.Mesh[] = [];
  let hoveredIdx = -1;

  let initialized = $state(false);
  let prevN = -1;

  // 响应式更新
  $effect(() => {
    const _b = board;
    const _t = browseTick;
    const _ax = sectionAxis;
    const _pos = sectionPos;
    const _n = N;
    if (!initialized) return;
    invalidateSphereCache();
    if (_n !== prevN) {
      prevN = _n;
      const c = (N - 1) / 2;
      boardPivot.position.set(c, c, c);
      boardPivot.children[0]?.position.set(-c, -c, -c);
      // 相机跟随棋盘中心
      camera.position.set(c + zoomLevel, c, c);
      camera.lookAt(c, c, c);
      disposeGroup(gridLineGroup);
      disposeGroup(dotGroup);
      for (const s of interactSpheres) {
        boardPivot.children[0]?.remove(s);
        s.geometry.dispose();
        (s.material as THREE.Material).dispose();
      }
      interactSpheres = [];
      buildGrid();
      buildInteractionSpheres();
    }
    updateStones();
    updateInnerCoreGlow();
    updateVacancies();
    updateGridOpacity();
    updateSectionPlane();
    renderFrame();
  });

  $effect(() => {
    const _ar = autoRotate;
    if (!initialized) return;
    if (_ar) {
      tiltPitch = DEFAULT_PITCH;
      tiltYaw = 0;
      snappingToCamera = false;
      keepLoopAlive();
    } else {
      // 开始游戏：平滑停止旋转
      if (autoRotate) {
        // 从自转过渡中（本分支不会同时触发）
      }
      dragVel = 0.0025; // 用阻尼平滑衰减到 0
      snappingToCamera = false;
      keepLoopAlive();
    }
  });

  $effect(() => {
    const _ic = innerCoreSet;
    const _vb = vacancyBlack;
    const _vw = vacancyWhite;
    if (!initialized) return;
    updateInnerCoreGlow();
    updateVacancies();
    renderFrame();
  });

  $effect(() => {
    const _h = hoverResult;
    if (!initialized) return;
    updateHighlights();
    renderFrame();
  });

  $effect(() => {
    const _mm = moveMode;
    const _ms = moveSourceIdx;
    const _mb = moveBlockIndices;
    const _vt = validTargets;
    const _vh = validTargetHover;
    if (!initialized) return;
    updateMoveBlock();
    updateValidTargets();
    renderFrame();
  });

  const LINE_COLOR = 0x7c6df0;
  const DOT_COLOR = 0x6c5ce7;
  const STONE_RADIUS = 0.29;
  const STONE_SEGMENTS = 20;

  // ── 初始化 ─────────────────────────────────────────

  onMount(() => {
    const rect = container.getBoundingClientRect();
    const w = rect.width || 800;
    const h = rect.height || 600;

    scene = new THREE.Scene();
    scene.background = new THREE.Color(0xf5f3ff);

    camera = new THREE.PerspectiveCamera(40, w / h, 0.1, 100);
    const center = (N - 1) / 2;
    zoomLevel = 12;
    // 相机在 X 轴上，正对棋盘中心，高度与棋盘平齐
    camera.position.set(center + zoomLevel, center, center);
    camera.lookAt(center, center, center);

    renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(w, h);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    container.appendChild(renderer.domElement);

    // 棋盘容器：pivot 在棋盘中心，内容偏移使旋转中心居中
    // 旋转逻辑：先绕世界 Z 轴俯仰（tiltPitch），再绕自身 Y 轴偏航（tiltYaw）
    const c = (N - 1) / 2;
    boardPivot = new THREE.Group();
    boardPivot.rotation.order = 'YXZ';
    boardPivot.position.set(c, c, c);
    scene.add(boardPivot);

    const boardContent = new THREE.Group();
    boardContent.position.set(-c, -c, -c);
    boardPivot.add(boardContent);

    // 浅色背景不需要强光源，仅作环境照明
    const ambient = new THREE.AmbientLight(0xffffff, 1.0);
    scene.add(ambient);

    boardContent.add(gridLineGroup);
    boardContent.add(dotGroup);
    boardContent.add(stoneGroup);
    boardContent.add(innerCoreGlowGroup);
    boardContent.add(vacancyGroup);
    boardContent.add(moveBlockGroup);
    boardContent.add(validTargetGroup);
    boardContent.add(highlightGroup);
    boardContent.add(sectionPlaneGroup);

    buildGrid();
    buildInteractionSpheres();

    renderer.domElement.addEventListener("pointermove", onPointerMove);
    renderer.domElement.addEventListener("dblclick", onDblClick);
    renderer.domElement.addEventListener("pointerleave", onPointerLeave);
    renderer.domElement.addEventListener("contextmenu", (e) => {
      e.preventDefault();
      oncellrightclick?.();
    });
    // ── 棋盘拖拽旋转（绕自身 Y 轴 + 世界 Z 轴） ──
    renderer.domElement.addEventListener("pointerdown", (e) => {
      isDragging = true;
      dragStartX = e.clientX;
      dragStartY = e.clientY;
      dragVel = 0;            // 重置惯性，拖拽从零开始
      clearTimeout(snapTimer); // 取消俯仰回正定时器
      (e.target as HTMLElement).setPointerCapture(e.pointerId);
      keepLoopAlive();
    });
    renderer.domElement.addEventListener("pointermove", (e) => {
      if (!isDragging) return;
      const dx = e.clientX - dragStartX;
      const dy = e.clientY - dragStartY;
      dragStartX = e.clientX;
      dragStartY = e.clientY;
      // 水平拖拽 → 偏航角（绕自身 Y 轴），垂直拖拽 → 俯仰角（绕世界 Z 轴）
      dragVel = dx * 0.005;
      tiltPitch = Math.max(-1.3, Math.min(1.3, tiltPitch - dy * 0.005));
      tiltYaw += dragVel;
      applyTilt();
    });
    renderer.domElement.addEventListener("pointerup", (e) => {
      isDragging = false;
      (e.target as HTMLElement).releasePointerCapture(e.pointerId);
      if (autoRotate) startSnapTimer();
    });
    renderer.domElement.addEventListener("pointercancel", () => {
      isDragging = false;
      if (autoRotate) startSnapTimer();
    });
    // 滚轮缩放
    renderer.domElement.addEventListener("wheel", (e) => {
      e.preventDefault();
      const center = (N - 1) / 2;
      zoomLevel = Math.max(3, Math.min(20, zoomLevel + e.deltaY * 0.01));
      const dir = new THREE.Vector3(1, 0, 0);
      camera.position.copy(dir.multiplyScalar(zoomLevel)).add(new THREE.Vector3(center, center, center));
      camera.lookAt(center, center, center);
      keepLoopAlive();
    }, { passive: false });
    document.addEventListener("keydown", onKeyDown);

    // 响应窗口大小变化
    const resizeObs = new ResizeObserver(() => {
      const rect = container.getBoundingClientRect();
      if (rect.width === 0 || rect.height === 0) return;
      camera.aspect = rect.width / rect.height;
      camera.updateProjectionMatrix();
      renderer.setSize(rect.width, rect.height);
      renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
      renderFrame();
    });
    resizeObs.observe(container);
    resizeObserver = resizeObs;

    initialized = true;
    updateStones();
    renderFrame();
  });

  onDestroy(() => {
    cancelAnimationFrame(_rafId);
    document.removeEventListener("keydown", onKeyDown);
    renderer?.dispose();
    resizeObserver?.disconnect();
  });

  // ── 构建网格 ───────────────────────────────────────

  function buildGrid() {
    disposeGroup(gridLineGroup);
    disposeGroup(dotGroup);
    intersectPoints = [];
    gridLineMeta = [];

    const lineMat = new THREE.LineBasicMaterial({
      color: LINE_COLOR,
      transparent: true,
      opacity: 0.35,
    });
    const dotMat = new THREE.MeshBasicMaterial({
      color: DOT_COLOR,
      transparent: true,
      opacity: 0.5,
    });
    const dotGeo = new THREE.SphereGeometry(0.04, 6, 6);
    const borderMat = new THREE.LineBasicMaterial({
      color: LINE_COLOR,
      transparent: true,
      opacity: 0.6,
    });

    const maxIdx = N - 1;

    for (let a = 0; a < N; a++) {
      for (let b = 0; b < N; b++) {
        const isBorder = a === 0 || a === maxIdx || b === 0 || b === maxIdx;

        const ptsX: THREE.Vector3[] = [];
        for (let x = 0; x < N; x++) ptsX.push(new THREE.Vector3(x, a, b));
        addLine(ptsX, isBorder ? borderMat : lineMat, 'x', a, b);

        const ptsY: THREE.Vector3[] = [];
        for (let y = 0; y < N; y++) ptsY.push(new THREE.Vector3(a, y, b));
        addLine(ptsY, isBorder ? borderMat : lineMat, 'y', a, b);

        const ptsZ: THREE.Vector3[] = [];
        for (let z = 0; z < N; z++) ptsZ.push(new THREE.Vector3(a, b, z));
        addLine(ptsZ, isBorder ? borderMat : lineMat, 'z', a, b);
      }
    }

    for (let x = 0; x < N; x++) {
      for (let y = 0; y < N; y++) {
        for (let z = 0; z < N; z++) {
          const dot = new THREE.Mesh(dotGeo, dotMat.clone());
          dot.position.set(x, y, z);
          dotGroup.add(dot);
          intersectPoints.push(new THREE.Vector3(x, y, z));
        }
      }
    }
  }

  function addLine(points: THREE.Vector3[], mat: THREE.LineBasicMaterial,
                   axis: string, fixed1: number, fixed2: number) {
    const geo = new THREE.BufferGeometry().setFromPoints(points);
    const line = new THREE.Line(geo, mat.clone());
    gridLineGroup.add(line);
    gridLineMeta.push({ axis, fixed1, fixed2, line, baseOpacity: mat.opacity });
  }

  function buildInteractionSpheres() {
    const geo = new THREE.SphereGeometry(STONE_RADIUS, 8, 8);
    const mat = new THREE.MeshBasicMaterial({
      transparent: true,
      opacity: 0,
      depthWrite: false,
    });
    const total = N * N * N;
    for (let i = 0; i < total; i++) {
      const pt = to3D(i, N);
      const mesh = new THREE.Mesh(geo, mat);
      mesh.position.set(pt.x, pt.y, pt.z);
      mesh.userData.idx = i;
      boardPivot.children[0]?.add(mesh);
      interactSpheres.push(mesh);
    }
  }

  // ── 剖面判定 ───────────────────────────────────────

  function isInSlice(x: number, y: number, z: number): boolean {
    if (!sectionAxis) return true;
    if (sectionAxis === 'x') return x === sectionPos;
    if (sectionAxis === 'y') return y === sectionPos;
    if (sectionAxis === 'z') return z === sectionPos;
    return true;
  }

  function updateGridOpacity() {
    for (const meta of gridLineMeta) {
      let onSection = true;
      if (sectionAxis) {
        if (meta.axis === sectionAxis) {
          onSection = false;
        } else if (sectionAxis === 'x') {
          onSection = meta.fixed1 === sectionPos;
        } else if (sectionAxis === 'y') {
          onSection = meta.axis === 'x' ? meta.fixed1 === sectionPos : meta.fixed2 === sectionPos;
        } else {
          onSection = meta.fixed2 === sectionPos;
        }
      }
      meta.line.material.opacity = onSection ? meta.baseOpacity : 0.06;
    }
    for (let i = 0; i < dotGroup.children.length; i++) {
      const pt = to3D(i, N);
      const dot = dotGroup.children[i] as THREE.Mesh;
      if (dot.isMesh) {
        (dot.material as THREE.MeshBasicMaterial).opacity = isInSlice(pt.x, pt.y, pt.z) ? 0.5 : 0.06;
      }
    }
  }

  // ── 更新棋子 ───────────────────────────────────────

  function updateStones() {
    disposeGroup(stoneGroup);

    const total = N * N * N;
    for (let i = 0; i < total; i++) {
      const cell = board[i];
      if (cell === 0 || cell >= 3) continue;

      const pt = to3D(i, N);
      const inSlice = isInSlice(pt.x, pt.y, pt.z);
      const isBlack = cell === 1;
      // 亮蓝（先手）vs 亮暖橘（后手）
      const color = isBlack ? COLORS.stonePrimary : COLORS.stoneSecondary;

      // 底层极淡轮廓，区分相邻棋子
      const outlineMat = new THREE.MeshBasicMaterial({
        color: 0x1a1a2e,
        transparent: true,
        opacity: 0.15,
        depthWrite: false,
      });
      const outline = new THREE.Mesh(
        new THREE.SphereGeometry(STONE_RADIUS * 1.10, STONE_SEGMENTS, STONE_SEGMENTS),
        outlineMat
      );
      outline.position.set(pt.x, pt.y, pt.z);
      stoneGroup.add(outline);

      // 棋子本体
      const mat = new THREE.MeshBasicMaterial({
        color,
        transparent: sectionAxis !== null,
        opacity: inSlice ? 1.0 : 0.30,
        depthWrite: inSlice,
      });
      const mesh = new THREE.Mesh(
        new THREE.SphereGeometry(STONE_RADIUS, STONE_SEGMENTS, STONE_SEGMENTS),
        mat
      );
      mesh.position.set(pt.x, pt.y, pt.z);
      stoneGroup.add(mesh);
    }
  }

  // ── 内芯光晕 ───────────────────────────────────────

  function updateInnerCoreGlow() {
    disposeGroup(innerCoreGlowGroup);

    for (const idx of innerCoreSet) {
      const pt = to3D(idx, N);
      const inSlice = isInSlice(pt.x, pt.y, pt.z);
      if (sectionAxis && !inSlice) continue;

      // 淡蓝色半透明光晕
      const glowGeo = new THREE.SphereGeometry(STONE_RADIUS * 1.15, 16, 16);
      const glowMat = new THREE.MeshBasicMaterial({
        color: 0x7c6df0,
        transparent: true,
        opacity: 0.20,
        depthWrite: false,
      });
      const glow = new THREE.Mesh(glowGeo, glowMat);
      glow.position.set(pt.x, pt.y, pt.z);
      innerCoreGlowGroup.add(glow);
    }
  }

  // ── 内芯空位（全局透视，不受剖面影响） ─────────────

  function updateVacancies() {
    disposeGroup(vacancyGroup);

    const renderVacancy = (idx: number, color: number) => {
      const pt = to3D(idx, N);
      const boxGeo = new THREE.BoxGeometry(0.25, 0.25, 0.25);
      const boxMat = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.35,
        wireframe: true,
        depthWrite: false,
      });
      const box = new THREE.Mesh(boxGeo, boxMat);
      box.position.set(pt.x, pt.y, pt.z);
      vacancyGroup.add(box);
    };

    for (const idx of vacancyBlack) renderVacancy(idx, 0x4466aa);
    for (const idx of vacancyWhite) renderVacancy(idx, 0xaa6644);
  }

  // ── 挪子模式：连通块高亮 ───────────────────────────

  function updateMoveBlock() {
    disposeGroup(moveBlockGroup);

    if (!moveMode || moveSourceIdx < 0) return;

    // 连通块外发光轮廓
    for (const idx of moveBlockIndices) {
      const pt = to3D(idx, N);
      const inSlice = isInSlice(pt.x, pt.y, pt.z);
      if (sectionAxis && !inSlice) continue;

      const color = 0xfaf0d8;

      const glowGeo = new THREE.SphereGeometry(STONE_RADIUS * 1.25, 16, 16);
      const glowMat = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.40,
        depthWrite: false,
      });
      const glow = new THREE.Mesh(glowGeo, glowMat);
      glow.position.set(pt.x, pt.y, pt.z);
      moveBlockGroup.add(glow);
    }
  }

  // ── 挪子模式：合法目标标记（球体，不受剖面影响） ─

  function updateValidTargets() {
    disposeGroup(validTargetGroup);

    if (!moveMode) return;

    for (const idx of validTargets) {
      const pt = to3D(idx, N);

      const sphereGeo = new THREE.SphereGeometry(STONE_RADIUS * 0.7, 12, 12);
      const sphereMat = new THREE.MeshBasicMaterial({
        color: 0x4cd964,
        transparent: true,
        opacity: 0.30,
        depthWrite: false,
      });
      const sphere = new THREE.Mesh(sphereGeo, sphereMat);
      sphere.position.set(pt.x, pt.y, pt.z);
      validTargetGroup.add(sphere);
    }

    // 悬停合法目标时高亮
    if (validTargetHover && validTargetHover.legal) {
      const pt = to3D(validTargetHover.idx, N);
      const dotGeo = new THREE.SphereGeometry(STONE_RADIUS * 0.8, 12, 12);
      const dotMat = new THREE.MeshBasicMaterial({
        color: 0x4cd964,
        transparent: true,
        opacity: 0.50,
        depthWrite: false,
      });
      const dot = new THREE.Mesh(dotGeo, dotMat);
      dot.position.set(pt.x, pt.y, pt.z);
      validTargetGroup.add(dot);
    }
  }

  // ── 悬停高亮 ───────────────────────────────────────

  function updateHighlights() {
    disposeGroup(highlightGroup);
    if (!hoverResult || hoverResult.idx < 0) return;

    const { idx, legal, fatalAxes, isInnerCore } = hoverResult;
    const pt = to3D(idx, N);
    const inSlice = isInSlice(pt.x, pt.y, pt.z);
    if (!inSlice) return;

    let glowColor: number;
    if (isInnerCore) {
      glowColor = 0x7c6df0; // 内芯紫色
    } else {
      glowColor = legal ? 0x7c6df0 : 0xff3344;
    }

    // 呼吸光环 — 有棋子的位置大一圈，空格与棋子同大
    const hasStone = board[idx] >= 1 && board[idx] <= 2;
    const glowSize = hasStone ? STONE_RADIUS * 1.3 : STONE_RADIUS;
    const glowGeo = new THREE.SphereGeometry(glowSize, 16, 16);
    const glowMat = new THREE.MeshBasicMaterial({
      color: glowColor,
      transparent: true,
      opacity: 0.25,
    });
    const glow = new THREE.Mesh(glowGeo, glowMat);
    glow.position.set(pt.x, pt.y, pt.z);
    highlightGroup.add(glow);

    // 剖面模式下用 2D 环替代
    if (sectionAxis) {
      const ringGeo = new THREE.RingGeometry(STONE_RADIUS * 0.7, STONE_RADIUS, 24);
      const ringMat = new THREE.MeshBasicMaterial({
        color: glowColor,
        transparent: true,
        opacity: 0.4,
        side: THREE.DoubleSide,
        depthWrite: false,
      });
      const ring = new THREE.Mesh(ringGeo, ringMat);
      ring.position.set(pt.x, pt.y, pt.z);
      switch (sectionAxis) {
        case 'x': ring.rotation.set(0, Math.PI / 2, 0); break;
        case 'y': ring.rotation.set(-Math.PI / 2, 0, 0); break;
        case 'z': ring.rotation.set(0, 0, 0); break;
      }
      highlightGroup.add(ring);
    }

    // 自杀：高亮致命截面
    if (!legal && fatalAxes) {
      for (const axis of fatalAxes) {
        const planeGroup = buildSectionPlane(axis, idx, N);
        highlightGroup.add(planeGroup);
      }
    }
  }

  function buildSectionPlane(axis: number, idx: number, N: number): THREE.Group {
    const group = new THREE.Group();
    const pt = to3D(idx, N);
    const color = 0xff3344;

    let fixed: number;
    if (axis === 0) fixed = pt.x;
    else if (axis === 1) fixed = pt.y;
    else fixed = pt.z;

    const lineMat = new THREE.LineBasicMaterial({
      color, transparent: true, opacity: 0.5,
    });

    for (let a = 0; a < N; a++) {
      const pts1: THREE.Vector3[] = [];
      for (let b = 0; b < N; b++) {
        const p: number[] = [];
        if (axis === 0) { p[0] = fixed; p[1] = a; p[2] = b; }
        else if (axis === 1) { p[0] = a; p[1] = fixed; p[2] = b; }
        else { p[0] = a; p[1] = b; p[2] = fixed; }
        pts1.push(new THREE.Vector3(p[0], p[1], p[2]));
      }
      const geo1 = new THREE.BufferGeometry().setFromPoints(pts1);
      group.add(new THREE.Line(geo1, lineMat));

      const pts2: THREE.Vector3[] = [];
      for (let b = 0; b < N; b++) {
        const p: number[] = [];
        if (axis === 0) { p[0] = fixed; p[1] = b; p[2] = a; }
        else if (axis === 1) { p[0] = b; p[1] = fixed; p[2] = a; }
        else { p[0] = b; p[1] = a; p[2] = fixed; }
        pts2.push(new THREE.Vector3(p[0], p[1], p[2]));
      }
      const geo2 = new THREE.BufferGeometry().setFromPoints(pts2);
      group.add(new THREE.Line(geo2, lineMat));
    }

    const planeGeo = new THREE.PlaneGeometry(N + 1, N + 1);
    const maxIdx = N - 1;
    let pos = new THREE.Vector3();
    let rot = new THREE.Euler();
    if (axis === 0) {
      pos.set(fixed, maxIdx / 2, maxIdx / 2);
      rot.set(0, Math.PI / 2, 0);
    } else if (axis === 1) {
      pos.set(maxIdx / 2, fixed, maxIdx / 2);
      rot.set(-Math.PI / 2, 0, 0);
    } else {
      pos.set(maxIdx / 2, maxIdx / 2, fixed);
    }
    const plane = new THREE.Mesh(planeGeo, new THREE.MeshBasicMaterial({
      color, transparent: true, opacity: 0.08,
      side: THREE.DoubleSide, depthWrite: false,
    }));
    plane.position.copy(pos);
    plane.rotation.copy(rot);
    group.add(plane);

    return group;
  }

  // ── 剖面平面指示器 ────────────────────────────────

  function updateSectionPlane() {
    disposeGroup(sectionPlaneGroup);
    if (!sectionAxis) return;

    const planeGeo = new THREE.PlaneGeometry(N, N);
    const planeMat = new THREE.MeshBasicMaterial({
      color: 0x7c6df0,
      transparent: true,
      opacity: 0.06,
      side: THREE.DoubleSide,
      depthWrite: false,
    });
    const mesh = new THREE.Mesh(planeGeo, planeMat);
    const half = (N - 1) / 2;

    switch (sectionAxis) {
      case 'x':
        mesh.position.set(sectionPos, half, half);
        mesh.rotation.set(0, Math.PI / 2, 0);
        break;
      case 'y':
        mesh.position.set(half, sectionPos, half);
        mesh.rotation.set(-Math.PI / 2, 0, 0);
        break;
      case 'z':
        mesh.position.set(half, half, sectionPos);
        break;
    }
    sectionPlaneGroup.add(mesh);

    const edgeGeo = new THREE.EdgesGeometry(planeGeo);
    const edgeMat = new THREE.LineBasicMaterial({
      color: 0x7c6df0,
      transparent: true,
      opacity: 0.2,
    });
    const edges = new THREE.LineSegments(edgeGeo, edgeMat);
    edges.position.copy(mesh.position);
    edges.rotation.copy(mesh.rotation);
    sectionPlaneGroup.add(edges);
  }

  // ── 鼠标交互 ───────────────────────────────────────

  let _cacheDirty = true;
  let _cachedActiveSpheres: THREE.Mesh[] = [];

  function getActiveSpheres(): THREE.Mesh[] {
    if (!_cacheDirty && !sectionAxis) return interactSpheres;
    _cacheDirty = false;
    if (!sectionAxis) return interactSpheres;
    const active: THREE.Mesh[] = [];
    for (let i = 0; i < interactSpheres.length; i++) {
      const pt = to3D(i, N);
      if (isInSlice(pt.x, pt.y, pt.z)) {
        active.push(interactSpheres[i]);
      }
    }
    _cachedActiveSpheres = active;
    return active;
  }

  function invalidateSphereCache() { _cacheDirty = true; }

  let _lastMoveTime = 0;

  function getIntersectIdx(clientX: number, clientY: number): number {
    if (!raycastEnabled) return -1;
    const activeSpheres = getActiveSpheres();
    if (activeSpheres.length === 0) return -1;

    const rect = renderer.domElement.getBoundingClientRect();
    pointer.x = ((clientX - rect.left) / rect.width) * 2 - 1;
    pointer.y = -((clientY - rect.top) / rect.height) * 2 + 1;

    raycaster.setFromCamera(pointer, camera);
    const intersects = raycaster.intersectObjects(activeSpheres, false);
    if (intersects.length > 0) {
      return intersects[0].object.userData.idx as number;
    }
    return -1;
  }

  function onPointerMove(event: PointerEvent) {
    keepLoopAlive();
    const now = performance.now();
    if (now - _lastMoveTime < 33) return;
    _lastMoveTime = now;

    const idx = getIntersectIdx(event.clientX, event.clientY);
    if (idx !== hoveredIdx) {
      hoveredIdx = idx;
      renderer.domElement.style.cursor = idx >= 0 ? "pointer" : "default";
      if (idx >= 0) {
        onhover?.(to3D(idx, N));
      } else {
        onleave?.();
      }
    }
  }

  function onDblClick(event: PointerEvent) {
    if (!raycastEnabled) return;
    const idx = getIntersectIdx(event.clientX, event.clientY);
    if (idx >= 0) {
      oncellclick?.(to3D(idx, N));
    } else {
      // 双击空白处 → 退出挪子模式
      oncellrightclick?.();
    }
  }

  function onPointerLeave() {
    hoveredIdx = -1;
    renderer.domElement.style.cursor = "default";
    onleave?.();
    renderer.domElement.dispatchEvent(new PointerEvent('pointerup', {
      bubbles: true, button: 0, pointerId: 0
    }));
    renderFrame();
  }

  // ── 键盘快捷键 ────────────────────────────────────

  function onKeyDown(event: KeyboardEvent) {
    const key = event.key;
    if (key === '`' || key === 'Escape') {
      onSectionChange?.(null, 0);
      event.preventDefault();
    } else if (key === '1') {
      onSectionChange?.('x', Math.floor((N - 1) / 2));
      event.preventDefault();
    } else if (key === '2') {
      onSectionChange?.('y', Math.floor((N - 1) / 2));
      event.preventDefault();
    } else if (key === '3') {
      onSectionChange?.('z', Math.floor((N - 1) / 2));
      event.preventDefault();
    } else if (key === '+' || key === '=') {
      if (sectionAxis) {
        const next = Math.min(sectionPos + 1, N - 1);
        onSectionChange?.(sectionAxis, next);
        event.preventDefault();
      }
    } else if (key === '-') {
      if (sectionAxis) {
        const next = Math.max(sectionPos - 1, 0);
        onSectionChange?.(sectionAxis, next);
        event.preventDefault();
      }
    } else if (key === 'ArrowUp' || key === 'ArrowRight') {
      if (sectionAxis) {
        const next = Math.min(sectionPos + 1, N - 1);
        onSectionChange?.(sectionAxis, next);
        event.preventDefault();
      }
    } else if (key === 'ArrowDown' || key === 'ArrowLeft') {
      if (sectionAxis) {
        const next = Math.max(sectionPos - 1, 0);
        onSectionChange?.(sectionAxis, next);
        event.preventDefault();
      }
    }
  }

  // ── 旋转工具 ──────────────────────────────────────

  /**
   * 应用棋盘旋转。先绕世界 Z 轴俯仰（tiltPitch），再绕自身 Y 轴偏航（tiltYaw）。
   * 自转时 tiltYaw 持续增长（绕倾斜后的自身 Y 轴），实现体对角线为旋转轴的效果。
   */
  function applyTilt() {
    if (!boardPivot) return;
    boardPivot.quaternion.setFromAxisAngle(new THREE.Vector3(0, 0, 1), tiltPitch);
    boardPivot.rotateY(tiltYaw);
  }

  /** 拖拽结束后 2 秒启动俯仰回正 */
  function startSnapTimer() {
    clearTimeout(snapTimer);
    snappingTilt = false;
    snapTimer = setTimeout(() => { snappingTilt = true; keepLoopAlive(); }, 2000);
  }

  // ── 渲染 ──────────────────────────────────────────

  const LOOP_IDLE_MS = 2000;
  let _loopActive = false;
  let _loopTimer: number | undefined;
  let _rafId: number;

  function keepLoopAlive() {
    if (!_loopActive) {
      _loopActive = true;
      (function loop() {
        if (!_loopActive) return;
        _rafId = requestAnimationFrame(loop);

        // ── 开始游戏：slerp 到目标姿态 ──
        if (snappingToCamera && boardPivot) {
          boardPivot.quaternion.slerp(targetQuat, 0.15);
          if (boardPivot.quaternion.angleTo(targetQuat) < 0.01) {
            boardPivot.quaternion.copy(targetQuat);
            snappingToCamera = false;
          }
          renderer.render(scene, camera); return;
        }

        // ── 自转：绕自身 Y 轴缓慢旋转 ──
        if (autoRotate && !isDragging) tiltYaw += 0.0025;

        // ── 阻尼惯性：拖拽后速度衰减 ──
        if (!isDragging && Math.abs(dragVel) > 0.0001) {
          tiltYaw += dragVel;
          dragVel *= 0.92;
        }

        // ── 俯仰回正：松手后平滑回到默认前倾角 ──
        if (snappingTilt) {
          tiltPitch += (DEFAULT_PITCH - tiltPitch) * 0.04;
          if (Math.abs(DEFAULT_PITCH - tiltPitch) < 0.003) {
            tiltPitch = DEFAULT_PITCH;
            snappingTilt = false;
          }
        }

        if (autoRotate || Math.abs(dragVel) > 0.0001 || snappingTilt) applyTilt();

        renderer.render(scene, camera);
      })();
    }
    clearTimeout(_loopTimer);
    if (autoRotate || snappingTilt || snappingToCamera || Math.abs(dragVel) > 0.0001 || isDragging) {
      // 保持循环
    } else {
      _loopTimer = window.setTimeout(() => { _loopActive = false; }, LOOP_IDLE_MS);
    }
  }

  function renderFrame() {
    renderer.render(scene, camera);
  }

  function disposeGroup(group: THREE.Group) {
    while (group.children.length) {
      const child = group.children[0] as THREE.Mesh | THREE.LineSegments;
      if (child instanceof THREE.Mesh || child instanceof THREE.LineSegments) {
        child.geometry?.dispose();
        if ('material' in child) {
          (child.material as THREE.Material)?.dispose();
        }
      }
      group.remove(child);
    }
  }
</script>

<div bind:this={container} class="board-container"></div>

<style>
  .board-container {
    width: 100%;
    height: 100%;
    outline: none;
  }
</style>
