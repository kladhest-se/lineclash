<?php
require_once __DIR__.'/inc/site.php';

site_header('Multiplayer falling blocks','home','./');
?>
<main id="main">
<section class="hero" aria-labelledby="hero-heading">

  <div class="hero-content">
    <div class="hero-eyebrow">Open source &mdash; GPL-3.0-or-later</div>
    <h1 class="hero-title" id="hero-heading">
      <span class="line">Line</span><span class="clash">Clash</span>
    </h1>
    <p class="hero-sub">Fast, brutal multiplayer falling blocks. Up to 8 players, AI bots, 14 specials, full guideline gameplay. Self-hosted.</p>
    <div class="hero-actions">
      <a class="btn btn-primary" href="#get">Get the game</a>
      <a class="btn btn-outline" href="#screenshots">See the game</a>
    </div>
  </div>
</section>

<section class="section" id="about" aria-labelledby="about-heading">
  <p class="eyebrow">About the game</p>
  <h2 class="section-title" id="about-heading">Stack together. Clash with friends.</h2>
  <p class="section-sub">LineClash is a native multiplayer falling-block game. Build clean stacks, clear lines, and send specials to shake up your rivals’ boards. Play in rooms with up to eight players, team up with friends, or practice against AI bots.</p>
  <p>Choose a community server or host your own. Room rules let you shape each session, while spectators, chat and an in-game music library keep the arena lively between rounds.</p>
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
      <p>Run your own server. Build for macOS and Linux. Lightweight native clients and a dedicated server.</p>
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

<section class="section" id="screenshots" aria-labelledby="screenshots-heading">
<p class="eyebrow">Inside LineClash</p><h2 class="section-title" id="screenshots-heading">Your board. Your rivals. One more round.</h2>
<p class="section-sub">A native desktop game with competitive rooms, a server browser, and a soundtrack for the next session. Screenshots from the current development client.</p>
<div class="screenshot-grid">
<figure><a href="assets/screenshots/game.png"><img src="assets/screenshots/game.png" width="1060" height="800" alt="LineClash game view with the playing field, next piece, hold and opponents" loading="lazy"></a><figcaption>The arena · falling blocks and multiplayer rooms</figcaption></figure>
<figure><a href="assets/screenshots/servers.png"><img src="assets/screenshots/servers.png" width="1060" height="800" alt="The native LineClash server browser" loading="lazy"></a><figcaption>Find a room · discover community servers</figcaption></figure>
</div></section>

<div class="section-divider"></div>

<section class="section community-destinations"><a class="feature-card" href="servers/"><h2>Find your next server →</h2><p>Browse live community servers and copy an address to join in the game.</p></a><a class="feature-card" href="statistics/"><h2>Follow the competition →</h2><p>Statistics, ranked leaderboards, match history, player profiles and replays.</p></a></section>

<div class="section-divider"></div>

<section class="section" id="license"><p class="eyebrow">Open source</p><h2 class="section-title">Free software. Make it yours.</h2><p class="section-sub">GPL version 3 or later — free to use, modify, and redistribute under its terms. Music and fonts carry their own licenses.</p><div class="hero-actions"><a class="btn btn-outline" href="source/">Source, license &amp; credits →</a><a class="btn btn-outline" href="documentation/">Read the documentation →</a></div></section>

<div class="section-divider"></div>

<section class="section" id="get" aria-labelledby="get-heading"><p class="eyebrow">Ready when you are</p><h2 class="section-title" id="get-heading">Get the game.</h2>
<p class="section-sub">The latest published GitHub release, with downloads for the platforms available in that release. Drafts and prereleases are excluded.</p>
<div class="download-panel"><p id="release-status" role="status">Checking the latest release…</p><div id="release-assets" class="release-assets"></div><div class="download-actions"><a class="btn btn-outline" href="https://github.com/kladhest-se/lineclash/releases">All GitHub releases ↗</a><a class="btn btn-outline" href="#build">Build from source</a><button type="button" class="copy-button" id="release-retry" hidden>Try again</button></div><noscript><p>Enable JavaScript to check downloads automatically, or visit GitHub releases. You can also build from source below.</p></noscript></div>
<p class="small muted">Native packages may require SDL2 and other runtime libraries. Check the release notes for platform and installation requirements.</p></section>

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

</script>


<script src="assets/downloads.js" defer></script>
</main>
<?php site_footer("./"); ?>
