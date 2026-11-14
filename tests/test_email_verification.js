// Browser behavior for private fragment links; no network or real credentials.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const script = fs.readFileSync('public-web/assets/email-verification.js', 'utf8');
function run(hash, form = true) {
    const nodes = {
        'verification-token': {value: ''}, 'verification-request': {hidden: false},
        'verification-confirm': {hidden: true}, 'verify-email': {focus() {this.focused = true;}}
    };
    const cleared = [];
    vm.runInNewContext(script, {
        location: {hash, pathname: '/account/verify/'},
        history: {replaceState(...args) {cleared.push(args);}},
        document: {getElementById(id) {return form ? nodes[id] : null;}}
    });
    return {nodes, cleared};
}
const token = 'a'.repeat(64);
let result = run('#' + token);
assert.equal(result.nodes['verification-token'].value, token);
assert.equal(result.nodes['verification-request'].hidden, true);
assert.equal(result.nodes['verification-confirm'].hidden, false);
assert.equal(result.nodes['verify-email'].focused, true);
assert.equal(result.cleared[0][2], '/account/verify/');
result = run('#not-a-token');
assert.equal(result.nodes['verification-token'].value, '');
assert.equal(result.nodes['verification-request'].hidden, false);
assert.equal(result.cleared.length, 1);
assert.equal(run('').cleared.length, 0);
assert.equal(run('#' + token, false).cleared.length, 1);
console.log('PASS: activation fragments move into explicit POST forms without activating on GET');
