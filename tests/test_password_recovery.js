// Browser behavior for private fragment links; no network or real credentials.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const script = fs.readFileSync('public-web/assets/password-recovery.js', 'utf8');
function run(hash, form = true) {
    const nodes = {
        'reset-token': {value: ''}, 'recovery-request': {hidden: false},
        'recovery-reset': {hidden: true}, 'reset-password': {focus() {this.focused = true;}}
    };
    const cleared = [];
    vm.runInNewContext(script, {
        location: {hash, pathname: '/account/recover/'},
        history: {replaceState(...args) {cleared.push(args);}},
        document: {getElementById(id) {return form ? nodes[id] : null;}}
    });
    return {nodes, cleared};
}
const token = 'a'.repeat(64);
let result = run('#' + token);
assert.equal(result.nodes['reset-token'].value, token);
assert.equal(result.nodes['recovery-request'].hidden, true);
assert.equal(result.nodes['recovery-reset'].hidden, false);
assert.equal(result.nodes['reset-password'].focused, true);
assert.equal(result.cleared[0][2], '/account/recover/');
result = run('#not-a-token');
assert.equal(result.nodes['reset-token'].value, '');
assert.equal(result.nodes['recovery-request'].hidden, false);
assert.equal(result.cleared.length, 1);
assert.equal(run('').cleared.length, 0);
assert.equal(run('#' + token, false).cleared.length, 1);
console.log('PASS: reset fragments move into POST form, leave the address bar, and handle disabled/malformed links');
