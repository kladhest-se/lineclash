<?php
declare(strict_types=1);
require_once __DIR__.'/community.php';
$titles=['servers'=>'Find your next server.','leaderboards'=>'Every win leaves a mark.','matches'=>'Fresh from the arena.','statistics'=>'The community, in numbers.','directory'=>'The LineClash community.'];
$labels=['servers'=>'Server browser','leaderboards'=>'Leaderboards','matches'=>'Match history','statistics'=>'Statistics','directory'=>'Community'];
$descriptions=['servers'=>'Find a room, copy its address, and join the next round in LineClash.','leaderboards'=>'Ranked wins from completed competitive games. Nicknames and rankings belong to each server.','matches'=>'Follow recent results, explore player statistics, and watch the available gameplay replays.','statistics'=>'A shared view of reported games, players and activity across community servers.','directory'=>'Servers, ranked wins and recent matches, all in one place.'];
$data=community_load($page);if(!$data['available'])http_response_code($data['invalid']?400:503);
site_header($labels[$page],$page);
?><main id="main" class="community-shell"><p class="eyebrow"><?=site_h($labels[$page])?></p><h1><?=site_h($titles[$page])?></h1><p class="page-intro"><?=site_h($descriptions[$page])?></p>
<?php if(!community_notice($data)):
if($page==='servers'):
$servers=array_values(array_filter($data['servers'],function($s)use($data){
    if($data['q']!=='' && stripos($s['name'].' '.$s['host'],$data['q'])===false)return false;
    if($data['availability']==='open') { $capacity=(int)($s['status']['capacity']??0);return $capacity>0 && isset($s['status']['players']) && (int)$s['status']['players']<$capacity; }
    return true;
})); ?>
<form class="filters" method="get"><label>Search servers<input type="search" name="q" maxlength="128" value="<?=site_h($data['q'])?>" placeholder="Server name or address"></label><label>Availability<select name="availability"><option value="all">All live servers</option><option value="open"<?=$data['availability']==='open'?' selected':''?>>Open slots</option></select></label><button class="filter-button" type="submit">Find servers</button><a href="./">Reset</a></form>
<p class="server-count"><?=count($servers)?> live <?=count($servers)===1?'server':'servers'?> in this view</p>
<?php community_servers($servers); ?><p class="copy-note">Copy an address, open LineClash, and add it to your server list. Listings and populations are reported by server operators.</p>
<?php elseif($page==='leaderboards'): ?>
<form class="filters" method="get"><?php community_server_select($data); ?><button class="filter-button" type="submit">Show leaderboard</button><a href="./">Reset</a></form>
<?php community_leaders($data['leaders'],'../',$data['offset']);community_pagination($data,$data['more_leaders']); ?>
<p class="small muted">Only eligible ranked rounds award leaderboard wins. Solo/bot games and manually stopped rounds remain available in <a href="../matches/">match history</a>. Results are server-reported.</p>
<?php elseif($page==='matches'): ?>
<form class="filters" method="get"><?php community_server_select($data); ?><label>Mode<select name="mode"><?php foreach(['all'=>'All matches','ranked'=>'Ranked','casual'=>'Casual'] as $key=>$label): ?><option value="<?=site_h($key)?>"<?=$data['mode']===$key?' selected':''?>><?=site_h($label)?></option><?php endforeach; ?></select></label><button class="filter-button" type="submit">Show matches</button><a href="./">Reset</a></form>
<?php community_matches($data['matches']);community_pagination($data,$data['more_matches']); ?>
<?php elseif($page==='statistics'): community_metrics($data); ?>
<section class="community-section"><div class="section-heading"><div><h2>Across the arena</h2><p>Human player records from uploaded match results.</p></div></div><div class="stats-grid"><?php $metrics=['cleared_lines'=>'Lines cleared','specials'=>'Specials sent','blocks'=>'Blocks placed','profiles'=>'Server-scoped profiles'];foreach($metrics as $key=>$label): ?><div class="metric"><strong><?=number_format((int)$data['totals'][$key])?></strong><span><?=site_h($label)?></span></div><?php endforeach; ?><div class="metric"><strong><?=number_format((int)$data['metrics']['games']-(int)$data['metrics']['ranked_games'])?></strong><span>Casual matches</span></div><div class="metric"><strong><?=$data['totals']['average_duration']!==null?number_format((float)$data['totals']['average_duration']/60,1).'m':'—'?></strong><span>Average match duration</span></div></div></section>
<section class="community-section"><div class="section-heading"><div><h2>Server activity</h2><p>Lifetime history, including servers currently offline.</p></div></div><?php if(!$data['server_totals']):community_empty('The community is just getting started.','Server activity will appear as operators register and matches are reported.');else: ?><div class="data-panel table-scroll"><table class="community-table"><thead><tr><th scope="col">Server</th><th scope="col">Matches</th><th scope="col">Ranked</th></tr></thead><tbody><?php foreach($data['server_totals'] as $server): ?><tr><td><a href="../matches/?<?=site_h(http_build_query(['server'=>$server['id']]))?>"><?=site_h($server['name'])?></a></td><td><?=number_format((int)$server['games'])?></td><td><?=number_format((int)$server['ranked_games'])?></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section>
<section class="community-section" id="leaderboards"><div class="section-heading"><div><h2>Leaderboards</h2><p>Ranked wins in completed competitive matches. Rankings and nicknames belong to each server.</p></div><a href="../leaderboards/">Filter and browse all rankings →</a></div><?php community_leaders($data['leaders']); ?></section>
<section class="community-section" id="matches"><div class="section-heading"><div><h2>Match history</h2><p>Recent results, player profiles and available replays.</p></div><a href="../matches/">Filter and browse all matches →</a></div><?php community_matches($data['matches']); ?></section>
<p class="small muted">New match times use UTC. Duration averages include only matches with known UTC times; legacy history is retained. Profiles are local to a server and do not imply a shared identity across servers.</p>
<?php else: community_metrics($data); ?>
<section class="community-section" id="servers"><div class="section-heading"><h2>Live servers</h2><a href="../servers/">Browse servers →</a></div><?php community_servers($data['servers'],6); ?></section>
<section class="community-section" id="matches"><div class="section-heading"><h2>Recent matches</h2><a href="../matches/">All matches →</a></div><?php community_matches($data['matches']); ?></section>
<section class="community-section" id="leaderboards"><div class="section-heading"><h2>Ranked wins</h2><a href="../leaderboards/">Full leaderboard →</a></div><?php community_leaders($data['leaders']); ?></section>
<?php endif;endif; ?>
<section class="community-section community-cta"><div><h2>Ready to play?</h2><p>Get the native game and join a community room.</p></div><a class="btn btn-primary" href="../#get">Get LineClash</a></section>
</main><?php site_footer(); ?>
