// Opening the email link submits its private fragment through a CSRF-protected POST.
(() => {
    const token = location.hash.slice(1);
    if (location.hash) history.replaceState(null, '', location.pathname + (location.search || ''));
    if (!/^[a-f0-9]{64}$/.test(token)) return;
    const field = document.getElementById('verification-token');
    if (!field) return;
    field.value = token;
    document.getElementById('verification-request').hidden = true;
    document.getElementById('verification-confirm').hidden = false;
    document.getElementById('verification-form').requestSubmit();
})();
