const assert = require('node:assert/strict'), fs = require('node:fs'), vm = require('node:vm');
const script = fs.readFileSync('public-web/assets/email-change.js', 'utf8');
const nodes = {'email-change-token': {value: ''}, 'confirm-email-change': {focus() {this.focused = true;}}};
const history = [];
vm.runInNewContext(script, {location: {hash: '#' + 'a'.repeat(64), pathname: '/account/email/'}, history: {replaceState(...args) {history.push(args);}}, document: {getElementById(id) {return nodes[id];}}});
assert.equal(nodes['email-change-token'].value, 'a'.repeat(64));
assert.equal(history[0][2], '/account/email/');
assert.equal(nodes['confirm-email-change'].focused, true);
console.log('PASS: email-change link uses a private fragment and explicit POST confirmation');
