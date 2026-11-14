// Verification is an explicit POST; mail scanners cannot activate accounts with GET.
(() => {
    const token = location.hash.slice(1);
    if (location.hash) history.replaceState(null, '', location.pathname);
    if (!/^[a-f0-9]{64}$/.test(token)) return;
    const field = document.getElementById('verification-token');
    if (!field) return;
    field.value = token;
    document.getElementById('verification-request').hidden = true;
    document.getElementById('verification-confirm').hidden = false;
    document.getElementById('verify-email').focus();
})();
