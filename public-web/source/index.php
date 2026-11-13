<?php
require_once __DIR__.'/../inc/site.php';
site_header('Source','source');
?><main id="main" class="community-shell"><p class="eyebrow">Source</p><h1>Open source. Built to be shared.</h1><p class="page-intro">Explore the native clients, dedicated server and community website. Report a bug, improve the documentation, or build your own arena.</p>
<section class="community-section"><h2>Source repository</h2><div class="source-grid"><article class="source-card"><h3>GitHub</h3><p>Browse the code, report issues, contribute changes, and find published releases.</p><a class="source-link" href="https://github.com/kladhest-se/lineclash">Open GitHub ↗</a></article><article class="source-card"><h3>Issues & contributions</h3><p>Include your platform, steps to reproduce and relevant logs when reporting a bug. Remove passwords and tokens from logs.</p><a class="source-link" href="https://github.com/kladhest-se/lineclash/issues">Browse issues ↗</a></article></div></section>
<section class="community-section"><h2>Get the source</h2><pre class="code-block">git clone https://github.com/kladhest-se/lineclash.git
cd lineclash</pre></section>
<section class="community-section" id="build-server">
<p class="eyebrow">Dedicated server</p><h2 class="section-title">Build the server.</h2>
<div class="tab-bar" role="tablist" aria-label="Server build platform">
<button class="tab-btn active" role="tab" aria-selected="true" aria-controls="tab-server-macos" onclick="showTab(this,'server-macos')">macOS</button>
<button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-server-linux" onclick="showTab(this,'server-linux')">Linux</button>
<button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-server-windows" onclick="showTab(this,'server-windows')">Windows</button>
</div>
<div id="tab-server-macos" class="tab-panel active" role="tabpanel">
<pre class="code-block"># Install Apple command-line tools (once)
xcode-select --install

# Build and run from the project root
git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
# If you already cloned the project, use that checkout instead.
make -j4 server

cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg</pre>
</div>
<div id="tab-server-linux" class="tab-panel" role="tabpanel">
<pre class="code-block">sudo zypper install -y gcc make git sqlite3-devel libcurl-devel

git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
# If you already cloned the project, use that checkout instead.
make -j4 server

cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg</pre>
</div>
<div id="tab-server-windows" class="tab-panel" role="tabpanel">
<pre class="code-block"># In PowerShell
winget install --exact --id MSYS2.MSYS2

# Open MSYS2 UCRT64 from the Start menu
pacman -Syu
# If asked to close the terminal, reopen UCRT64 and repeat the update.

pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3

git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
# If you already cloned the project, use that checkout instead.
make -j4 server

./server/lineclashd.exe

# Administration tool (in another terminal)
./server/lineclashd-tool.exe --help</pre>
</div>
</section>

<!-- Build -->
<div class="community-section" id="build">
  <p class="eyebrow">Get started</p>
  <h2 class="section-title">Build the clients.</h2>

  <div class="tab-bar" role="tablist" aria-label="Client build platform">
    <button class="tab-btn active" role="tab" aria-selected="true"  aria-controls="tab-macos"   onclick="showTab(this,'macos')">macOS</button>
    <button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-linux" onclick="showTab(this,'linux')">Linux</button>
    <button class="tab-btn" role="tab" aria-selected="false" aria-controls="tab-windows" onclick="showTab(this,'windows')">Windows</button>
  </div>

  <div id="tab-macos" class="tab-panel active" role="tabpanel">
<pre class="code-block"><span class="c"># Install Apple command-line tools (once)</span>
xcode-select --install

<span class="c"># Install libraries with Homebrew</span>
<span class="s">brew</span> install sdl2 curl

<span class="c"># Clone and build</span>
<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash

<span class="c"># Build the desktop client</span>
make -j4 client
<span class="c"># Launch the desktop client</span>
./clients/gui/lineclash</pre>
  </div>
  <div id="tab-linux" class="tab-panel" role="tabpanel">
<pre class="code-block"><span class="s">sudo zypper</span> install -y gcc make git libSDL2-devel libcurl-devel

<span class="s">git</span> clone https://github.com/kladhest-se/lineclash.git
<span class="s">cd</span> lineclash

<span class="c"># Desktop client (SDL2)</span>
make -j4 client

<span class="c"># Launch the desktop client</span>
./clients/gui/lineclash</pre>
  </div>

<div id="tab-windows" class="tab-panel" role="tabpanel">
<pre class="code-block"># In PowerShell
winget install --exact --id MSYS2.MSYS2

# Open MSYS2 UCRT64 from the Start menu
pacman -Syu
# If asked to close the terminal, reopen UCRT64 and repeat the update.

pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2

git clone https://github.com/kladhest-se/lineclash.git
cd lineclash

make -j4 client

./clients/gui/lineclash.exe
</pre>

</div>
</div>


<section class="community-section"><h2>AmigaOS 3.x preview</h2><p>The native 68020 client uses Intuition, bsdsocket.library and AmiSSL for HTTPS directory discovery. No SDL or FPU is required. This is an experimental build; audio playback is not implemented.</p>
<pre class="code-block">make amiga
# A different installed cross-toolchain:
make -C clients/amiga AMIGA_TOOLS=/path/to/m68k-amigaos-toolchain
</pre><p>Run <code>clients/amiga/LineClash</code> on AmigaOS 3.x with a running Roadshow, AmiTCP or Miami stack. For public discovery, install AmiSSL 4.10 or newer with its certificates and assigns. Keep the executable named <code>LineClash</code> so its directory worker can launch.</p></section>

<script>
/* ── Tab switching ─────────────────────────────────────── */
function showTab(btn, id) {
  const section=btn.closest('.community-section');
  section.querySelectorAll('.tab-btn').forEach(b => {
    b.classList.remove('active');
    b.setAttribute('aria-selected', 'false');
  });
  section.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  btn.classList.add('active');
  btn.setAttribute('aria-selected', 'true');
  document.getElementById('tab-' + id).classList.add('active');
}

</script>
</main><?php site_footer(); ?>
