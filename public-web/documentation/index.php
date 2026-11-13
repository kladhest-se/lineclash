<?php require_once __DIR__.'/../inc/site.php'; site_header('Documentation','documentation'); ?>
<main id="main" class="documentation-shell documentation-overview"><header class="documentation-intro"><p class="eyebrow">LineClash / Guides</p><h1>Documentation</h1><p class="page-intro">Guides for players and server operators.</p></header>
<?php documentation_tabs(); ?>
<div class="documentation-cards">
<?php
$guides=[
 ['basics/','Gameplay','Rules, game modes and specials.'],
 ['controls/','Clients & controls','Install, connect and configure your client.'],
 ['server/','Server setup','Run, configure and manage your server.'],
];
foreach($guides as [$href,$title,$description]): ?>
<a class="documentation-card" href="<?=site_h($href)?>"><h2><?=site_h($title)?> <span aria-hidden="true">↗</span></h2><p><?=site_h($description)?></p></a>
<?php endforeach; ?>
</div></main><?php site_footer(); ?>
