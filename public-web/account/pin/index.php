<?php
// Older bookmarks lead to the same hidden flow, without carrying POST credentials.
header('X-Robots-Tag: noindex, nofollow');header('Cache-Control: no-store');header('Location: ../../pin/',true,303);exit;
