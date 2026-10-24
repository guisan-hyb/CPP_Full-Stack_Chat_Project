const fs = require('fs');
const http = require('http');
const net = require('net');

const GATE_HOST = process.env.GATE_HOST || '127.0.0.1';
const GATE_PORT = Number(process.env.GATE_PORT || 8080);
const CHAT1_HOST = process.env.CHAT1_HOST || GATE_HOST;
const CHAT1_PORT = Number(process.env.CHAT1_PORT || 8990);
const CHAT2_HOST = process.env.CHAT2_HOST || GATE_HOST;
const CHAT2_PORT = Number(process.env.CHAT2_PORT || 8991);
const REDIS_HOST = process.env.REDIS_HOST || GATE_HOST;
const REDIS_PORT = Number(process.env.REDIS_PORT || 6379);
const PAIR_COUNT = Number(process.env.PERF_PAIRS || 50);
const HTTP_DURATION_MS = Number(process.env.PERF_HTTP_MS || 10000);
const CHAT_DURATION_MS = Number(process.env.PERF_CHAT_MS || 15000);
const REQUEST_TIMEOUT_MS = Number(process.env.PERF_TIMEOUT_MS || 5000);
const OUTPUT_FILE = process.env.PERF_OUTPUT || 'perf-results.json';

const MSG_CHAT_LOGIN = 1005;
const MSG_CHAT_LOGIN_RSP = 1006;
const ID_ADD_FRIEND_REQ = 1009;
const ID_ADD_FRIEND_RSP = 1010;
const ID_AUTH_FRIEND_REQ = 1013;
const ID_AUTH_FRIEND_RSP = 1014;
const ID_TEXT_CHAT_MSG_REQ = 1017;
const ID_TEXT_CHAT_MSG_RSP = 1018;
const ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019;

const RUN_ID = `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`;
const PASSWORD = 'PerfPass123!';
const MESSAGE_CONTENT = 'x'.repeat(64);

class MiniRedis {
    constructor() {
        this.socket = null;
        this.buffer = Buffer.alloc(0);
        this.pending = [];
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.socket = net.createConnection({ host: REDIS_HOST, port: REDIS_PORT });
            this.socket.once('connect', resolve);
            this.socket.once('error', reject);
            this.socket.on('data', data => {
                this.buffer = Buffer.concat([this.buffer, data]);
                this.parseReplies();
            });
        });
    }

    parseReplies() {
        while (this.pending.length > 0) {
            const parsed = parseResp(this.buffer);
            if (!parsed) return;
            this.buffer = parsed.rest;
            const waiter = this.pending.shift();
            if (parsed.error) waiter.reject(parsed.error);
            else waiter.resolve(parsed.value);
        }
    }

    command(...args) {
        return new Promise((resolve, reject) => {
            this.pending.push({ resolve, reject });
            const parts = [`*${args.length}\r\n`];
            for (const arg of args) {
                const value = Buffer.from(String(arg));
                parts.push(`$${value.length}\r\n`);
                parts.push(value);
                parts.push('\r\n');
            }
            this.socket.write(Buffer.concat(parts.map(part => Buffer.isBuffer(part) ? part : Buffer.from(part))));
        });
    }

    close() {
        if (this.socket && !this.socket.destroyed) this.socket.destroy();
    }
}

function parseResp(buffer) {
    if (buffer.length < 3) return null;
    const lineEnd = buffer.indexOf('\r\n');
    if (lineEnd < 0) return null;
    const marker = String.fromCharCode(buffer[0]);
    const line = buffer.subarray(1, lineEnd).toString();

    if (marker === '+') return { value: line, rest: buffer.subarray(lineEnd + 2) };
    if (marker === '-') return { error: new Error(line), rest: buffer.subarray(lineEnd + 2) };
    if (marker === ':') return { value: Number(line), rest: buffer.subarray(lineEnd + 2) };

    if (marker === '$') {
        const length = Number(line);
        if (length < 0) return { value: null, rest: buffer.subarray(lineEnd + 2) };
        const end = lineEnd + 2 + length;
        if (buffer.length < end + 2) return null;
        return {
            value: buffer.subarray(lineEnd + 2, end).toString(),
            rest: buffer.subarray(end + 2)
        };
    }

    if (marker === '*') {
        const count = Number(line);
        if (count < 0) return { value: null, rest: buffer.subarray(lineEnd + 2) };
        let offset = lineEnd + 2;
        const values = [];
        for (let index = 0; index < count; index += 1) {
            const itemEnd = buffer.indexOf('\r\n', offset);
            if (itemEnd < 0) return null;
            const itemMarker = String.fromCharCode(buffer[offset]);
            const itemLength = Number(buffer.subarray(offset + 1, itemEnd).toString());
            if (itemMarker === '-') {
                return {
                    error: new Error(buffer.subarray(offset + 1, itemEnd).toString()),
                    rest: buffer.subarray(itemEnd + 2)
                };
            }
            if (itemMarker !== '$' || itemLength < 0) {
                throw new Error(`unsupported Redis array item marker: ${itemMarker}`);
            }
            const itemStart = itemEnd + 2;
            const itemStop = itemStart + itemLength;
            if (buffer.length < itemStop + 2) return null;
            values.push(buffer.subarray(itemStart, itemStop).toString());
            offset = itemStop + 2;
        }
        return { value: values, rest: buffer.subarray(offset) };
    }

    throw new Error(`unsupported Redis response marker: ${marker}`);
}

function postJson(path, body) {
    const data = Buffer.from(JSON.stringify(body));
    const started = process.hrtime.bigint();
    return new Promise((resolve, reject) => {
        const request = http.request({
            host: GATE_HOST,
            port: GATE_PORT,
            path,
            method: 'POST',
            agent: false,
            headers: {
                'content-type': 'application/json',
                'content-length': data.length
            }
        }, response => {
            const chunks = [];
            response.on('data', chunk => chunks.push(chunk));
            response.on('end', () => {
                try {
                    resolve({
                        statusCode: response.statusCode,
                        body: JSON.parse(Buffer.concat(chunks).toString('utf8')),
                        latencyMs: elapsedMs(started)
                    });
                } catch (error) {
                    reject(new Error(`invalid JSON from ${path}: ${error.message}`));
                }
            });
        });
        request.setTimeout(REQUEST_TIMEOUT_MS, () => request.destroy(new Error(`${path} timed out`)));
        request.on('error', reject);
        request.end(data);
    });
}

function getTest() {
    const started = process.hrtime.bigint();
    return new Promise((resolve, reject) => {
        const request = http.get({
            host: GATE_HOST,
            port: GATE_PORT,
            path: '/get_test',
            agent: false
        }, response => {
            response.resume();
            response.on('end', () => resolve({
                statusCode: response.statusCode,
                latencyMs: elapsedMs(started)
            }));
        });
        request.setTimeout(REQUEST_TIMEOUT_MS, () => request.destroy(new Error('/get_test timed out')));
        request.on('error', reject);
    });
}

class TcpPeer {
    constructor() {
        this.socket = null;
        this.buffer = Buffer.alloc(0);
        this.pending = new Map();
        this.textWaiters = new Map();
    }

    connect(host, port) {
        return new Promise((resolve, reject) => {
            this.socket = net.createConnection({ host, port });
            this.socket.once('connect', resolve);
            this.socket.once('error', reject);
            this.socket.on('data', data => this.onData(data));
            this.socket.on('error', error => this.rejectAll(error));
            this.socket.on('close', () => this.rejectAll(new Error('TCP connection closed')));
        });
    }

    onData(data) {
        this.buffer = Buffer.concat([this.buffer, data]);
        while (this.buffer.length >= 4) {
            const id = this.buffer.readUInt16BE(0);
            const length = this.buffer.readUInt16BE(2);
            if (this.buffer.length < 4 + length) return;
            const payload = this.buffer.subarray(4, 4 + length).toString('utf8');
            this.buffer = this.buffer.subarray(4 + length);
            let body;
            try {
                body = JSON.parse(payload);
            } catch (error) {
                this.rejectAll(new Error(`invalid TCP JSON for frame ${id}: ${error.message}`));
                return;
            }

            if (id === ID_NOTIFY_TEXT_CHAT_MSG_REQ) {
                for (const item of body.text_array || []) {
                    const waiter = this.textWaiters.get(item.unique_id);
                    if (waiter) {
                        this.textWaiters.delete(item.unique_id);
                        clearTimeout(waiter.timer);
                        waiter.resolve({
                            receivedAt: process.hrtime.bigint(),
                            body
                        });
                        break;
                    }
                }
                continue;
            }

            const queue = this.pending.get(id);
            if (queue && queue.length > 0) {
                const waiter = queue.shift();
                clearTimeout(waiter.timer);
                waiter.resolve(body);
            }
        }
    }

    rejectAll(error) {
        for (const queue of this.pending.values()) {
            for (const waiter of queue) {
                clearTimeout(waiter.timer);
                waiter.reject(error);
            }
        }
        for (const waiter of this.textWaiters.values()) {
            clearTimeout(waiter.timer);
            waiter.reject(error);
        }
        this.pending.clear();
        this.textWaiters.clear();
    }

    waitFor(id, timeoutMs = REQUEST_TIMEOUT_MS) {
        return new Promise((resolve, reject) => {
            let queue = this.pending.get(id);
            if (!queue) {
                queue = [];
                this.pending.set(id, queue);
            }
            const waiter = { resolve, reject, timer: null };
            waiter.timer = setTimeout(() => {
                const current = this.pending.get(id) || [];
                const index = current.indexOf(waiter);
                if (index >= 0) current.splice(index, 1);
                reject(new Error(`timeout waiting for frame ${id}`));
            }, timeoutMs);
            queue.push(waiter);
        });
    }

    waitForText(uniqueId, timeoutMs = REQUEST_TIMEOUT_MS) {
        return new Promise((resolve, reject) => {
            const waiter = { resolve, reject, timer: null };
            waiter.timer = setTimeout(() => {
                this.textWaiters.delete(uniqueId);
                reject(new Error(`timeout waiting for delivered message ${uniqueId}`));
            }, timeoutMs);
            this.textWaiters.set(uniqueId, waiter);
        });
    }

    send(id, body) {
        const payload = Buffer.from(JSON.stringify(body));
        if (payload.length > 2048) throw new Error(`payload too large: ${payload.length}`);
        const frame = Buffer.alloc(4 + payload.length);
        frame.writeUInt16BE(id, 0);
        frame.writeUInt16BE(payload.length, 2);
        payload.copy(frame, 4);
        this.socket.write(frame);
    }

    async request(id, body, responseId, timeoutMs = REQUEST_TIMEOUT_MS) {
        const response = this.waitFor(responseId, timeoutMs);
        this.send(id, body);
        return response;
    }

    async login(uid, token) {
        const started = process.hrtime.bigint();
        const response = await this.request(
            MSG_CHAT_LOGIN,
            { uid, token },
            MSG_CHAT_LOGIN_RSP);
        return { response, latencyMs: elapsedMs(started) };
    }

    close() {
        if (this.socket && !this.socket.destroyed) this.socket.destroy();
    }
}

function elapsedMs(started) {
    return Number(process.hrtime.bigint() - started) / 1e6;
}

function percentile(values, p) {
    if (values.length === 0) return null;
    const sorted = [...values].sort((a, b) => a - b);
    const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil((p / 100) * sorted.length) - 1));
    return round(sorted[index]);
}

function round(value) {
    return Math.round(value * 1000) / 1000;
}

function summarize(records, durationMs, extra = {}) {
    const successful = records.filter(record => record.ok);
    const errorCounts = {};
    for (const record of records) {
        if (record.ok) continue;
        const key = String(record.error ?? 'unknown');
        errorCounts[key] = (errorCounts[key] || 0) + 1;
    }
    const latencies = successful.map(record => record.latencyMs);
    return {
        ...extra,
        durationSeconds: round(durationMs / 1000),
        attempts: records.length,
        successes: successful.length,
        failures: records.length - successful.length,
        successRatePercent: records.length === 0 ? 0 : round(successful.length * 100 / records.length),
        errorCounts,
        throughputPerSecond: round(successful.length * 1000 / durationMs),
        latencyMs: {
            min: latencies.length ? round(Math.min(...latencies)) : null,
            mean: latencies.length ? round(latencies.reduce((sum, value) => sum + value, 0) / latencies.length) : null,
            p50: percentile(latencies, 50),
            p95: percentile(latencies, 95),
            p99: percentile(latencies, 99),
            max: latencies.length ? round(Math.max(...latencies)) : null
        }
    };
}

async function runWorkers(durationMs, vus, worker) {
    const stopAt = process.hrtime.bigint() + BigInt(durationMs) * 1000000n;
    const records = [];
    const workers = Array.from({ length: vus }, async (_, index) => {
        while (process.hrtime.bigint() < stopAt) {
            records.push(await worker(index));
        }
    });
    await Promise.all(workers);
    return records;
}

async function createUsers(redis) {
    const users = [];
    const records = [];
    const concurrency = 1;
    let nextIndex = 0;
    const startedAt = process.hrtime.bigint();

    async function worker() {
        while (true) {
            const index = nextIndex;
            nextIndex += 1;
            if (index >= PAIR_COUNT * 2) return;

            const role = index % 2 === 0 ? 'a' : 'b';
            const pairIndex = Math.floor(index / 2);
            const name = `perf_${RUN_ID.replace(/-/g, '_')}_${index}`;
            const email = `${name}@perf.local`;
            await redis.command('SET', `code_${email}`, '1234', 'EX', '600');
            const response = await postJson('/user_register', {
                user: name,
                email,
                passwd: PASSWORD,
                confirm: PASSWORD,
                verifycode: '1234'
            });
            if (response.body.error !== 0 || !response.body.uid) {
                records.push({ ok: false, latencyMs: response.latencyMs });
                throw new Error(`register failed for ${name}: ${JSON.stringify(response.body)}`);
            }
            records.push({ ok: true, latencyMs: response.latencyMs });
            users[index] = {
                uid: response.body.uid,
                name,
                email,
                role,
                pairIndex,
                token: null
            };
        }
    }

    await Promise.all(Array.from({ length: concurrency }, () => worker()));
    return {
        users,
        records,
        concurrency,
        durationMs: elapsedMs(startedAt)
    };
}

async function loginUsers(users) {
    const concurrency = 4;
    let nextIndex = 0;

    async function worker() {
        while (true) {
            const index = nextIndex;
            nextIndex += 1;
            if (index >= users.length) return;
            const user = users[index];
            const response = await postJson('/user_login', {
                email: user.email,
                passwd: PASSWORD
            });
            if (response.body.error !== 0 || !response.body.token) {
                throw new Error(`login failed for ${user.name}: ${JSON.stringify(response.body)}`);
            }
            if (response.body.host !== GATE_HOST ||
                ![CHAT1_PORT, CHAT2_PORT].includes(Number(response.body.port))) {
                throw new Error(`unexpected chat assignment: ${response.body.host}:${response.body.port}`);
            }
            user.token = response.body.token;
            user.host = response.body.host;
            user.port = Number(response.body.port);
        }
    }

    await Promise.all(Array.from({ length: concurrency }, () => worker()));
}

async function createFriendships(pairs) {
    for (const pair of pairs) {
        const alice = new TcpPeer();
        const bob = new TcpPeer();
        try {
            await alice.connect(CHAT1_HOST, CHAT1_PORT);
            await bob.connect(CHAT1_HOST, CHAT1_PORT);
            const aliceLogin = await alice.login(pair.alice.uid, pair.alice.token);
            const bobLogin = await bob.login(pair.bob.uid, pair.bob.token);
            if (aliceLogin.response.error !== 0 || bobLogin.response.error !== 0) {
                throw new Error(`friend setup login failed for pair ${pair.index}`);
            }

            const add = await alice.request(
                ID_ADD_FRIEND_REQ,
                {
                    applyname: pair.alice.name,
                    backname: pair.bob.name,
                    to_uid: pair.bob.uid
                },
                ID_ADD_FRIEND_RSP);
            if (add.error !== 0) throw new Error(`add friend failed for pair ${pair.index}: ${JSON.stringify(add)}`);

            const auth = await bob.request(
                ID_AUTH_FRIEND_REQ,
                { to_uid: pair.alice.uid, back: pair.alice.name },
                ID_AUTH_FRIEND_RSP);
            if (auth.error !== 0) throw new Error(`auth friend failed for pair ${pair.index}: ${JSON.stringify(auth)}`);
        } finally {
            alice.close();
            bob.close();
        }
    }
    await new Promise(resolve => setTimeout(resolve, 1000));
}

async function benchmarkHttpGet(vus) {
    const records = await runWorkers(HTTP_DURATION_MS, vus, async () => {
        try {
            const response = await getTest();
            return {
                ok: response.statusCode === 200,
                latencyMs: response.latencyMs,
                error: response.statusCode
            };
        } catch (error) {
            return { ok: false, latencyMs: 0, error: error.message };
        }
    });
    return summarize(records, HTTP_DURATION_MS, { phase: 'http_get_test', vus });
}

async function benchmarkHttpLogin(users, vus) {
    const records = await runWorkers(HTTP_DURATION_MS, vus, async index => {
        const user = users[index % users.length];
        try {
            const response = await postJson('/user_login', {
                email: user.email,
                passwd: PASSWORD
            });
            if (response.body.error === 0 && response.body.token) user.token = response.body.token;
            return {
                ok: response.body.error === 0,
                latencyMs: response.latencyMs,
                error: response.body.error
            };
        } catch (error) {
            return { ok: false, latencyMs: 0, error: error.message };
        }
    });
    return summarize(records, HTTP_DURATION_MS, { phase: 'http_login', vus });
}

async function benchmarkTcpLogin(users, vus) {
    const peers = [];
    const records = [];
    const startedAt = process.hrtime.bigint();
    try {
        for (let index = 0; index < vus; index += 1) {
            const user = users[index];
            const target = index % 2 === 0
                ? [CHAT1_HOST, CHAT1_PORT]
                : [CHAT2_HOST, CHAT2_PORT];
            const peer = new TcpPeer();
            peers.push(peer);
            try {
                const connectionStarted = process.hrtime.bigint();
                await peer.connect(target[0], target[1]);
                const result = await peer.login(user.uid, user.token);
                records.push({
                    ok: result.response.error === 0,
                    latencyMs: elapsedMs(connectionStarted),
                    error: result.response.error
                });
            } catch (error) {
                records.push({ ok: false, latencyMs: 0, error: error.message });
            }
        }
    } finally {
        for (const peer of peers) peer.close();
    }
    await new Promise(resolve => setTimeout(resolve, 1000));
    return summarize(records, elapsedMs(startedAt), { phase: 'tcp_login_sequential', connections: vus });
}

async function openChatPair(pair, mode) {
    let aTarget;
    let bTarget;
    if (mode === 'cross') {
        aTarget = [CHAT1_HOST, CHAT1_PORT];
        bTarget = [CHAT2_HOST, CHAT2_PORT];
    } else if (pair.index % 2 === 0) {
        aTarget = [CHAT1_HOST, CHAT1_PORT];
        bTarget = [CHAT1_HOST, CHAT1_PORT];
    } else {
        aTarget = [CHAT2_HOST, CHAT2_PORT];
        bTarget = [CHAT2_HOST, CHAT2_PORT];
    }

    const a = new TcpPeer();
    const b = new TcpPeer();
    try {
        await a.connect(aTarget[0], aTarget[1]);
        await b.connect(bTarget[0], bTarget[1]);
        const aLogin = await a.login(pair.alice.uid, pair.alice.token);
        const bLogin = await b.login(pair.bob.uid, pair.bob.token);
        if (aLogin.response.error !== 0 || bLogin.response.error !== 0) {
            throw new Error(`chat setup login failed for pair ${pair.index}`);
        }
        return { a, b };
    } catch (error) {
        a.close();
        b.close();
        throw error;
    }
}

async function benchmarkChat(pairs, senders, mode) {
    const connections = [];
    try {
        for (let index = 0; index < senders; index += 1) {
            connections.push(await openChatPair(pairs[index], mode));
        }

        const ackRecords = [];
        const deliveryRecords = [];
        const stopAt = process.hrtime.bigint() + BigInt(CHAT_DURATION_MS) * 1000000n;

        const workers = connections.map(async ({ a, b }, index) => {
            const pair = pairs[index];
            let sequence = 0;
            while (process.hrtime.bigint() < stopAt) {
                sequence += 1;
                const uniqueId = `${RUN_ID}-${mode}-${pair.index}-${sequence}`;
                const delivery = b.waitForText(uniqueId);
                const ack = a.waitFor(ID_TEXT_CHAT_MSG_RSP);
                const started = process.hrtime.bigint();
                a.send(ID_TEXT_CHAT_MSG_REQ, {
                    to_uid: pair.bob.uid,
                    text_array: [{
                        unique_id: uniqueId,
                        content: MESSAGE_CONTENT
                    }]
                });

                try {
                    const [ackResponse, delivered] = await Promise.all([ack, delivery]);
                    ackRecords.push({
                        ok: ackResponse.error === 0,
                        latencyMs: elapsedMs(started),
                        error: ackResponse.error
                    });
                    deliveryRecords.push({
                        ok: true,
                        latencyMs: Number(delivered.receivedAt - started) / 1e6
                    });
                } catch (error) {
                    ackRecords.push({ ok: false, latencyMs: 0, error: error.message });
                    deliveryRecords.push({ ok: false, latencyMs: 0, error: error.message });
                    throw error;
                }
            }
        });

        await Promise.all(workers);
        return {
            mode,
            senders,
            ack: summarize(ackRecords, CHAT_DURATION_MS),
            delivery: summarize(deliveryRecords, CHAT_DURATION_MS)
        };
    } finally {
        for (const connection of connections) {
            connection.a.close();
            connection.b.close();
        }
        await new Promise(resolve => setTimeout(resolve, 1200));
    }
}

async function waitForRedisOnlineCount(redis) {
    for (let attempt = 0; attempt < 20; attempt += 1) {
        const counts = await redis.command('HMGET', 'logincount', 'chatserver1', 'chatserver2');
        const total = (counts || []).reduce((sum, value) => sum + Number(value || 0), 0);
        if (total === 0) return 0;
        await new Promise(resolve => setTimeout(resolve, 250));
    }
    const counts = await redis.command('HMGET', 'logincount', 'chatserver1', 'chatserver2');
    return (counts || []).reduce((sum, value) => sum + Number(value || 0), 0);
}

async function main() {
    const startedAt = new Date();
    const redis = new MiniRedis();
    const results = {
        runId: RUN_ID,
        startedAt: startedAt.toISOString(),
        target: {
            gate: `${GATE_HOST}:${GATE_PORT}`,
            chat1: `${CHAT1_HOST}:${CHAT1_PORT}`,
            chat2: `${CHAT2_HOST}:${CHAT2_PORT}`,
            redis: `${REDIS_HOST}:${REDIS_PORT}`
        },
        settings: {
            pairs: PAIR_COUNT,
            httpDurationMs: HTTP_DURATION_MS,
            chatDurationMs: CHAT_DURATION_MS,
            messageBytes: Buffer.byteLength(MESSAGE_CONTENT),
            requestTimeoutMs: REQUEST_TIMEOUT_MS
        },
        phases: []
    };

    try {
        await redis.connect();
        console.log(`Creating ${PAIR_COUNT * 2} test users...`);
        const registration = await createUsers(redis);
        const users = registration.users;
        await loginUsers(users);
        results.phases.push(summarize(
            registration.records,
            registration.durationMs,
            { phase: 'http_register', concurrency: registration.concurrency }));

        const pairs = Array.from({ length: PAIR_COUNT }, (_, index) => ({
            index,
            alice: users[index * 2],
            bob: users[index * 2 + 1]
        }));

        console.log('Creating friendships...');
        await createFriendships(pairs);

        for (const vus of [1, 10, 50, 100]) {
            console.log(`HTTP /get_test VUs=${vus}`);
            results.phases.push(await benchmarkHttpGet(vus));
        }

        for (const vus of [1, 10, 50, 100]) {
            if (vus > users.length) continue;
            console.log(`HTTP login VUs=${vus}`);
            results.phases.push(await benchmarkHttpLogin(users, vus));
        }

        for (const connections of [10, 50, 100]) {
            if (connections > users.length) continue;
            await loginUsers(users.slice(0, connections));
            console.log(`TCP login connections=${connections}`);
            results.phases.push(await benchmarkTcpLogin(users, connections));
        }

        await loginUsers(users);
        for (const senders of [1, 5, 10, 25, 50]) {
            if (senders > PAIR_COUNT) continue;
            console.log(`Same-node chat senders=${senders}`);
            results.phases.push(await benchmarkChat(pairs, senders, 'same'));
        }

        for (const senders of [1, 5, 10, 25, 50]) {
            if (senders > PAIR_COUNT) continue;
            console.log(`Cross-node chat senders=${senders}`);
            results.phases.push(await benchmarkChat(pairs, senders, 'cross'));
        }

        results.redisOnlineCountAfterTests = await waitForRedisOnlineCount(redis);
        results.finishedAt = new Date().toISOString();
        results.durationSeconds = round((Date.now() - startedAt.getTime()) / 1000);
        fs.writeFileSync(OUTPUT_FILE, `${JSON.stringify(results, null, 2)}\n`);
        console.log(JSON.stringify(results, null, 2));
    } finally {
        redis.close();
    }
}

main().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
