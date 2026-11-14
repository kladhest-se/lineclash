(() => {
    const token = location.hash.slice(1);
    if (location.hash) history.replaceState(null, '', location.pathname);
    const field = document.getElementById('email-change-token');
    if (!field || !/^[a-f0-9]{64}$/.test(token)) return;
    field.value = token;
    document.getElementById('confirm-email-change').focus();
})();
