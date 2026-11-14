const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const crypto = require('node:crypto');
const script = fs.readFileSync('public-web/directory/replay.js', 'utf8');
function element() {return {children: [], textContent: '', hidden: true, disabled: true, value: '1', append(value) {this.children.push(value);}};}
async function render(tampered) {
    const events = [{elapsed_ms: 0, line: 'playerjoin 1 DELETED_PLAYER -'}, {elapsed_ms: 0, line: 'playerjoin 2 @rival Blue'}, {elapsed_ms: 100, line: 'f 1 ' + '0'.repeat(264)}];
    const source = JSON.stringify(events);
    const nodes = Object.fromEntries(['summary', 'players', 'status', 'download', 'seek', 'board', 'play', 'speed', 'events', 'time'].map(id => [id, element()]));
    const labels = [];
    nodes.board.getContext = () => ({fillRect() {}, fillText(value) {labels.push(value);}});
    const document = {getElementById(id) {return nodes[id];}, createElement() {return element();}};
    const data = {match: {server_id: 'one', ranked: 1, end_reason: 'completed', started_epoch: 1000, ended_epoch: 1100, winner_nick: 'DELETED PLAYER', winner_team: 'Red', replay_redacted: 1, replay_sha256: crypto.createHash('sha256').update(source).digest('hex')}, players: [{nick: 'DELETED PLAYER', deleted: true, placement: 1, won: 1, lines_cleared: 10, specials_sent: 2, blocks_placed: 30}, {nick: '@rival', deleted: false, placement: 2, won: 0}], events, replay_source: source + (tampered ? ' ' : '')};
    await vm.runInNewContext(script, {document, location: {search: '?server=one&database=abc&game=1'}, URLSearchParams, TextEncoder, crypto: crypto.webcrypto, fetch: async () => ({ok: true, json: async () => data}), requestAnimationFrame() {}});
    return {nodes, labels};
}
(async () => {
    const {nodes, labels} = await render(false);
    assert.match(nodes.summary.textContent, /Winner: DELETED PLAYER/);
    assert.match(nodes.summary.textContent, /Team: Red/);
    assert.equal(nodes.players.children[0].children[0].textContent, 'DELETED PLAYER');
    assert.equal(nodes.players.children[0].children[0].children.length, 0);
    assert.equal(nodes.players.children[0].children[1].textContent, 'Won');
    assert.match(nodes.players.children[1].children[0].children[0].href, /nick=%40rival/);
    assert.ok(labels.includes('DELETED PLAYER'));
    assert.match(nodes.status.textContent, /Names removed for privacy/);
    assert.equal(nodes.play.disabled, false);
    assert.match((await render(true)).nodes.status.textContent, /checksum mismatch/i);
    console.log('PASS: deleted replay labels have no profile links, winners/team results remain and checksum failures still block playback');
})().catch(error => {console.error(error); process.exit(1);});
