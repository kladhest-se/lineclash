<?php
require_once __DIR__.'/../inc/site.php';
site_header('Source','source');
?><main id="main" class="community-shell"><p class="eyebrow">Source</p><h1>Open source. Built to be shared.</h1><p class="page-intro">Explore the native clients, dedicated server and community website. Report a bug, improve the documentation, or build your own arena.</p>
<section class="community-section"><h2>Source repository</h2><div class="source-grid"><article class="source-card"><h3>GitHub</h3><p>Browse the code, report issues, contribute changes, and find published releases.</p><a class="source-link" href="https://github.com/kladhest-se/lineclash">Open GitHub ↗</a></article><article class="source-card"><h3>Issues & contributions</h3><p>Include your platform, steps to reproduce and relevant logs when reporting a bug. Remove passwords and tokens from logs.</p><a class="source-link" href="https://github.com/kladhest-se/lineclash/issues">Browse issues ↗</a></article></div></section>
<section class="community-section"><h2>Get the source</h2><pre class="code-block">git clone https://github.com/kladhest-se/lineclash.git
cd lineclash</pre><p>See <a href="#build">the build instructions below</a> for platform dependencies, or read the <a href="../documentation/#development">development guide</a>. The main branch is the development version; published release packages appear in <a href="../#get">Get the game on Home</a>.</p></section>
<!-- Build -->
<div class="community-section" id="build">
  <p class="eyebrow">Get started</p>
  <h2 class="section-title">Build from source.</h2>
  <p class="section-sub">Build the native clients and dedicated server with a C compiler, make, SDL2, SQLite, and libcurl on macOS/Linux. Windows uses WinHTTP.</p>

  <div class="tab-bar" role="tablist">
    <button class="tab-btn active" role="tab" aria-selected="true"  aria-controls="tab-macos"   onclick="showTab(this,'macos')">macOS</button>
    <button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-linux" onclick="showTab(this,'linux')">Linux</button>
    <button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-windows" onclick="showTab(this,'windows')">Windows</button>
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
  <div id="tab-linux" class="tab-panel" role="tabpanel">
    <p>Install the dependencies for your distribution, then use the shared build and launch commands below.</p>
    <h3>openSUSE</h3>
<pre class="code-block"><span class="s">sudo zypper</span> install -y gcc make git libSDL2-devel sqlite3-devel libcurl-devel</pre>
    <h3>Ubuntu / Debian</h3>
<pre class="code-block"><span class="s">sudo apt</span> install gcc make git libsdl2-dev libsqlite3-dev libcurl4-openssl-dev</pre>
    <h3>Build and run</h3>
<pre class="code-block"><span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash &amp;&amp; make

<span class="c"># Run a local server and launch the game</span>
<span class="s">./server/lineclashd</span> &amp;
<span class="s">./clients/gui/lineclash</span></pre>
  </div>

<div id="tab-windows" class="tab-panel" role="tabpanel">
<pre class="code-block"># Install Windows build tools and libraries
pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-python

git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
make -j4

# Launch the native game
./clients/gui/lineclash.exe

# Optional dedicated server (run in another terminal)
./server/lineclashd.exe

# Verify on Windows, then create a portable ZIP
make verify
python scripts/package-windows.py</pre>
<p>The server keeps its configuration, logs and database in <code>%APPDATA%\LineClash\Server</code>. The ZIP bundles the needed runtime DLLs and music; extract it and launch <code>bin/lineclash.exe</code>.</p>
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
<section class="community-section"><h2>License &amp; credits</h2><div class="guide-panel"><p>LineClash’s game code is licensed under the GNU General Public License, version 3 or any later version. You may use, modify and redistribute it under those terms. The software is provided without warranty.</p><p>Music and fonts have their own licenses and attribution requirements. Review the credits when redistributing the game or its assets.</p><p><a href="https://github.com/kladhest-se/lineclash/blob/main/LICENSE.md">Read the full license ↗</a> · <a href="https://github.com/kladhest-se/lineclash/blob/main/CREDITS.md">Music and font credits ↗</a></p><p>Created by <a href="https://github.com/frossmant">Tommy Frössman</a>. <a href="https://ko-fi.com/frossmant">Support development on Ko-fi ↗</a></p></div></section></main><?php site_footer(); ?>
