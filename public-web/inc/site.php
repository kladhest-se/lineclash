<?php
declare(strict_types=1);
require_once __DIR__.'/service.php';
function site_h(mixed $value): string { return htmlspecialchars((string)$value,ENT_QUOTES|ENT_SUBSTITUTE,'UTF-8'); }
function site_header(string $title,string $active='home',string $base='../'): void {
    header('Cache-Control: no-store');
    $links=['home'=>'Home','servers'=>'Servers','statistics'=>'Statistics','documentation'=>'Documentation','source'=>'Source','account'=>'Account'];
    if(in_array($active,['leaderboards','matches','directory'],true))$active='statistics';
?><!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title><?=site_h($title)?> — LineClash</title><meta name="description" content="Multiplayer falling blocks. Find a server, follow the leaderboards, and replay the latest matches.">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="<?=site_h($base)?>assets/site.css"></head><body><canvas id="block-background" aria-hidden="true"></canvas>
<a class="skip-link" href="#main">Skip to content</a>
<nav class="nav" aria-label="Main"><div class="nav-inner">
<a class="nav-logo" href="<?=site_h($base)?>">Line<span class="clash">Clash</span></a>
<ul class="nav-links"><?php foreach($links as $key=>$label): ?><li><a href="<?=site_h($base.($key==='home'?'':$key.'/'))?>"<?=$active===$key?' aria-current="page"':''?>><?=site_h($label)?></a></li><?php endforeach; ?></ul>
<a class="nav-btn" href="<?=site_h($base)?>#get">Get the game</a>
</div></nav>
<?php }
function site_footer(string $base='../'): void { ?>
<footer><div class="footer-inner"><a class="footer-logo" href="<?=site_h($base)?>">Line<span class="clash">Clash</span></a><ul class="footer-links" aria-label="Footer"><li><a href="<?=site_h($base)?>credits/">Credits</a></li><li><a href="<?=site_h($base)?>privacy/">Privacy</a></li><li><a href="<?=site_h($base)?>api/docs/">API</a></li><li><a href="https://github.com/kladhest-se/lineclash">GitHub</a></li></ul><span class="footer-copy">&copy; <?=date('Y')?> Tommy Frössman</span></div></footer>
<script src="<?=site_h($base)?>assets/blocks.js" defer></script>
<script src="<?=site_h($base)?>assets/community.js" defer></script>
</body></html>
<?php }

function documentation_tabs(string $active='overview'): void {
    $guides=['overview'=>'Overview','basics'=>'Gameplay','controls'=>'Clients & controls','server'=>'Server setup'];
    $base=$active==='overview'?'./':'../';
?><nav class="documentation-tabs" aria-label="Documentation guides"><?php foreach($guides as $key=>$label): ?><a href="<?=site_h($base.($key==='overview'?'':$key.'/'))?>"<?=$active===$key?' aria-current="page"':''?>><?=site_h($label)?></a><?php endforeach; ?></nav><?php } ?>
