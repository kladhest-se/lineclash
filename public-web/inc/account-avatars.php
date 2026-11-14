<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
function account_avatars(): array {
    return [
        'initial'=>['Initial',[], 'teal'],
        'i'=>['Straight',[[0,0],[1,0],[2,0],[3,0]],'sky'],
        'j'=>['Hook',[[0,0],[0,1],[1,1],[2,1]],'blue'],
        'l'=>['Corner',[[2,0],[0,1],[1,1],[2,1]],'peach'],
        'o'=>['Square',[[0,0],[1,0],[0,1],[1,1]],'yellow'],
        's'=>['Zig',[[1,0],[2,0],[0,1],[1,1]],'green'],
        't'=>['Tee',[[0,0],[1,0],[2,0],[1,1]],'mauve'],
        'z'=>['Zag',[[0,0],[1,0],[1,1],[2,1]],'red'],
    ];
}
function account_avatar_markup(string $avatar,string $handle): string {
    $choices=account_avatars();[$label,$cells,$color]=$choices[$avatar]??$choices['initial'];
    if(!$cells)return '<span class="avatar-initial">'.htmlspecialchars(strtoupper(substr($handle,0,1)),ENT_QUOTES|ENT_SUBSTITUTE,'UTF-8').'</span>';
    $width=max(array_column($cells,0))+1;$height=max(array_column($cells,1))+1;
    $svg='<svg viewBox="0 0 44 44" class="avatar-blocks" style="color:var(--'.$color.')" aria-hidden="true" focusable="false">';
    foreach($cells as $i=>[$x,$y])$svg.='<rect x="'.((44-($width*10-2))/2+$x*10).'" y="'.((44-($height*10-2))/2+$y*10).'" width="8" height="8" rx="1.5" fill="currentColor" opacity="'.($i===1?'1':'.75').'"/>';
    return $svg.'</svg>';
}
