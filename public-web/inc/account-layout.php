<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/site.php';
function account_intro(): void { ?>
<p class="account-intro">One account for public servers, your player identity and match statistics. Private servers can use their own local accounts.</p>
<?php }
function account_navigation(string $base='./'): void { if(site_account_user())return; ?>
<nav class="account-links" aria-label="Account help">
<a href="<?=site_h($base)?>?mode=register">Create an account</a>
<a href="<?=site_h($base)?>">Sign in</a>
<a href="<?=site_h($base)?>?mode=activation">Resend activation email</a>
<a href="<?=site_h($base)?>?mode=recover">Forgot password?</a>
</nav>
<?php }
