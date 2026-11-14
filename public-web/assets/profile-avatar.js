// Save avatar choices in order, retaining the latest choice during a pending request.
(() => {
    const form = document.querySelector('.profile-avatar-form');
    if (!form) return;
    const inputs = Array.from(form.querySelectorAll('input[name="avatar"]'));
    const status = form.querySelector('[data-avatar-status]');
    let saved = inputs.find(input => input.checked)?.value;
    let pending = null;
    let running = false;
    const message = (text, error = false) => {
        status.textContent = text;
        status.dataset.state = error ? 'error' : 'saved';
    };
    const showAvatar = value => {
        const input = inputs.find(input => input.value === value);
        const icon = input.closest('label').querySelector('.avatar-choice-icon');
        document.querySelectorAll('[data-avatar-current]').forEach(preview => {
            preview.replaceChildren(...Array.from(icon.childNodes, child => child.cloneNode(true)));
        });
    };
    const save = async () => {
        if (running) return;
        running = true;
        while (pending !== null) {
            const value = pending;
            pending = null;
            message('Saving…');
            const body = new URLSearchParams(new FormData(form));
            body.set('avatar', value);
            body.set('format', 'json');
            let failure = 'Could not save your avatar. Please try again.';
            try {
                const response = await fetch(form.action, {
                    method: 'POST', credentials: 'same-origin', redirect: 'error',
                    headers: {'Accept': 'application/json'}, body
                });
                const result = await response.json();
                if (!response.ok || result.avatar !== value) {
                    if (typeof result.error === 'string') failure = result.error;
                    throw new Error('Avatar was not saved');
                }
                saved = value;
                if (pending === null) {
                    showAvatar(saved);
                    message('Avatar saved.');
                }
            } catch {
                if (pending === null) {
                    inputs.forEach(input => { input.checked = input.value === saved; });
                    showAvatar(saved);
                    message(failure, true);
                }
            }
        }
        running = false;
    };
    form.addEventListener('change', event => {
        if (!inputs.includes(event.target)) return;
        pending = event.target.value;
        void save();
    });
    form.addEventListener('submit', event => {
        event.preventDefault();
        pending = inputs.find(input => input.checked).value;
        void save();
    });
    message('Changes saved automatically.');
})();
