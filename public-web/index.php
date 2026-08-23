<?php
$version    = '1.0.0';
$year       = date('Y');
$github_url = 'https://github.com/kladhest-se/lineclash';
$kofi_url   = 'https://ko-fi.com/frossmant';
?><!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LineClash — Multiplayer Falling Blocks</title>
<meta name="description" content="Fast, brutal multiplayer falling blocks. Open source, self-hosted, up to 8 players.">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=Inter:wght@400;500;600&display=swap" rel="stylesheet">
<style>
/* ── Catppuccin Frappé ─────────────────────────────────── */
:root {
  --crust:    #232634;
  --mantle:   #292c3c;
  --base:     #303446;
  --surface0: #414559;
  --surface1: #51576d;
  --surface2: #626880;
  --overlay0: #737994;
  --overlay1: #838ba7;
  --subtext1: #b5bfe2;
  --text:     #c6d0f5;
  --lavender: #babbf1;
  --blue:     #8caaee;
  --sapphire: #85c1dc;
  --sky:      #99d1db;
  --teal:     #81c8be;
  --green:    #a6d189;
  --yellow:   #e5c890;
  --peach:    #ef9f76;
  --red:      #e78284;
  --mauve:    #ca9ee6;
  --pink:     #f4b8e4;
  --rosewater:#f2d5cf;

  --mono: 'Space Mono', monospace;
  --sans: 'Inter', system-ui, sans-serif;

  --radius: 8px;
  --radius-lg: 14px;
}

* { box-sizing: border-box; margin: 0; padding: 0; }
html { scroll-behavior: smooth; }

body {
  background: var(--crust);
  color: var(--text);
  font-family: var(--sans);
  font-size: 16px;
  line-height: 1.65;
  -webkit-font-smoothing: antialiased;
}

a { color: var(--mauve); text-decoration: none; }
a:hover { text-decoration: underline; }
a:focus-visible { outline: 2px solid var(--mauve); outline-offset: 3px; border-radius: 3px; }

/* ── Nav ─────────────────────────────────────────────────── */
.nav {
  position: sticky;
  top: 0;
  z-index: 200;
  background: rgba(35, 38, 52, 0.90);
  backdrop-filter: blur(14px);
  -webkit-backdrop-filter: blur(14px);
  border-bottom: 1px solid var(--surface0);
}
.nav-inner {
  display: flex;
  align-items: center;
  justify-content: space-between;
  max-width: 1000px;
  margin: 0 auto;
  padding: 0 28px;
  height: 58px;
}
.nav-logo {
  font-family: var(--mono);
  font-size: 17px;
  font-weight: 700;
  color: var(--text);
  letter-spacing: -0.5px;
}
.nav-logo .clash { color: var(--mauve); }
.nav-links {
  display: flex;
  gap: 32px;
  list-style: none;
  font-size: 14px;
  font-weight: 500;
}
.nav-links a { color: var(--overlay1); transition: color .15s; }
.nav-links a:hover { color: var(--text); text-decoration: none; }
.nav-btn {
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.04em;
  padding: 8px 18px;
  background: var(--mauve);
  color: var(--crust);
  border-radius: var(--radius);
  transition: opacity .15s;
}
.nav-btn:hover { opacity: .85; text-decoration: none; }

/* ── Hero ─────────────────────────────────────────────────── */
.hero {
  position: relative;
  min-height: 100svh;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
}
#hero-canvas {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  opacity: .18;
  pointer-events: none;
}
.hero-content {
  position: relative;
  z-index: 2;
  text-align: center;
  padding: 80px 28px 60px;
  max-width: 720px;
  margin: 0 auto;
}
.hero-eyebrow {
  display: inline-block;
  font-family: var(--mono);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: .15em;
  text-transform: uppercase;
  color: var(--green);
  background: rgba(166, 209, 137, .1);
  border: 1px solid rgba(166, 209, 137, .25);
  border-radius: 100px;
  padding: 5px 16px;
  margin-bottom: 32px;
}
.hero-title {
  font-family: var(--mono);
  font-size: clamp(56px, 11vw, 108px);
  font-weight: 700;
  line-height: .9;
  letter-spacing: -3px;
  margin-bottom: 28px;
}
.hero-title .line  { color: var(--text); }
.hero-title .clash { color: var(--mauve); }
.hero-sub {
  font-size: 18px;
  color: var(--subtext1);
  max-width: 480px;
  margin: 0 auto 44px;
  line-height: 1.6;
}
.hero-actions {
  display: flex;
  gap: 12px;
  justify-content: center;
  flex-wrap: wrap;
}

/* ── Buttons ──────────────────────────────────────────────── */
.btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 12px 26px;
  border-radius: var(--radius);
  font-size: 14px;
  font-weight: 600;
  transition: all .15s;
  cursor: pointer;
}
.btn:hover { text-decoration: none; }
.btn-primary { background: var(--mauve); color: var(--crust); }
.btn-primary:hover { background: var(--lavender); }
.btn-outline {
  background: transparent;
  color: var(--subtext1);
  border: 1px solid var(--surface1);
}
.btn-outline:hover { border-color: var(--overlay1); color: var(--text); }

/* ── Sections ─────────────────────────────────────────────── */
.section {
  padding: 96px 28px;
  max-width: 1000px;
  margin: 0 auto;
}
.section-divider {
  height: 1px;
  background: var(--surface0);
  max-width: 1000px;
  margin: 0 auto;
}
.eyebrow {
  font-family: var(--mono);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: .15em;
  text-transform: uppercase;
  color: var(--blue);
  margin-bottom: 12px;
}
.section-title {
  font-family: var(--mono);
  font-size: clamp(26px, 4vw, 38px);
  font-weight: 700;
  letter-spacing: -1px;
  margin-bottom: 12px;
  color: var(--text);
}
.section-sub {
  color: var(--overlay1);
  font-size: 15px;
  max-width: 520px;
  margin-bottom: 52px;
  line-height: 1.6;
}

/* ── Feature cards ────────────────────────────────────────── */
.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 12px;
}
.feature-card {
  background: var(--base);
  border: 1px solid var(--surface0);
  border-radius: var(--radius-lg);
  padding: 28px;
  transition: border-color .2s, transform .2s;
}
.feature-card:hover {
  border-color: var(--surface2);
  transform: translateY(-2px);
}
.feature-icon {
  width: 36px;
  height: 36px;
  border-radius: var(--radius);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
  margin-bottom: 16px;
}
.icon-mauve  { background: rgba(202,158,230,.15); }
.icon-blue   { background: rgba(140,170,238,.15); }
.icon-green  { background: rgba(166,209,137,.15); }
.icon-peach  { background: rgba(239,159,118,.15); }
.icon-sky    { background: rgba(153,209,219,.15); }
.icon-yellow { background: rgba(229,200,144,.15); }
.feature-card h3 {
  font-size: 15px;
  font-weight: 600;
  color: var(--text);
  margin-bottom: 8px;
}
.feature-card p { font-size: 14px; color: var(--overlay1); line-height: 1.6; }

/* ── Specials ─────────────────────────────────────────────── */
.specials-row {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}
.special-chip {
  display: flex;
  align-items: center;
  gap: 10px;
  background: var(--base);
  border: 1px solid var(--surface0);
  border-radius: var(--radius);
  padding: 10px 16px;
  transition: border-color .15s;
}
.special-chip:hover { border-color: var(--mauve); }
.special-chip-key {
  font-family: var(--mono);
  font-size: 10px;
  font-weight: 700;
  letter-spacing: .05em;
  color: var(--mauve);
  background: rgba(202,158,230,.12);
  border-radius: 4px;
  padding: 2px 7px;
  white-space: nowrap;
}
.special-chip-name { font-size: 13px; font-weight: 600; color: var(--subtext1); }

/* ── Build tabs ───────────────────────────────────────────── */
.tab-bar {
  display: flex;
  border-bottom: 1px solid var(--surface0);
  margin-bottom: -1px;
  gap: 0;
}
.tab-btn {
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
  letter-spacing: .04em;
  padding: 10px 22px;
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  color: var(--overlay0);
  cursor: pointer;
  transition: color .15s, border-color .15s;
  margin-bottom: -1px;
}
.tab-btn:hover { color: var(--text); }
.tab-btn.active { color: var(--mauve); border-bottom-color: var(--mauve); }
.tab-panel { display: none; }
.tab-panel.active { display: block; }
pre.code-block {
  background: var(--mantle);
  border: 1px solid var(--surface0);
  border-radius: 0 var(--radius-lg) var(--radius-lg) var(--radius-lg);
  padding: 28px;
  font-family: var(--mono);
  font-size: 13px;
  line-height: 1.8;
  overflow-x: auto;
  color: var(--text);
  margin-top: 0;
}
.c { color: var(--overlay0); }   /* comment */
.k { color: var(--mauve); }      /* keyword / command */
.s { color: var(--green); }      /* string / success */

/* ── Platforms ────────────────────────────────────────────── */
.platform-row {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-top: 36px;
}
.platform-chip {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: var(--subtext1);
  background: var(--base);
  border: 1px solid var(--surface0);
  border-radius: 100px;
  padding: 6px 16px;
}
.dot { width: 7px; height: 7px; border-radius: 50%; background: var(--green); flex-shrink: 0; }

/* ── Source cards ─────────────────────────────────────────── */
.source-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 12px;
}
.source-card {
  background: var(--base);
  border: 1px solid var(--surface0);
  border-radius: var(--radius-lg);
  padding: 24px;
  transition: border-color .2s;
}
.source-card:hover { border-color: var(--mauve); }
.source-card h3 {
  font-family: var(--mono);
  font-size: 13px;
  color: var(--mauve);
  margin-bottom: 8px;
}
.source-card p { font-size: 13px; color: var(--overlay1); margin-bottom: 16px; line-height: 1.5; }
.source-link {
  font-family: var(--mono);
  font-size: 11px;
  color: var(--overlay0);
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.source-link:hover { color: var(--mauve); text-decoration: none; }

/* ── Ko-fi ────────────────────────────────────────────────── */
.kofi-wrap { margin-bottom: 44px; }
.kofi-wrap a:hover { text-decoration: none; }

/* ── Footer ───────────────────────────────────────────────── */
footer {
  border-top: 1px solid var(--surface0);
  background: var(--mantle);
  padding: 36px 28px;
}
.footer-inner {
  max-width: 1000px;
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 16px;
}
.footer-logo { font-family: var(--mono); font-size: 14px; color: var(--overlay0); }
.footer-logo .clash { color: var(--mauve); }
.footer-links { display: flex; gap: 24px; list-style: none; }
.footer-links a { font-size: 13px; color: var(--overlay0); }
.footer-links a:hover { color: var(--text); text-decoration: none; }
.footer-copy { font-size: 12px; color: var(--surface2); }

/* ── Responsive ───────────────────────────────────────────── */
@media (max-width: 640px) {
  .nav-links { display: none; }
  .hero-title { letter-spacing: -2px; }
  .footer-inner { flex-direction: column; text-align: center; }
  .footer-links { justify-content: center; }
}
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { animation-duration: .01ms !important; transition-duration: .01ms !important; }
}
</style>
</head>
<body>

<nav class="nav" role="navigation" aria-label="Main">
  <div class="nav-inner">
    <span class="nav-logo">Line<span class="clash">Clash</span></span>
    <ul class="nav-links">
      <li><a href="#features">Features</a></li>
      <li><a href="#specials">Specials</a></li>
      <li><a href="#build">Build</a></li>
      <li><a href="#source">Source</a></li>
    </ul>
    <a class="nav-btn" href="<?= htmlspecialchars($github_url) ?>">GitHub</a>
  </div>
</nav>

<section class="hero" aria-labelledby="hero-heading">
  <canvas id="hero-canvas" aria-hidden="true"></canvas>
  <div class="hero-content">
    <div class="hero-eyebrow">v<?= htmlspecialchars($version) ?> &mdash; GPL-3.0 &mdash; Open Source</div>
    <h1 class="hero-title" id="hero-heading">
      <span class="line">Line</span><span class="clash">Clash</span>
    </h1>
    <p class="hero-sub">Fast, brutal multiplayer falling blocks. Up to 8 players, AI bots, 10 specials, full guideline gameplay. Self-hosted.</p>
    <div class="hero-actions">
      <a class="btn btn-primary" href="#build">Build it</a>
      <a class="btn btn-outline" href="<?= htmlspecialchars($github_url) ?>">View source</a>
    </div>
  </div>
</section>

<div class="section-divider"></div>

<!-- Features -->
<div class="section" id="features">
  <p class="eyebrow">What you get</p>
  <h2 class="section-title">Fully featured. Nothing missing.</h2>
  <p class="section-sub">Real-time multiplayer with the depth of guideline play and the chaos of specials.</p>
  <div class="features-grid">
    <div class="feature-card">
      <div class="feature-icon icon-mauve">⚡</div>
      <h3>Real-time multiplayer</h3>
      <p>Up to 8 players per room. Spectator mode. Team play. Reconnect window keeps your slot when you drop.</p>
    </div>
    <div class="feature-card">
      <div class="feature-icon icon-blue">🎯</div>
      <h3>Guideline gameplay</h3>
      <p>SRS wall kicks, hold piece, lock delay, back-to-back bonus, combos, and perfect clear. All configurable per room.</p>
    </div>
    <div class="feature-card">
      <div class="feature-icon icon-green">🤖</div>
      <h3>AI bots</h3>
      <p>Configurable difficulty. Bots play with full game rules — garbage targeting, B2B, combos, and all 10 specials.</p>
    </div>
    <div class="feature-card">
      <div class="feature-icon icon-peach">💾</div>
      <h3>Persistent rooms</h3>
      <p>Room settings saved to SQLite and restored on restart. Leaderboard and stats HTTP API included.</p>
    </div>
    <div class="feature-card">
      <div class="feature-icon icon-sky">🖥️</div>
      <h3>Two clients</h3>
      <p>SDL2 GUI client with HiDPI/Retina support. Terminal CLI client for headless or SSH play.</p>
    </div>
    <div class="feature-card">
      <div class="feature-icon icon-yellow">📦</div>
      <h3>Self-hosted</h3>
      <p>Run your own server. Packages for macOS, openSUSE Leap, and Ubuntu. One binary, no runtime dependencies.</p>
    </div>
  </div>
  <div class="platform-row">
    <div class="platform-chip"><span class="dot"></span>macOS arm64</div>
    <div class="platform-chip"><span class="dot"></span>openSUSE Leap 15</div>
    <div class="platform-chip"><span class="dot"></span>Ubuntu 24.04</div>
    <div class="platform-chip"><span class="dot"></span>Any Linux (build from source)</div>
  </div>
</div>

<div class="section-divider"></div>

<!-- Specials -->
<div class="section" id="specials">
  <p class="eyebrow">Combat</p>
  <h2 class="section-title">10 specials. Pick your chaos.</h2>
  <p class="section-sub">Clear lines to earn specials. Fire them at opponents — or yourself when it helps.</p>
  <div class="specials-row">
    <?php
    $specials = [
      ['A', 'Add Lines'],
      ['C', 'Clear Lines'],
      ['N', 'Nuke'],
      ['R', 'Random Clear'],
      ['S', 'Switch Field'],
      ['B', 'Block Bomb'],
      ['G', 'Gravity'],
      ['Q', 'Quake'],
      ['O', 'Column Clear'],
      ['→', 'Targeting'],
    ];
    foreach ($specials as [$key, $name]):
    ?>
    <div class="special-chip">
      <span class="special-chip-key"><?= htmlspecialchars($key) ?></span>
      <span class="special-chip-name"><?= htmlspecialchars($name) ?></span>
    </div>
    <?php endforeach; ?>
  </div>
</div>

<div class="section-divider"></div>

<!-- Build -->
<div class="section" id="build">
  <p class="eyebrow">Get started</p>
  <h2 class="section-title">Build from source.</h2>
  <p class="section-sub">A C compiler and SDL2 are all you need. Compiles in seconds.</p>

  <div class="tab-bar" role="tablist">
    <button class="tab-btn active" role="tab" aria-selected="true"  aria-controls="tab-macos"   onclick="showTab(this,'macos')">macOS</button>
    <button class="tab-btn"        role="tab" aria-selected="false" aria-controls="tab-opensuse" onclick="showTab(this,'opensuse')">openSUSE</button>
    <button class="tab-btn"        role="tab" aria-selected="false" aria-controls="tab-ubuntu"   onclick="showTab(this,'ubuntu')">Ubuntu</button>
  </div>

  <div id="tab-macos" class="tab-panel active" role="tabpanel">
<pre class="code-block"><span class="c"># Install SDL2</span>
<span class="s">brew</span> install sdl2

<span class="c"># Clone and build</span>
<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash &amp;&amp; make

<span class="c"># Run</span>
<span class="s">./server/lineclashd</span> &amp;
<span class="s">./clients/gui/lineclash</span></pre>
  </div>
  <div id="tab-opensuse" class="tab-panel" role="tabpanel">
<pre class="code-block"><span class="c"># Install dependencies</span>
<span class="s">sudo zypper</span> install -y gcc make libSDL2-devel sqlite3-devel

<span class="c"># Clone and build</span>
<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash &amp;&amp; make

<span class="c"># Run</span>
<span class="s">./server/lineclashd</span> &amp;
<span class="s">./clients/gui/lineclash</span></pre>
  </div>
  <div id="tab-ubuntu" class="tab-panel" role="tabpanel">
<pre class="code-block"><span class="c"># Install dependencies</span>
<span class="s">sudo apt</span> install gcc make libsdl2-dev

<span class="c"># Clone and build</span>
<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash &amp;&amp; make

<span class="c"># Run</span>
<span class="s">./server/lineclashd</span> &amp;
<span class="s">./clients/gui/lineclash</span></pre>
  </div>
</div>

<div class="section-divider"></div>

<!-- Source / Sponsor -->
<div class="section" id="source">
  <p class="eyebrow">Open source</p>
  <h2 class="section-title">GPL-3.0. Fork it.</h2>
  <p class="section-sub">Built by <a href="https://github.com/frossmant">Tommy Frössman</a>. Source on GitHub, issues welcome.</p>

  <div class="kofi-wrap">
    <a href="<?= htmlspecialchars($kofi_url) ?>" target="_blank" rel="noopener">
      <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi" height="36">
    </a>
  </div>

  <div class="source-grid">
    <div class="source-card">
      <h3>GitHub</h3>
      <p>Source, issues, and releases. Pull requests welcome.</p>
      <a class="source-link" href="<?= htmlspecialchars($github_url) ?>">github.com/kladhest-se/lineclash</a>
    </div>
    <div class="source-card">
      <h3>Issues</h3>
      <p>Bug reports and feature requests.</p>
      <a class="source-link" href="<?= htmlspecialchars($github_url) ?>/issues">github.com/kladhest-se/lineclash/issues</a>
    </div>
    <div class="source-card">
      <h3>License</h3>
      <p>GPL-3.0 — free to use, modify, and distribute.</p>
      <a class="source-link" href="<?= htmlspecialchars($github_url) ?>/blob/master/LICENSE.md">GPL-3.0 License</a>
    </div>
  </div>
</div>

<footer>
  <div class="footer-inner">
    <span class="footer-logo">Line<span class="clash">Clash</span> v<?= htmlspecialchars($version) ?></span>
    <ul class="footer-links">
      <li><a href="<?= htmlspecialchars($github_url) ?>">GitHub</a></li>
      <li><a href="<?= htmlspecialchars($github_url) ?>/issues">Issues</a></li>
      <li><a href="<?= htmlspecialchars($kofi_url) ?>">Ko-fi</a></li>
      <li><a href="<?= htmlspecialchars($github_url) ?>/blob/master/LICENSE.md">GPL-3.0</a></li>
    </ul>
    <span class="footer-copy">&copy; <?= $year ?> Tommy Frössman</span>
  </div>
</footer>

<script>
/* ── Tab switching ─────────────────────────────────────── */
function showTab(btn, id) {
  document.querySelectorAll('.tab-btn').forEach(b => {
    b.classList.remove('active');
    b.setAttribute('aria-selected', 'false');
  });
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  btn.classList.add('active');
  btn.setAttribute('aria-selected', 'true');
  document.getElementById('tab-' + id).classList.add('active');
}

/* ── Falling blocks canvas ─────────────────────────────── */
(function () {
  const canvas = document.getElementById('hero-canvas');
  const ctx    = canvas.getContext('2d');

  /* Catppuccin Frappé piece colours */
  const COLOURS = [
    '#99d1db', /* sky   — I */
    '#e5c890', /* yellow— O */
    '#ca9ee6', /* mauve — T */
    '#a6d189', /* green — S */
    '#e78284', /* red   — Z */
    '#8caaee', /* blue  — J */
    '#ef9f76', /* peach — L */
  ];

  const CELL = 28;
  let cols, rows, pieces = [];

  function resize() {
    canvas.width  = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
    cols  = Math.ceil(canvas.width  / CELL) + 2;
    rows  = Math.ceil(canvas.height / CELL) + 4;
  }

  function randInt(a, b) { return Math.floor(Math.random() * (b - a + 1)) + a; }

  function spawn() {
    const col    = COLOURS[randInt(0, COLOURS.length - 1)];
    const width  = randInt(1, 4);
    const height = randInt(1, 3);
    return {
      x:     randInt(0, cols - width) * CELL,
      y:     -height * CELL - randInt(0, 80),
      w:     width  * CELL - 2,
      h:     height * CELL - 2,
      speed: 0.25 + Math.random() * 0.45,
      col,
    };
  }

  function init() {
    pieces = [];
    for (let i = 0; i < Math.floor(cols * 0.6); i++) {
      const p = spawn();
      p.y = randInt(-canvas.height, 0);
      pieces.push(p);
    }
  }

  let raf;
  function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    for (let i = pieces.length - 1; i >= 0; i--) {
      const p = pieces[i];
      p.y += p.speed;

      ctx.fillStyle = p.col;
      ctx.beginPath();
      ctx.roundRect(p.x, p.y, p.w, p.h, 3);
      ctx.fill();

      if (p.y > canvas.height + 20) {
        pieces[i] = spawn();
      }
    }

    /* Spawn new pieces so density stays constant */
    while (pieces.length < Math.floor(cols * 0.6)) pieces.push(spawn());

    raf = requestAnimationFrame(draw);
  }

  const ro = new ResizeObserver(() => {
    resize();
    init();
  });
  ro.observe(canvas);

  resize();
  init();
  draw();

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) cancelAnimationFrame(raf);
    else draw();
  });
})();
</script>
</body>
</html>
