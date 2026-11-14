// Reset tokens stay in the page fragment until moved to the POST form.
(() => {
    const token = location.hash.slice(1);
    if (location.hash) history.replaceState(null, '', location.pathname);
    if (!/^[a-f0-9]{64}$/.test(token)) return;
    const field = document.getElementById('reset-token');
    if (!field) return;
    field.value = token;
    document.getElementById('recovery-request').hidden = true;
    document.getElementById('recovery-reset').hidden = false;
    document.getElementById('reset-password').focus();
})();
