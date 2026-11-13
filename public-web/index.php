<?php
require_once __DIR__.'/inc/site.php';


function home_preview(string $image,string $caption): void { ?>
<a class="screenshot-thumb" href="assets/screenshots/<?=site_h($image)?>.png" data-screenshot data-caption="<?=site_h($caption)?>" aria-label="Enlarge: <?=site_h($caption)?>"><img src="assets/screenshots/thumbs/<?=site_h($image)?>.png" width="180" height="136" alt="<?=site_h($caption)?>" loading="lazy"><span><?=site_h($caption)?></span></a>
<?php }

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
    <p class="hero-kicker">Your board. Your rivals. Your rules.</p>
    <div class="hero-actions">
      <a class="btn btn-primary" href="#get">Get the game</a>
      <a class="btn btn-outline" href="#features">See the game</a>
    </div>
  </div>
</section>
<nav class="home-index" aria-label="On this page"><a href="#about">The game <span>01</span></a><a href="#features">Features <span>02</span></a><a href="#screenshots">Soundtrack <span>03</span></a><a href="#get">Downloads <span>04</span></a></nav>

<section class="section" id="about" aria-labelledby="about-heading">
  <p class="eyebrow">The game</p>
  <h2 class="section-title" id="about-heading">Stack together. Clash with friends.</h2>
  <p class="section-sub">LineClash is a native multiplayer falling-block game. Build clean stacks, clear lines, and send specials to shake up your rivals’ boards. Play in rooms with up to eight players, team up with friends, or practice against AI bots.</p>
  <p>Choose a community server or host your own. Room rules let you shape each session, while spectators, chat and an in-game music library keep the arena lively between rounds.</p>
</section>

<div class="section-divider"></div>

<!-- Features -->
<div class="section" id="features">
  <p class="eyebrow">Made for play</p>
  <h2 class="section-title">Built for your next rivalry.</h2>
  <p class="section-sub">From a quick practice session to a room full of rivals. Click a preview to take a closer look.</p>
  <div class="features-grid">
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">01</span>
      <h3>Real-time multiplayer</h3>
      <p>Up to 8 players per room. Spectator mode. Team play. Reconnect window keeps your slot when you drop.</p>
      <div class="feature-previews"><?php home_preview('arena','Multiplayer arena'); ?><?php home_preview('rooms','Choose a room'); ?></div>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">02</span>
      <h3>Guideline gameplay</h3>
      <p>SRS wall kicks, hold piece, lock delay, back-to-back bonus, combos, and perfect clear. All configurable per room.</p>
      <div class="feature-previews"><?php home_preview('arena','Hold, next piece & boards'); ?><?php home_preview('rules','Room rules'); ?></div>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">03</span>
      <h3>AI bots</h3>
      <p>Configurable difficulty. Bots play with full game rules — garbage targeting, B2B, combos, and all 14 specials.</p>
      <div class="feature-previews"><?php home_preview('bots','AI difficulty controls'); ?><?php home_preview('arena','Practice in the arena'); ?></div>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">04</span>
      <h3>Persistent rooms</h3>
      <p>Room settings saved to SQLite and restored on restart. Leaderboard and stats HTTP API included.</p>
      <div class="feature-previews"><?php home_preview('rooms','Room browser'); ?><?php home_preview('rules','Configure a room'); ?></div>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">05</span>
      <h3>Open source. Multiple platforms.</h3>
      <p>Free software built for macOS and Linux. Explore the source, customize the game, and run it on your preferred platform.</p>
      <div class="feature-previews"><?php home_preview('arena','Native desktop game'); ?><?php home_preview('servers','Desktop server browser'); ?></div>
    </article>
    <article class="feature-item">
      <span class="feature-number" aria-hidden="true">06</span>
      <h3>Self-hosted</h3>
      <p>Run your own server. Build for macOS and Linux. Lightweight native clients and a dedicated server.</p>
      <div class="feature-previews"><?php home_preview('servers','Community servers'); ?><?php home_preview('rooms','Hosted rooms'); ?></div>
    </article>
  </div>
  <p class="preview-note">Native client previews with example players and rooms. Each board uses its own piece sequence.</p>
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
  <p class="eyebrow">Change the round</p>
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
<p class="eyebrow">Between rounds</p><h2 class="section-title" id="screenshots-heading">Find your rhythm.</h2>
<p class="section-sub">An in-game music library for long sessions, with track controls and credits close at hand. Explore the player and the people behind the soundtrack.</p>
<div class="screenshot-grid compact-gallery">
<?php home_preview('music','In-game music player'); home_preview('credits','Soundtrack credits'); ?>
</div></section>

<div class="section-divider"></div>

<section class="section" id="license"><p class="eyebrow">Yours to explore</p><h2 class="section-title">Free software. Make it yours.</h2><p class="section-sub">GPL version 3 or later — free to use, modify, and redistribute under its terms. Music and fonts carry their own licenses.</p><div class="hero-actions"><a class="btn btn-outline" href="source/">Source, license &amp; credits →</a><a class="btn btn-outline" href="documentation/">Read the documentation →</a></div></section>

<div class="section-divider"></div>

<section class="section" id="get" aria-labelledby="get-heading"><p class="eyebrow">Join the next round</p><h2 class="section-title" id="get-heading">Get the game.</h2>
<p class="section-sub">The latest published GitHub release, with downloads for the platforms available in that release. Drafts and prereleases are excluded.</p>
<div class="download-panel"><p id="release-status" role="status">Checking the latest release…</p><div id="release-assets" class="release-assets"></div><div class="download-actions"><a class="btn btn-outline" href="https://github.com/kladhest-se/lineclash/releases">All GitHub releases ↗</a><a class="btn btn-outline" href="source/#build">Build from source</a><button type="button" class="copy-button" id="release-retry" hidden>Try again</button></div><noscript><p>Enable JavaScript to check downloads automatically, or visit GitHub releases. Build instructions are available on the Source page.</p></noscript></div>
<p class="small muted">Native packages may require SDL2 and other runtime libraries. Check the release notes for platform and installation requirements.</p></section>

<dialog class="screenshot-viewer" aria-labelledby="screenshot-caption"><div class="viewer-toolbar"><p id="screenshot-caption"></p><button type="button" class="copy-button" data-viewer-zoom aria-pressed="false">Actual size</button><a class="section-link" data-viewer-original target="_blank" rel="noopener">Open original ↗</a><button type="button" class="copy-button" data-viewer-close aria-label="Close screenshot">Close ×</button></div><div class="viewer-stage"><img alt=""></div></dialog>
<script src="assets/screenshots.js" defer></script>
<script src="assets/downloads.js" defer></script>
</main>
<?php site_footer("./"); ?>
