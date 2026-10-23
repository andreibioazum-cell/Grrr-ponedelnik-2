/* Cubic Battle 4: 3D Minecraft — Web Worker (WASM + WebGL2 + Live Firebase + Audio) */
"use strict";

let wasm = null;
let mem = null;
let gl = null;
let canvas = null;
let prog = null;
let vbo = null;
let ibo = null;
let uRes = null, uPipe = null, uGray = null, uTex = null;
const texCache = new Map();
let whiteTex = null;

/* In-memory WASI filesystem with persistent cb4_data/* mirrored to main-thread localStorage */
const vfs = new Map();
const openFds = new Map();
let nextFd = 4;

const dec = new TextDecoder("utf-8");
const enc = new TextEncoder();

function cstr(ptr) {
  if (!ptr) return "";
  const u8 = new Uint8Array(mem.buffer);
  let e = ptr;
  while (e < u8.length && u8[e] !== 0) e++;
  return dec.decode(u8.subarray(ptr, e));
}

function writeCstr(ptr, cap, str) {
  if (!ptr || cap <= 0) return 0;
  const bytes = enc.encode(str || "");
  const u8 = new Uint8Array(mem.buffer);
  const n = Math.min(bytes.length, cap - 1);
  u8.set(bytes.subarray(0, n), ptr);
  u8[ptr + n] = 0;
  return n;
}

function normPath(raw) {
  return raw.replace(/^\.?\//, "").replace(/\/+/g, "/");
}

function syncSaveFile(path) {
  if (!path.startsWith("cb4_data/") || path.endsWith(".tmp")) return;
  const data = vfs.get(path);
  if (!data) {
    self.postMessage({ type: "save_del", key: path });
    return;
  }
  let bin = "";
  for (let i = 0; i < data.length; i++) bin += String.fromCharCode(data[i]);
  self.postMessage({ type: "save_put", key: path, b64: btoa(bin) });
}

/* Non-blocking async cache for repeated Firebase GET polls + direct sync XHR for writes/auth */
const getCache = new Map();

function pollUrlAsync(url, headersObj) {
  const entry = getCache.get(url);
  const now = performance.now();
  if (entry && (entry.inflight || now - entry.ts < 85)) return;
  const rec = entry || { status: 0, body: "", etag: "", ts: 0, inflight: false };
  rec.inflight = true;
  getCache.set(url, rec);
  fetch(url, { method: "GET", headers: headersObj, cache: "no-store" })
    .then(async (r) => {
      const txt = await r.text();
      rec.status = r.status;
      if (r.status !== 304) rec.body = txt;
      const et = r.headers.get("etag") || "";
      if (et) rec.etag = et;
      rec.ts = performance.now();
      rec.inflight = false;
    })
    .catch(() => {
      rec.inflight = false;
      rec.ts = performance.now();
    });
}

function js_http_ex(methodPtr, urlPtr, bodyPtr, outPtr, cap, hdrPtr, valPtr, etagPtr, etagCap) {
  const method = cstr(methodPtr) || "GET";
  const url = cstr(urlPtr);
  const body = cstr(bodyPtr);
  const hdr = cstr(hdrPtr);
  const val = cstr(valPtr);
  if (!url) return 0;

  /* Fast non-blocking background GET polls for real-time 60 FPS multiplayer rooms! */
  const isFastPoll =
    method === "GET" &&
    (url.includes("/rooms/") || url.includes("/HitboxChaos") || url.includes("/FlashSale") ||
      url.includes("/AzumBuff") || url.includes("/bans.json")) &&
    !url.includes("rooms.json");

  if (isFastPoll) {
    const headersObj = {};
    if (hdr && val) headersObj[hdr] = val;
    pollUrlAsync(url, headersObj);
    const cached = getCache.get(url);
    if (!cached || cached.status === 0) {
      /* Prime very first read synchronously once so room entry is immediate */
      try {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", url, false);
        if (hdr && val) xhr.setRequestHeader(hdr, val);
        xhr.send(null);
        const et = xhr.getResponseHeader("ETag") || "";
        getCache.set(url, {
          status: xhr.status,
          body: xhr.responseText || "",
          etag: et,
          ts: performance.now(),
          inflight: false,
        });
        writeCstr(outPtr, cap, xhr.responseText || "");
        writeCstr(etagPtr, etagCap, et);
        return xhr.status || 0;
      } catch (_e) {
        return 0;
      }
    }
    writeCstr(outPtr, cap, cached.body);
    writeCstr(etagPtr, etagCap, cached.etag);
    return cached.status;
  }

  /* Push writes asynchronously when updating our own 60Hz slot so FPS stays locked at 60! */
  if ((method === "PUT" || method === "PATCH") && url.includes("/players/")) {
    fetch(url, {
      method,
      headers: { "Content-Type": "application/json" },
      body,
      keepalive: true,
    }).catch(() => {});
    writeCstr(outPtr, cap, body);
    return 200;
  }

  /* Synchronous XHR inside Worker for Auth, Profile, Room Create/Join, Chat & 3D Block Sync */
  try {
    const xhr = new XMLHttpRequest();
    xhr.open(method, url, false);
    xhr.setRequestHeader("Content-Type", "application/json");
    if (hdr && val) xhr.setRequestHeader(hdr, val);
    xhr.send(method === "GET" || method === "DELETE" ? null : body);
    writeCstr(outPtr, cap, xhr.responseText || "");
    const et = xhr.getResponseHeader("ETag") || "";
    writeCstr(etagPtr, etagCap, et);
    /* Invalidate chat cache immediately after writing a 3D block edit or chat line! */
    if (url.includes("/chat/")) {
      for (const k of getCache.keys()) {
        if (k.includes("/chat.json")) getCache.delete(k);
      }
    }
    return xhr.status || 0;
  } catch (_e) {
    return 0;
  }
}

/* WebGL2 Hardware Batch Renderer */
function initWebGL(offscreen) {
  canvas = offscreen;
  gl = canvas.getContext("webgl2", { alpha: false, antialias: false, depth: false, powerPreference: "high-performance" });
  const vsSrc = `#version 300 es
    precision highp float;
    layout(location = 0) in vec2 a_pos;
    layout(location = 1) in vec2 a_uv;
    layout(location = 2) in vec4 a_col;
    uniform vec2 u_res;
    out vec2 v_uv;
    out vec4 v_col;
    void main() {
      vec2 ndc = (a_pos / u_res) * 2.0 - 1.0;
      gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
      v_uv = a_uv;
      v_col = a_col;
    }`;
  const fsSrc = `#version 300 es
    precision mediump float;
    in vec2 v_uv;
    in vec4 v_col;
    uniform sampler2D u_tex;
    uniform int u_pipe;
    uniform int u_gray;
    out vec4 fragColor;
    void main() {
      vec4 c = v_col;
      if (u_pipe == 1) {
        c = v_col * texture(u_tex, v_uv);
      } else if (u_pipe == 2) {
        c = vec4(v_col.rgb, v_col.a * texture(u_tex, v_uv).a);
      }
      if (c.a <= 0.002) discard;
      if (u_gray == 1) {
        float y = dot(c.rgb, vec3(0.299, 0.587, 0.114));
        c.rgb = vec3(y);
      }
      fragColor = c;
    }`;
  const compile = (type, src) => {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    return s;
  };
  prog = gl.createProgram();
  gl.attachShader(prog, compile(gl.VERTEX_SHADER, vsSrc));
  gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fsSrc));
  gl.linkProgram(prog);
  gl.useProgram(prog);

  uRes = gl.getUniformLocation(prog, "u_res");
  uPipe = gl.getUniformLocation(prog, "u_pipe");
  uGray = gl.getUniformLocation(prog, "u_gray");
  uTex = gl.getUniformLocation(prog, "u_tex");
  gl.uniform1i(uTex, 0);

  const vao = gl.createVertexArray();
  gl.bindVertexArray(vao);
  vbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
  gl.bufferData(gl.ARRAY_BUFFER, 65536 * 20, gl.DYNAMIC_DRAW);

  /* GeoVert = { float x, y, u, v; uint32_t c; } = 20 bytes/vertex */
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 20, 0);
  gl.enableVertexAttribArray(1);
  gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 20, 8);
  gl.enableVertexAttribArray(2);
  gl.vertexAttribPointer(2, 4, gl.UNSIGNED_BYTE, true, 20, 16);

  ibo = gl.createBuffer();
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, 393216 * 2, gl.DYNAMIC_DRAW);

  whiteTex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, whiteTex);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([255, 255, 255, 255]));

  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
}

function getGpuTexture(ptr, w, h) {
  if (!ptr || w <= 0 || h <= 0) return whiteTex;
  let entry = texCache.get(ptr);
  const src = new Uint8Array(mem.buffer, ptr, w * h * 4);
  /* Sample 4 bytes signature so dynamic font atlas updates re-upload automatically */
  const sig = (w << 16) ^ h ^ (src[0] << 8) ^ src[(src.length >> 1) & ~3];
  if (entry && entry.sig === sig) return entry.tex;
  const tex = entry ? entry.tex : gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, src);
  /* Crisp nearest-neighbor pixel-art filtering for Minecraft block/skin textures! */
  const isPixelArt = (w === 256 && h === 256) || (w <= 64 && h <= 64);
  const flt = isPixelArt ? gl.NEAREST : gl.LINEAR;
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, flt);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, flt);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  texCache.set(ptr, { tex, sig });
  return tex;
}

function js_gl_submit(w, h, vertsPtr, vn, idxPtr, inCnt, batchesPtr, bn, gray) {
  if (!gl || vn <= 0 || inCnt <= 0 || bn <= 0) return;
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  gl.viewport(0, 0, w, h);
  gl.uniform2f(uRes, w, h);
  gl.uniform1i(uGray, gray);

  gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array(mem.buffer, vertsPtr, vn * 20));
  gl.bufferSubData(gl.ELEMENT_ARRAY_BUFFER, 0, new Uint16Array(mem.buffer, idxPtr, inCnt));

  const dv = new DataView(mem.buffer, batchesPtr, bn * 24);
  for (let i = 0; i < bn; i++) {
    const off = i * 24;
    const pipe = dv.getInt32(off, true);
    const texPtr = dv.getUint32(off + 4, true);
    const tw = dv.getInt32(off + 8, true);
    const th = dv.getInt32(off + 12, true);
    const first = dv.getUint32(off + 16, true);
    const count = dv.getUint32(off + 20, true);
    gl.uniform1i(uPipe, pipe);
    gl.bindTexture(gl.TEXTURE_2D, pipe === 0 ? whiteTex : getGpuTexture(texPtr, tw, th));
    gl.drawElements(gl.TRIANGLES, count, gl.UNSIGNED_SHORT, first * 2);
  }
}

/* Minimal rock-solid WASI preview1 implementation over vfs Map */
const wasi = {
  clock_time_get(_id, _prec, outPtr) {
    const ns = BigInt(Math.floor(performance.now() * 1e6));
    new DataView(mem.buffer).setBigUint64(outPtr, ns, true);
    return 0;
  },
  fd_prestat_get(fd, bufPtr) {
    if (fd !== 3) return 8; /* EBADF */
    const dv = new DataView(mem.buffer);
    dv.setUint8(bufPtr, 0);
    dv.setUint32(bufPtr + 4, 1, true); /* "." */
    return 0;
  },
  fd_prestat_dir_name(fd, pathPtr, _pathLen) {
    if (fd !== 3) return 8;
    new Uint8Array(mem.buffer)[pathPtr] = 46; /* '.' */
    return 0;
  },
  fd_fdstat_get(fd, bufPtr) {
    const u8 = new Uint8Array(mem.buffer, bufPtr, 24);
    u8.fill(0);
    u8[0] = fd === 3 ? 3 : 4;
    return 0;
  },
  fd_fdstat_set_flags() { return 0; },
  path_open(_dirfd, _dirflags, pathPtr, pathLen, oflags, _rb, _ri, fdflags, openedPtr) {
    const raw = dec.decode(new Uint8Array(mem.buffer, pathPtr, pathLen));
    const p = normPath(raw);
    const create = (oflags & 1) !== 0;
    const trunc = (oflags & 8) !== 0;
    const append = (fdflags & 1) !== 0;
    let data = vfs.get(p);
    if (!data) {
      if (!create) return 44; /* ENOENT */
      data = new Uint8Array(0);
      vfs.set(p, data);
    } else if (trunc) {
      data = new Uint8Array(0);
      vfs.set(p, data);
    }
    const fd = nextFd++;
    openFds.set(fd, { path: p, pos: append ? data.length : 0, dirty: create || trunc });
    new DataView(mem.buffer).setUint32(openedPtr, fd, true);
    return 0;
  },
  fd_read(fd, iovsPtr, iovsLen, nreadPtr) {
    const f = openFds.get(fd);
    if (!f) return 8;
    const data = vfs.get(f.path) || new Uint8Array(0);
    const dv = new DataView(mem.buffer);
    const u8 = new Uint8Array(mem.buffer);
    let total = 0;
    for (let i = 0; i < iovsLen; i++) {
      const ptr = dv.getUint32(iovsPtr + i * 8, true);
      const len = dv.getUint32(iovsPtr + i * 8 + 4, true);
      const rem = Math.max(0, data.length - f.pos);
      const n = Math.min(len, rem);
      if (n > 0) {
        u8.set(data.subarray(f.pos, f.pos + n), ptr);
        f.pos += n;
        total += n;
      }
      if (n < len) break;
    }
    dv.setUint32(nreadPtr, total, true);
    return 0;
  },
  fd_write(fd, iovsPtr, iovsLen, nwrittenPtr) {
    const dv = new DataView(mem.buffer);
    const u8 = new Uint8Array(mem.buffer);
    if (fd === 1 || fd === 2) {
      let total = 0;
      for (let i = 0; i < iovsLen; i++) total += dv.getUint32(iovsPtr + i * 8 + 4, true);
      dv.setUint32(nwrittenPtr, total, true);
      return 0;
    }
    const f = openFds.get(fd);
    if (!f) return 8;
    let data = vfs.get(f.path) || new Uint8Array(0);
    let logicalLen = data.length;
    let total = 0;
    for (let i = 0; i < iovsLen; i++) {
      const ptr = dv.getUint32(iovsPtr + i * 8, true);
      const len = dv.getUint32(iovsPtr + i * 8 + 4, true);
      if (f.pos + len > data.length) {
        const grown = new Uint8Array(Math.max(f.pos + len, data.length * 2));
        grown.set(data.subarray(0, logicalLen));
        data = grown;
      }
      data.set(u8.subarray(ptr, ptr + len), f.pos);
      f.pos += len;
      if (f.pos > logicalLen) logicalLen = f.pos;
      total += len;
    }
    vfs.set(f.path, data.subarray(0, logicalLen));
    f.dirty = true;
    dv.setUint32(nwrittenPtr, total, true);
    return 0;
  },
  fd_seek(fd, offsetBig, whence, newOffPtr) {
    const f = openFds.get(fd);
    if (!f) return 8;
    const data = vfs.get(f.path) || new Uint8Array(0);
    const off = Number(offsetBig);
    let np = whence === 0 ? off : whence === 1 ? f.pos + off : data.length + off;
    if (np < 0) np = 0;
    f.pos = np;
    new DataView(mem.buffer).setBigUint64(newOffPtr, BigInt(np), true);
    return 0;
  },
  fd_close(fd) {
    const f = openFds.get(fd);
    if (!f) return 0;
    openFds.delete(fd);
    if (f.dirty) syncSaveFile(f.path);
    return 0;
  },
  path_rename(_od, oldPtr, oldLen, _nd, newPtr, newLen) {
    const op = normPath(dec.decode(new Uint8Array(mem.buffer, oldPtr, oldLen)));
    const np = normPath(dec.decode(new Uint8Array(mem.buffer, newPtr, newLen)));
    const d = vfs.get(op);
    if (!d) return 44;
    vfs.set(np, d);
    vfs.delete(op);
    syncSaveFile(np);
    return 0;
  },
  path_unlink_file(_d, pathPtr, pathLen) {
    const p = normPath(dec.decode(new Uint8Array(mem.buffer, pathPtr, pathLen)));
    vfs.delete(p);
    syncSaveFile(p);
    return 0;
  },
  path_remove_directory() { return 0; },
  proc_exit() {},
};

let audioPtr = 0;
const AUDIO_FRAMES = 1024;
let lastLock = -1;
let lastKb = 0;
let lastTs = 0;

function tickLoop() {
  const now = performance.now();
  const dt = lastTs > 0 ? (now - lastTs) / 1000.0 : 0.016;
  lastTs = now;
  wasm.web_frame(dt);

  const wantLock = wasm.web_wants_lock();
  if (wantLock !== lastLock) {
    lastLock = wantLock;
    self.postMessage({ type: "lock_state", want: wantLock });
  }
  const wantKb = wasm.web_wants_kb();
  if (wantKb !== lastKb) {
    lastKb = wantKb;
    self.postMessage({ type: "kb_state", open: wantKb });
  }
  if (typeof self.requestAnimationFrame === "function") {
    self.requestAnimationFrame(tickLoop);
  } else {
    setTimeout(tickLoop, 16);
  }
}

self.onmessage = async (ev) => {
  const m = ev.data;
  if (m.type === "boot") {
    for (const [k, b64] of Object.entries(m.saves || {})) {
      const bin = atob(b64);
      const u8 = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
      vfs.set(k, u8);
    }
    const manifest = await (await fetch("assets_manifest.json")).json();
    let loaded = 0;
    await Promise.all(
      manifest.map(async (rel) => {
        const buf = await (await fetch(rel)).arrayBuffer();
        vfs.set(rel, new Uint8Array(buf));
        loaded++;
        self.postMessage({ type: "progress", done: loaded, total: manifest.length });
      })
    );
    initWebGL(m.canvas);
    const wasmBytes = await (await fetch("cb4.wasm")).arrayBuffer();
    const { instance } = await WebAssembly.instantiate(wasmBytes, {
      wasi_snapshot_preview1: wasi,
      env: {
        js_gl_submit,
        js_http_ex,
        js_log: (_err, msgPtr) => {
          const line = cstr(msgPtr);
          if (_err) console.warn("[CB4]", line);
        },
      },
    });
    wasm = instance.exports;
    mem = wasm.memory;
    if (wasm._initialize) wasm._initialize();
    audioPtr = wasm.web_alloc(AUDIO_FRAMES * 4);
    wasm.web_init(m.width, m.height);
    self.postMessage({ type: "ready" });
    tickLoop();
    return;
  }
  if (!wasm) return;
  if (m.type === "resize") wasm.web_resize(m.width, m.height);
  else if (m.type === "touch") wasm.web_touch(m.x, m.y, m.action, m.id);
  else if (m.type === "key") wasm.web_key(m.vk, m.down);
  else if (m.type === "mdelta") wasm.web_mouse_delta(m.dx, m.dy);
  else if (m.type === "mbtn") wasm.web_mouse_btn(m.btn, m.down);
  else if (m.type === "wheel") wasm.web_wheel(m.delta);
  else if (m.type === "text") {
    const bytes = enc.encode(m.text + "\0");
    const ptr = wasm.web_alloc(bytes.length + 4);
    new Uint8Array(mem.buffer, ptr, bytes.length).set(bytes);
    wasm.web_text(ptr, m.submit ? 1 : 0);
    wasm.web_free(ptr);
  } else if (m.type === "audio_pull" && audioPtr) {
    wasm.web_mix_audio(audioPtr, AUDIO_FRAMES);
    const copy = new Int16Array(mem.buffer, audioPtr, AUDIO_FRAMES * 2).slice();
    self.postMessage({ type: "audio_pcm", pcm: copy.buffer }, [copy.buffer]);
  }
};
