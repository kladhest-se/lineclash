// Avatar autosave ordering and failures, without real accounts or network requests.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const script = fs.readFileSync('public-web/assets/profile-avatar.js', 'utf8');
const tick = () => new Promise(resolve => setImmediate(resolve));
function fixture() {
    const handlers = {};
    const status = {textContent: '', dataset: {}};
    const previews = Array.from({length: 3}, () => ({value: 'initial', replaceChildren(...nodes) {this.value = nodes[0].value;}}));
    const inputs = ['initial', 't', 'z', 'o', 'i'].map(value => ({
        value, checked: value === 'initial',
        closest() {return {querySelector() {return {childNodes: [{cloneNode() {return {value};}}]};}};}
    }));
    const form = {
        // Native HTML forms expose named controls before their action property.
        action: {toString() {return '[object HTMLInputElement]';}},
        getAttribute(name) {assert.equal(name, 'action'); return './';},
        querySelectorAll() {return inputs;}, querySelector() {return status;},
        addEventListener(name, handler) {handlers[name] = handler;}
    };
    class Fields {
        *[Symbol.iterator]() {yield ['csrf', 'fixture-csrf']; yield ['action', 'avatar']; yield ['avatar', inputs.find(input => input.checked).value];}
    }
    const requests = [];
    vm.runInNewContext(script, {
        document: {querySelector() {return form;}, querySelectorAll() {return previews;}},
        FormData: Fields, URLSearchParams,
        fetch(url, options) {return new Promise((resolve, reject) => requests.push({url, options, resolve, reject}));}
    });
    return {
        inputs, status, previews, requests,
        choose(value) {inputs.forEach(input => {input.checked = input.value === value;}); handlers.change({target: inputs.find(input => input.value === value)});},
        reply(index, value, ok = true) {requests[index].resolve({ok, async json() {return ok ? {avatar: value} : {error: 'Unavailable'};}});},
        submit() {let prevented = false; handlers.submit({preventDefault() {prevented = true;}}); return prevented;}
    };
}
(async () => {
    const f = fixture();
    assert.equal(f.requests.length, 0);
    f.choose('t'); f.choose('z'); f.choose('o');
    assert.equal(f.requests.length, 1); // Only one save may be in flight.
    const first = f.requests[0];
    assert.equal(first.url, './');
    assert.equal(first.options.credentials, 'same-origin');
    assert.equal(first.options.body.get('csrf'), 'fixture-csrf');
    assert.equal(first.options.body.get('action'), 'avatar');
    assert.equal(first.options.body.get('format'), 'json');
    assert.equal(first.options.body.get('avatar'), 't');
    f.reply(0, 't'); await tick();
    assert.equal(f.requests.length, 2);
    assert.equal(f.requests[1].options.body.get('avatar'), 'o'); // Intermediate selections are replaced by the latest choice.
    f.reply(1, 'o'); await tick();
    assert.ok(f.previews.every(preview => preview.value === 'o'));
    assert.equal(f.status.textContent, 'Avatar saved.');
    f.choose('i'); f.reply(2, null, false); await tick();
    assert.equal(f.status.dataset.state, 'error');
    assert.equal(f.status.textContent, 'Unavailable');
    assert.equal(f.inputs.find(input => input.checked).value, 'o');
    assert.ok(f.previews.every(preview => preview.value === 'o'));
    f.choose('i'); f.reply(3, 'i'); await tick();
    assert.ok(f.previews.every(preview => preview.value === 'i'));
    assert.equal(f.status.textContent, 'Avatar saved.');
    f.choose('t'); f.requests[4].reject(new Error('Network unavailable')); await tick();
    assert.equal(f.inputs.find(input => input.checked).value, 'i');
    assert.equal(f.status.dataset.state, 'error');
    assert.equal(f.submit(), true);
    f.reply(5, 'i'); await tick();
    assert.equal(f.status.textContent, 'Avatar saved.');
    vm.runInNewContext(script, {document: {querySelector() {return null;}}});
    console.log('PASS: avatar autosave sends CSRF, serializes rapid changes, updates all previews, restores confirmed state on failure and allows retry');
})().catch(error => {console.error(error); process.exitCode = 1;});
