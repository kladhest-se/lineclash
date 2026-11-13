<?php
require_once __DIR__.'/inc/site.php';

site_header('Multiplayer falling blocks','home','./');
?>
<main id="main" class="home-page">
<section class="hero home-hero" aria-labelledby="hero-heading">

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
  <figure class="hero-scene"><a href="#screenshots"><img src="assets/screenshots/game.png" width="1060" height="800" alt="Preview of the native LineClash game" fetchpriority="high"></a><figcaption>Native multiplayer falling blocks · your next rivalry starts here</figcaption></figure>
</section>

<section class="section" id="about" aria-labelledby="about-heading">
  <p class="eyebrow">01 / About the game</p>
  <h2 class="section-title" id="about-heading">Stack together. Clash with friends.</h2>
  <p class="section-sub">LineClash is a native multiplayer falling-block game. Build clean stacks, clear lines, and send specials to shake up your rivals’ boards. Play in rooms with up to eight players, team up with friends, or practice against AI bots.</p>
  <p>Choose a community server or host your own. Room rules let you shape each session, while spectators, chat and an in-game music library keep the arena lively between rounds.</p>
</section>

<div class="section-divider"></div>

<!-- Features -->
<div class="section" id="features">
  <p class="eyebrow">02 / Made for play</p>
  <h2 class="section-title">Built for your next rivalry.</h2>
  <p class="section-sub">Real-time multiplayer with the depth of guideline play and the chaos of specials.</p>
  <div class="features-grid">
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">01</span>
      <h3>Real-time multiplayer</h3>
      <p>Up to 8 players per room. Spectator mode. Team play. Reconnect window keeps your slot when you drop.</p>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">02</span>
      <h3>Guideline gameplay</h3>
      <p>SRS wall kicks, hold piece, lock delay, back-to-back bonus, combos, and perfect clear. All configurable per room.</p>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">03</span>
      <h3>AI bots</h3>
      <p>Configurable difficulty. Bots play with full game rules — garbage targeting, B2B, combos, and all 14 specials.</p>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">04</span>
      <h3>Persistent rooms</h3>
      <p>Room settings saved to SQLite and restored on restart. Leaderboard and stats HTTP API included.</p>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">05</span>
      <h3>Two clients</h3>
      <p>SDL2 GUI client with HiDPI/Retina support. Terminal CLI client for headless or SSH play.</p>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">06</span>
      <h3>Self-hosted</h3>
      <p>Run your own server. Build for macOS and Linux. Lightweight native clients and a dedicated server.</p>
    </article>
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
  <p class="eyebrow">03 / Change the round</p>
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
<p class="eyebrow">04 / Inside LineClash</p><h2 class="section-title" id="screenshots-heading">Your board. Your rivals. One more round.</h2>
<p class="section-sub">A native desktop game with competitive rooms, a server browser, and a soundtrack for the next session. Screenshots from the current development client.</p>
<div class="screenshot-grid">
<figure><a href="assets/screenshots/game.png"><img src="assets/screenshots/game.png" width="1060" height="800" alt="LineClash game view with the playing field, next piece, hold and opponents" loading="lazy"></a><figcaption>The arena · falling blocks and multiplayer rooms</figcaption></figure>
<figure><a href="assets/screenshots/servers.png"><img src="assets/screenshots/servers.png" width="1060" height="800" alt="The native LineClash server browser" loading="lazy"></a><figcaption>Find a room · discover community servers</figcaption></figure>
</div></section>

<div class="section-divider"></div>

<section class="section" id="license"><p class="eyebrow">05 / Open source</p><h2 class="section-title">Free software. Make it yours.</h2><p class="section-sub">GPL version 3 or later — free to use, modify, and redistribute under its terms. Music and fonts carry their own licenses.</p><div class="hero-actions"><a class="btn btn-outline" href="source/">Source, license &amp; credits →</a><a class="btn btn-outline" href="documentation/">Read the documentation →</a></div></section>

<div class="section-divider"></div>

<section class="section" id="get" aria-labelledby="get-heading"><p class="eyebrow">06 / Ready when you are</p><h2 class="section-title" id="get-heading">Get the game.</h2>
<p class="section-sub">The latest published GitHub release, with downloads for the platforms available in that release. Drafts and prereleases are excluded.</p>
<div class="download-panel"><p id="release-status" role="status">Checking the latest release…</p><div id="release-assets" class="release-assets"></div><div class="download-actions"><a class="btn btn-outline" href="https://github.com/kladhest-se/lineclash/releases">All GitHub releases ↗</a><a class="btn btn-outline" href="source/#build">Build from source</a><button type="button" class="copy-button" id="release-retry" hidden>Try again</button></div><noscript><p>Enable JavaScript to check downloads automatically, or visit GitHub releases. Build instructions are available on the Source page.</p></noscript></div>
<p class="small muted">Native packages may require SDL2 and other runtime libraries. Check the release notes for platform and installation requirements.</p></section>

<script src="assets/downloads.js" defer></script>
</main>
<?php site_footer("./"); ?>
