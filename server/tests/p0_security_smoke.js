const assert = require('assert');
const http = require('http');
const net = require('net');
const Redis = require('../VerifyServer/node_modules/ioredis');

// Run after StatusServer, GateServer and both ChatServers are listening.
// The script creates unique test users; clean test data afterwards when rebuilding a demo database.
const GATE_HOST = '127.0.0.1';
const GATE_PORT = 8080;
const REDIS_HOST = process.env.REDIS_HOST || '127.0.0.1';
const REDIS_PORT = Number(process.env.REDIS_PORT || 6379);
const REDIS_PASSWORD = process.env.REDIS_PASSWORD || undefined;
const MSG_CHAT_LOGIN = 1005;
const MSG_CHAT_LOGIN_RSP = 1006;
const ID_SEARCH_USER_REQ = 1007;
const ID_SEARCH_USER_RSP = 1008;
const ID_ADD_FRIEND_REQ = 1009;
const ID_ADD_FRIEND_RSP = 1010;
const ID_AUTH_FRIEND_REQ = 1013;
const ID_AUTH_FRIEND_RSP = 1014;
const ID_TEXT_CHAT_MSG_REQ = 1017;
const ID_TEXT_CHAT_MSG_RSP = 1018;
const ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019;
const TOKEN_INVALID = 1010;
const FRIEND_REQUIRED = 1015;

function postJson(path, body) {
    const data = Buffer.from(JSON.stringify(body));
    return new Promise((resolve, reject) => {
        const request = http.request({
            host: GATE_HOST,
            port: GATE_PORT,
            path,
            method: 'POST',
            headers: {
                'content-type': 'application/json',
                'content-length': data.length
            }
        }, response => {
            const chunks = [];
            response.on('data', chunk => chunks.push(chunk));
            response.on('end', () => {
                try {
                    resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
                } catch (error) {
                    reject(new Error(`invalid JSON from ${path}: ${error.message}`));
                }
            });
        });
        request.on('error', reject);
        request.end(data);
    });
}

class TcpPeer {
    constructor() {
        this.socket = null;
        this.buffer = Buffer.alloc(0);
        this.pending = new Map();
        this.inbox = new Map();
    }

    connect(host, port) {
        return new Promise((resolve, reject) => {
            this.socket = net.createConnection({ host, port });
            this.socket.once('connect', resolve);
            this.socket.once('error', reject);
            this.socket.on('data', data => this.onData(data));
            this.socket.on('error', error => this.rejectAll(error));
        });
    }

    onData(data) {
        this.buffer = Buffer.concat([this.buffer, data]);
        while (this.buffer.length >= 4) {
            const id = this.buffer.readUInt16BE(0);
            const length = this.buffer.readUInt16BE(2);
            if (this.buffer.length < 4 + length) {
                return;
            }

            const payload = this.buffer.subarray(4, 4 + length).toString('utf8');
            this.buffer = this.buffer.subarray(4 + length);
            const queue = this.pending.get(id);
            if (queue && queue.length > 0) {
                const waiter = queue.shift();
                clearTimeout(waiter.timer);
                waiter.resolve(JSON.parse(payload));
                continue;
            }

            let inbox = this.inbox.get(id);
            if (!inbox) {
                inbox = [];
                this.inbox.set(id, inbox);
            }
            inbox.push(JSON.parse(payload));
        }
    }

    rejectAll(error) {
        for (const queue of this.pending.values()) {
            for (const waiter of queue) {
                clearTimeout(waiter.timer);
                waiter.reject(error);
            }
        }
        this.pending.clear();
    }

    waitFor(id, timeoutMs = 5000) {
        const inbox = this.inbox.get(id);
        if (inbox && inbox.length > 0) {
            return Promise.resolve(inbox.shift());
        }

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
                if (index >= 0) {
                    current.splice(index, 1);
                }
                reject(new Error(`timeout waiting for frame ${id}`));
            }, timeoutMs);
            queue.push(waiter);
        });
    }

    send(id, body) {
        const payload = Buffer.from(JSON.stringify(body));
        const frame = Buffer.alloc(4 + payload.length);
        frame.writeUInt16BE(id, 0);
        frame.writeUInt16BE(payload.length, 2);
        payload.copy(frame, 4);
        this.socket.write(frame);
    }

    async request(id, body, responseId, timeoutMs = 5000) {
        const response = this.waitFor(responseId, timeoutMs);
        this.send(id, body);
        return response;
    }

    async login(uid, token) {
        return this.request(
            MSG_CHAT_LOGIN,
            { uid, token },
            MSG_CHAT_LOGIN_RSP);
    }

    close() {
        if (this.socket && !this.socket.destroyed) {
            this.socket.destroy();
        }
    }
}

async function register(redis, password, suffix, role) {
    const email = `${role}.${suffix}@test.local`;
    const user = `${role}_${suffix}`;
    await redis.set(`code_${email}`, '1234', 'EX', 120);

    const response = await postJson('/user_register', {
        user,
        email,
        passwd: password,
        confirm: password,
        verifycode: '1234'
    });
    assert.strictEqual(response.error, 0, `register ${role} failed: ${JSON.stringify(response)}`);
    assert.ok(response.uid > 0, `register ${role} returned invalid uid`);

    return { email, user, uid: response.uid };
}

async function main() {
    const suffix = Date.now().toString();
    const redis = new Redis({
        host: REDIS_HOST,
        port: REDIS_PORT,
        password: REDIS_PASSWORD,
        connectTimeout: 3000,
        maxRetriesPerRequest: 1,
        retryStrategy: () => null
    });

    const peers = [];
    try {
        const alice = await register(redis, 'Alice123!', suffix, 'alice');
        const bob = await register(redis, 'Bob12345!', suffix, 'bob');

        const aliceLogin = await postJson('/user_login', {
            email: alice.email,
            passwd: 'Alice123!'
        });
        const bobLogin = await postJson('/user_login', {
            email: bob.email,
            passwd: 'Bob12345!'
        });
        assert.strictEqual(aliceLogin.error, 0, `alice login failed: ${JSON.stringify(aliceLogin)}`);
        assert.strictEqual(bobLogin.error, 0, `bob login failed: ${JSON.stringify(bobLogin)}`);

        const tokenTtl = await redis.ttl(`utoken_${alice.uid}`);
        assert.ok(tokenTtl > 0 && tokenTtl <= 1800, `unexpected token TTL: ${tokenTtl}`);

        const unauthenticated = new TcpPeer();
        peers.push(unauthenticated);
        await unauthenticated.connect(aliceLogin.host, Number(aliceLogin.port));
        const unauthSearch = await unauthenticated.request(
            ID_SEARCH_USER_REQ,
            { uid: bob.user },
            ID_SEARCH_USER_RSP);
        assert.strictEqual(unauthSearch.error, TOKEN_INVALID, 'unauthenticated search must be rejected');

        const alicePeer = new TcpPeer();
        const bobPeer = new TcpPeer();
        peers.push(alicePeer, bobPeer);
        await alicePeer.connect(aliceLogin.host, Number(aliceLogin.port));
        await bobPeer.connect(bobLogin.host, Number(bobLogin.port));

        const aliceChatLogin = await alicePeer.login(alice.uid, aliceLogin.token);
        const bobChatLogin = await bobPeer.login(bob.uid, bobLogin.token);
        assert.strictEqual(aliceChatLogin.error, 0, 'alice TCP login failed');
        assert.strictEqual(bobChatLogin.error, 0, 'bob TCP login failed');

        const searchByName = await alicePeer.request(
            ID_SEARCH_USER_REQ,
            { uid: bob.user },
            ID_SEARCH_USER_RSP);
        assert.strictEqual(searchByName.error, 0, 'name search should succeed');
        assert.strictEqual(searchByName.uid, bob.uid);

        const nameCacheValue = await redis.get(`nameinfo_${bob.user}`);
        assert.strictEqual(nameCacheValue, String(bob.uid), 'name cache must store only uid');
        const profileCacheValue = await redis.get(`ubaseinfo_${bob.uid}`);
        assert.ok(profileCacheValue, 'profile cache should exist');
        assert.strictEqual(JSON.parse(profileCacheValue).uid, bob.uid);
        const profileTtl = await redis.ttl(`ubaseinfo_${bob.uid}`);
        const nameTtl = await redis.ttl(`nameinfo_${bob.user}`);
        assert.ok(profileTtl > 0 && profileTtl <= 600, `unexpected profile TTL: ${profileTtl}`);
        assert.ok(nameTtl > 0 && nameTtl <= 600, `unexpected name TTL: ${nameTtl}`);

        const loginCounts = await redis.hgetall('logincount');
        const totalLoginCount = Object.values(loginCounts)
            .reduce((total, count) => total + Number(count), 0);
        assert.strictEqual(totalLoginCount, 2, `unexpected online count: ${JSON.stringify(loginCounts)}`);

        const beforeFriend = {
            to_uid: bob.uid,
            text_array: [
                { unique_id: `before-${suffix}`, content: 'not friends yet' }
            ]
        };
        const beforeFriendResponse = await alicePeer.request(
            ID_TEXT_CHAT_MSG_REQ,
            beforeFriend,
            ID_TEXT_CHAT_MSG_RSP);
        assert.strictEqual(beforeFriendResponse.error, FRIEND_REQUIRED, 'stranger chat must be rejected');

        const addFriend = await alicePeer.request(
            ID_ADD_FRIEND_REQ,
            { applyname: 'smoke test', backname: 'bob', to_uid: bob.uid },
            ID_ADD_FRIEND_RSP);
        assert.strictEqual(addFriend.error, 0, `add friend failed: ${JSON.stringify(addFriend)}`);

        const authFriend = await bobPeer.request(
            ID_AUTH_FRIEND_REQ,
            { to_uid: alice.uid, back: 'alice' },
            ID_AUTH_FRIEND_RSP);
        assert.strictEqual(authFriend.error, 0, `auth friend failed: ${JSON.stringify(authFriend)}`);

        const friendMessageId = `friend-${suffix}`;
        const friendMessage = {
            to_uid: bob.uid,
            text_array: [
                { unique_id: friendMessageId, content: 'hello from smoke test' }
            ]
        };
        const friendMessageResponse = await alicePeer.request(
            ID_TEXT_CHAT_MSG_REQ,
            friendMessage,
            ID_TEXT_CHAT_MSG_RSP);
        assert.strictEqual(friendMessageResponse.error, 0, 'friend chat must succeed');

        const notification = await bobPeer.waitFor(ID_NOTIFY_TEXT_CHAT_MSG_REQ);
        assert.strictEqual(notification.from_uid, alice.uid);
        assert.strictEqual(notification.text_array[0].unique_id, friendMessageId);

        const spoofed = await alicePeer.request(
            ID_TEXT_CHAT_MSG_REQ,
            {
                from_uid: bob.uid,
                to_uid: bob.uid,
                text_array: [
                    { unique_id: `spoof-${suffix}`, content: 'spoofed sender' }
                ]
            },
            ID_TEXT_CHAT_MSG_RSP);
        assert.strictEqual(spoofed.error, TOKEN_INVALID, 'spoofed from_uid must be rejected');

        for (const peer of peers) {
            peer.close();
        }
        await new Promise(resolve => setTimeout(resolve, 500));
        const finalCounts = await redis.hgetall('logincount');
        const finalLoginCount = Object.values(finalCounts)
            .reduce((total, count) => total + Number(count), 0);
        assert.strictEqual(finalLoginCount, 0, `online count did not return to zero: ${JSON.stringify(finalCounts)}`);

        console.log('P0 security smoke test passed');
    } finally {
        for (const peer of peers) {
            peer.close();
        }
        await redis.quit();
    }
}

main().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
