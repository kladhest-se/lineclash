<?php
require_once __DIR__.'/inc/community.php';
$version='1.0.0'; $year=date('Y'); $github_url='https://github.com/kladhest-se/lineclash'; $kofi_url='https://ko-fi.com/frossmant';
$community=community_load();
site_header('Multiplayer falling blocks','home','./');
?>
<main id="main">
<section class="hero" aria-labelledby="hero-heading">
  <canvas id="hero-canvas" aria-hidden="true"></canvas>
  <div class="hero-content">
    <div class="hero-eyebrow">v<?= htmlspecialchars($version) ?> &mdash; GPL-3.0 &mdash; Open Source</div>
    <h1 class="hero-title" id="hero-heading">
      <span class="line">Line</span><span class="clash">Clash</span>
    </h1>
    <p class="hero-sub">Fast, brutal multiplayer falling blocks. Up to 8 players, AI bots, 14 specials, full guideline gameplay. Self-hosted.</p>
    <div class="hero-actions">
      <a class="btn btn-primary" href="servers/">Find a server</a>
      <a class="btn btn-outline" href="#build">Get the game</a>
    </div>
  </div>
</section>


<section class="community-shell" aria-labelledby="community-heading">
<div class="section-heading"><div><p class="eyebrow">The community</p><h2 id="community-heading">A good match is never far away.</h2><p>Find a server, follow your rivals, and see how the last round ended.</p></div><a class="section-link" href="statistics/">All statistics →</a></div>
<?php community_metrics($community); if(!community_notice($community)): ?>
<section class="community-section" id="servers"><div class="section-heading"><div><h2>Live servers</h2><p>Your next room is waiting.</p></div><a class="section-link" href="servers/">Browse all servers →</a></div><?php community_servers($community['servers'],6); ?><p class="copy-note">Copy an address and add it to your server list in LineClash.</p></section>
<div class="community-columns"><section id="leaderboards"><div class="section-heading"><div><h2>On the leaderboard</h2><p>Ranked wins. Local rivalries.</p></div><a class="section-link" href="leaderboards/">Full leaderboard →</a></div><?php community_leaders($community['leaders'],'./'); ?></section><section id="matches"><div class="section-heading"><div><h2>Fresh from the arena</h2><p>Results and available replays.</p></div><a class="section-link" href="matches/">All matches →</a></div><?php community_matches($community['matches'],'./'); ?></section></div>
<?php endif; ?>
</section>

<div class="section-divider"></div>

<!-- Features -->
<div class="section" id="features">
  <p class="eyebrow">What you get</p>
  <h2 class="section-title">Built for your next rivalry.</h2>
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
      <p>Configurable difficulty. Bots play with full game rules — garbage targeting, B2B, combos, and all 14 specials.</p>
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
      <p>Run your own server. Packages for macOS, openSUSE Leap, and Ubuntu. Lightweight native clients and a dedicated server.</p>
    </div>
  </div>
  <div class="platform-row">
    <div class="platform-chip"><span class="dot"></span>macOS arm64</div>
    <div class="platform-chip"><span class="dot"></span>openSUSE Leap</div>
    <div class="platform-chip"><span class="dot"></span>Ubuntu 24.04</div>
    <div class="platform-chip"><span class="dot"></span>Any Linux (build from source)</div>
  </div>
</div>

<div class="section-divider"></div>

<!-- Specials -->
<div class="section" id="specials">
  <p class="eyebrow">Combat</p>
  <h2 class="section-title">14 specials. Pick your chaos.</h2>
  <p class="section-sub">Clear lines to earn specials. Fire them at opponents — or yourself when it helps.</p>
  <div class="specials-row">
    <?php
    $specials = [['a','Add Line'],['c','Clear Line'],['b','Clear Specials'],['r','Random Clear'],['o','Block Bomb'],['q','Quake'],['g','Gravity'],['s','Switch Field'],['n','Nuke'],['i','Immunity'],['v','Clear Column'],['m','Mutate'],['d','Darkness'],['f','Confusion']];
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
  <p class="section-sub">Build the native clients and dedicated server with a C compiler, make, SDL2, SQLite and libcurl.</p>

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
<span class="s">sudo zypper</span> install -y gcc make libSDL2-devel sqlite3-devel libcurl-devel

<span class="c"># Clone and build</span>
<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash &amp;&amp; make

<span class="c"># Run</span>
<span class="s">./server/lineclashd</span> &amp;
<span class="s">./clients/gui/lineclash</span></pre>
  </div>
  <div id="tab-ubuntu" class="tab-panel" role="tabpanel">
<pre class="code-block"><span class="c"># Install dependencies</span>
<span class="s">sudo apt</span> install gcc make libsdl2-dev libsqlite3-dev libcurl4-openssl-dev

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
      <a class="source-link" href="<?= htmlspecialchars($github_url) ?>/blob/main/LICENSE.md">GPL-3.0 License</a>
    </div>
  </div>
</div>



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

    if(!window.matchMedia('(prefers-reduced-motion: reduce)').matches) raf = requestAnimationFrame(draw);
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


</main>
<?php site_footer("./"); ?>
