package com.ponedelnik.messenger;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;

/** Direct UDP messenger. Rendezvous/STUN carry setup metadata only; chat payloads stay peer-to-peer. */
final class P2pMessenger {
    private static final int PND_EVENT_IDENTITY = 1;
    private static final int PND_EVENT_STATUS = 2;
    private static final int PND_EVENT_INCOMING = 3;
    private static final int PND_EVENT_PEER_INFO = 4;
    private static final int PND_EVENT_CONNECTED = 5;
    private static final int PND_EVENT_MESSAGE = 6;
    private static final int PND_EVENT_SENT = 7;
    private static final int PND_EVENT_VERIFIED = 8;
    private static final int PND_EVENT_ERROR = 9;
    private static final long PUNCH_MS = 12000;
    private final MessengerActivity activity;
    private final SecureRandom random = new SecureRandom();
    private final Map<String, Session> sessions = new ConcurrentHashMap<>();
    private final SharedPreferences prefs;
    private final P2pCall calls;
    private ExecutorService commands;
    private volatile boolean running;
    private String serverUrl, ownId, token, nickname, ownFingerprint;
    private KeyPair identity;
    private JSONArray localCandidates;
    private DatagramSocket socket;
    private SignalClient signal;
    private Thread pollThread, receiveThread;

    private static final class Session {
        final String peerId;
        final boolean outgoing;
        volatile KeyPair ephemeral;
        volatile JSONObject localHello, remoteHello;
        volatile String remoteFingerprint;
        volatile ArrayList<InetSocketAddress> remoteCandidates = new ArrayList<>();
        volatile P2pWire.Keys keys;
        volatile InetSocketAddress remoteAddress;
        volatile boolean connected, verified;
        final AtomicLong messageSendSequence = new AtomicLong();
        final AtomicLong controlSendSequence = new AtomicLong();
        final AtomicLong audioSendSequence = new AtomicLong();
        volatile long messageReceiveSequence, controlReceiveSequence, audioReceiveSequence;
        Session(String id, boolean isOutgoing) { peerId = id; outgoing = isOutgoing; }
    }

    P2pMessenger(MessengerActivity activity) {
        this.activity = activity;
        this.prefs = activity.getSharedPreferences("ponedelnik-p2p", Context.MODE_PRIVATE);
        this.calls = new P2pCall(activity, new P2pCall.Sink() {
            @Override public boolean sendControl(String peer, String message) {
                return P2pMessenger.this.sendControl(peer, message);
            }
            @Override public void sendVoice(String peer, byte[] frame) {
                P2pMessenger.this.sendVoice(peer, frame);
            }
            @Override public void emit(int type, String peer, String data) {
                if (type == P2pCall.EVENT_INCOMING) {
                    Session session = sessions.get(peer);
                    try { data = session == null ? "" : P2pUtil.peerInfo(session.remoteHello); }
                    catch (Exception ignored) { data = ""; }
                }
                P2pMessenger.this.emit(type, peer, data);
            }
        });
    }

    void start(String url, String nick) {
        if (running) stop();
        running = true;
        serverUrl = url.trim().replaceAll("/+$", "");
        nickname = P2pUtil.cleanNickname(nick);
        commands = Executors.newSingleThreadExecutor();
        commands.execute(new Runnable() {
            @Override public void run() { P2pMessenger.this.initialize(); }
        });
    }

    private void initialize() {
        try {
            P2pUtil.validateServerUrl(serverUrl);
            identity = MessengerCrypto.identity();
            ownFingerprint = MessengerCrypto.fingerprint(identity.getPublic().getEncoded());
            socket = new DatagramSocket(0);
            socket.setSoTimeout(1000);
            localCandidates = StunClient.discover(socket);
            loadRegistration();
            if (ownId == null) register();
            startReceiver();
            startPoller();
            emit(PND_EVENT_IDENTITY, ownId, ownFingerprint);
            emit(PND_EVENT_STATUS, "", "Готово для прямого соединения · сообщения не хранятся сервером");
        } catch (Exception e) {
            emit(PND_EVENT_ERROR, "", P2pUtil.safeMessage(e));
        }
    }

    private void loadRegistration() {
        try {
            String encrypted = prefs.getString("registration", "");
            if (encrypted.isEmpty()) return;
            JSONObject saved = new JSONObject(MessengerCrypto.decryptLocal(encrypted));
            if (!serverUrl.equals(saved.getString("url"))) return;
            ownId = saved.getString("id");
            token = saved.getString("token");
            signal = new SignalClient(serverUrl, ownId, token);
        } catch (Exception ignored) {
            prefs.edit().remove("registration").apply();
        }
    }

    private synchronized void register() throws Exception {
        String previousId = ownId;
        JSONObject response = SignalClient.register(serverUrl);
        ownId = response.getString("peer_id");
        if (previousId != null && !previousId.equals(ownId)) sessions.clear();
        token = response.getString("token");
        signal = new SignalClient(serverUrl, ownId, token);
        JSONObject saved = new JSONObject();
        saved.put("url", serverUrl);
        saved.put("id", ownId);
        saved.put("token", token);
        prefs.edit().putString("registration", MessengerCrypto.encryptLocal(saved.toString())).apply();
        emit(PND_EVENT_IDENTITY, ownId, ownFingerprint);
    }

    private void startPoller() {
        pollThread = new Thread(new Runnable() {
            @Override public void run() {
                while (running) {
                    try {
                        JSONObject response = signal.poll();
                        JSONObject item = response.optJSONObject("signal");
                        if (item != null) handleSignal(item);
                    } catch (Exception e) {
                        if (!running) return;
                        if (String.valueOf(e.getMessage()).contains("HTTP 401")) {
                            try { register(); } catch (Exception retry) { emit(PND_EVENT_ERROR, "", P2pUtil.safeMessage(retry)); }
                        } else {
                            emit(PND_EVENT_STATUS, "", "Сигналинг недоступен; повторяем подключение…");
                            P2pUtil.sleep(900);
                        }
                    }
                }
            }
        }, "pnd-signal-poll");
        pollThread.setDaemon(true);
        pollThread.start();
    }

    private void startReceiver() {
        receiveThread = new Thread(new Runnable() {
            @Override public void run() {
                byte[] buffer = new byte[P2pWire.MAX_PACKET_BYTES];
                while (running && socket != null && !socket.isClosed()) {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    try {
                        socket.receive(packet);
                        byte[] data = Arrays.copyOfRange(packet.getData(), packet.getOffset(), packet.getOffset() + packet.getLength());
                        if (P2pUtil.starts(data, "PH1\n")) receiveHello(data, packet);
                        else if (P2pUtil.starts(data, "PMD1")) receiveMessage(data, packet);
                        else if (P2pUtil.starts(data, "PCT1")) receiveControl(data, packet);
                        else if (P2pUtil.starts(data, "PVA1")) receiveVoice(data, packet);
                    } catch (java.net.SocketTimeoutException ignored) {
                    } catch (Exception e) {
                        if (running && socket != null && !socket.isClosed()) emit(PND_EVENT_ERROR, "", P2pUtil.safeMessage(e));
                    }
                }
            }
        }, "pnd-udp-receive");
        receiveThread.setDaemon(true);
        receiveThread.start();
    }

    private void handleSignal(JSONObject item) throws Exception {
        String from = item.getString("from").toUpperCase();
        String kind = item.getString("kind");
        JSONObject hello = item.getJSONObject("payload").getJSONObject("hello");
        if (!P2pWire.verify(hello, from, ownId)) throw new SecurityException("invalid signed peer invitation");
        String fingerprint = P2pWire.fingerprint(hello);
        String pinned = prefs.getString("pin_" + from, "");
        if (!pinned.isEmpty() && !P2pUtil.normalizeFingerprint(pinned).equals(P2pUtil.normalizeFingerprint(fingerprint))) {
            emit(PND_EVENT_ERROR, from, "Ключ контакта изменился — соединение отклонено");
            return;
        }
        Session session = sessions.get(from);
        if ("offer".equals(kind)) {
            if (session != null && session.outgoing) {
                if (ownId.compareTo(from) < 0) return;
                sessions.remove(from, session);
                session = null;
            }
            if (session == null) {
                session = new Session(from, false);
                sessions.put(from, session);
            }
            session.remoteHello = hello;
            session.remoteFingerprint = fingerprint;
            session.verified = !pinned.isEmpty();
            session.remoteCandidates = StunClient.parse(hello.getJSONArray("candidates"));
            emit(PND_EVENT_INCOMING, from, P2pUtil.peerInfo(hello));
        } else if ("answer".equals(kind) && session != null && session.outgoing) {
            session.remoteHello = hello;
            session.remoteFingerprint = fingerprint;
            session.verified = !pinned.isEmpty();
            session.remoteCandidates = StunClient.parse(hello.getJSONArray("candidates"));
            emit(PND_EVENT_PEER_INFO, from, P2pUtil.peerInfo(hello));
            if (session.verified) emit(PND_EVENT_VERIFIED, from, fingerprint);
            prepareSession(session);
        }
    }

    void connect(String peerId) {
        if (!running || commands == null) { emit(PND_EVENT_ERROR, peerId, "Сначала подключите сигналинг"); return; }
        commands.execute(new Runnable() {
            @Override public void run() {
                try {
                    String id = peerId == null ? "" : peerId.trim().toUpperCase();
                    if (!id.matches("[0-9A-F]{32}") || id.equals(ownId)) throw new IllegalArgumentException("неверный Peer ID");
                    Session session = new Session(id, true);
                    sessions.put(id, session);
                    session.localHello = createHello(id, session);
                    JSONObject payload = new JSONObject();
                    payload.put("hello", session.localHello);
                    signal.send(id, "offer", payload);
                    emit(PND_EVENT_STATUS, id, "Запрос отправлен; ждём принятия и прямого UDP-соединения…");
                } catch (Exception e) { emit(PND_EVENT_ERROR, peerId, P2pUtil.safeMessage(e)); }
            }
        });
    }

    void accept(String peerId) {
        if (commands == null) return;
        commands.execute(new Runnable() {
            @Override public void run() {
                try {
                    Session session = sessions.get(peerId);
                    if (session == null || session.remoteHello == null || session.outgoing)
                        throw new IllegalStateException("запрос уже истёк");
                    session.localHello = createHello(peerId, session);
                    JSONObject payload = new JSONObject();
                    payload.put("hello", session.localHello);
                    signal.send(peerId, "answer", payload);
                    prepareSession(session);
                    emit(PND_EVENT_PEER_INFO, peerId, P2pUtil.peerInfo(session.remoteHello));
                    if (session.verified) emit(PND_EVENT_VERIFIED, peerId, session.remoteFingerprint);
                } catch (Exception e) { emit(PND_EVENT_ERROR, peerId, P2pUtil.safeMessage(e)); }
            }
        });
    }

    private JSONObject createHello(String peer, Session session) throws Exception {
        session.ephemeral = MessengerCrypto.ephemeral();
        byte[] nonce = new byte[16];
        random.nextBytes(nonce);
        return P2pWire.hello(ownId, peer, nickname, identity, session.ephemeral, localCandidates, MessengerCrypto.encode(nonce));
    }

    private void prepareSession(Session session) throws Exception {
        if (session.localHello == null || session.remoteHello == null) return;
        if (!P2pWire.verify(session.remoteHello, session.peerId, ownId))
            throw new SecurityException("подпись контакта не прошла проверку");
        session.keys = P2pWire.derive(ownId, session.localHello, session.remoteHello, session.ephemeral);
        punch(session);
    }

    void verify(String peerId, String fingerprint) {
        Session session = sessions.get(peerId);
        if (session == null || session.remoteFingerprint == null) {
            emit(PND_EVENT_ERROR, peerId, "Отпечаток ещё не получен");
            return;
        }
        String entered = P2pUtil.normalizeFingerprint(fingerprint);
        if (!entered.equals(P2pUtil.normalizeFingerprint(session.remoteFingerprint))) {
            emit(PND_EVENT_ERROR, peerId, "Отпечаток не совпал. Ничего не отправлено.");
            return;
        }
        prefs.edit().putString("pin_" + peerId, session.remoteFingerprint).apply();
        session.verified = true;
        emit(PND_EVENT_VERIFIED, peerId, session.remoteFingerprint);
    }

    void send(String peerId, String text) {
        Session session = secureSession(peerId);
        if (session == null) { emit(PND_EVENT_ERROR, peerId, "Нет проверенного прямого соединения"); return; }
        try {
            byte[] plain = text.getBytes(StandardCharsets.UTF_8);
            if (plain.length == 0 || plain.length > P2pWire.MAX_MESSAGE_BYTES)
                throw new IllegalArgumentException("лимит сообщения — 900 байт UTF-8");
            long sequence = session.messageSendSequence.incrementAndGet();
            byte[] packet = P2pWire.seal("PMD1", ownId, peerId, sequence, session.keys.sessionId,
                    session.keys.send, plain, random);
            if (!sendPacket(packet, session.remoteAddress)) throw new java.io.IOException("Не удалось отправить сообщение");
            emit(PND_EVENT_SENT, peerId, text);
        } catch (Exception error) { emit(PND_EVENT_ERROR, peerId, P2pUtil.safeMessage(error)); }
    }

    void call(String peerId) {
        if (secureSession(peerId) == null) { emit(PND_EVENT_ERROR, peerId, "Сначала установите и проверьте P2P-соединение"); return; }
        calls.call(peerId);
    }
    void acceptCall(String peerId) { calls.accept(peerId); }
    void declineCall(String peerId) { calls.decline(peerId); }
    void endCall(String peerId) { calls.end(peerId); }
    void muteCall(String peerId, boolean muted) { calls.setMuted(peerId, muted); }

    private Session secureSession(String peerId) {
        Session session = sessions.get(peerId);
        return session != null && session.connected && session.verified && session.keys != null
                ? session : null;
    }

    private boolean sendControl(String peerId, String value) {
        Session session = secureSession(peerId);
        if (session == null) return false;
        try {
            long sequence = session.controlSendSequence.incrementAndGet();
            return sendPacket(P2pWire.seal("PCT1", ownId, peerId, sequence, session.keys.sessionId,
                    session.keys.send, value.getBytes(StandardCharsets.US_ASCII), random), session.remoteAddress);
        } catch (Exception error) { emit(PND_EVENT_ERROR, peerId, P2pUtil.safeMessage(error)); return false; }
    }

    private void sendVoice(String peerId, byte[] frame) {
        Session session = secureSession(peerId);
        if (session == null) return;
        try {
            long sequence = session.audioSendSequence.incrementAndGet();
            sendPacket(P2pWire.seal("PVA1", ownId, peerId, sequence, session.keys.sessionId,
                    session.keys.send, frame, random), session.remoteAddress);
        } catch (Exception error) { emit(PND_EVENT_ERROR, peerId, P2pUtil.safeMessage(error)); }
    }

    private void punch(Session session) {
        Thread thread = new Thread(new Runnable() {
            @Override public void run() {
                long end = System.currentTimeMillis() + PUNCH_MS;
                byte[] hello = ("PH1\n" + session.localHello.toString()).getBytes(StandardCharsets.UTF_8);
                while (running && !session.connected && System.currentTimeMillis() < end) {
                    for (InetSocketAddress candidate : session.remoteCandidates) sendPacket(hello, candidate);
                    P2pUtil.sleep(250);
                }
                if (running && !session.connected) emit(PND_EVENT_STATUS, session.peerId,
                        "Прямой маршрут не найден. Эта NAT-сеть может не поддерживать P2P без relay.");
            }
        }, "pnd-punch-" + session.peerId);
        thread.setDaemon(true);
        thread.start();
    }

    private void receiveHello(byte[] packet, DatagramPacket source) throws Exception {
        JSONObject hello = new JSONObject(new String(packet, 4, packet.length - 4, StandardCharsets.UTF_8));
        String from = hello.getString("from").toUpperCase();
        Session session = sessions.get(from);
        if (session == null || session.localHello == null || session.remoteHello == null || session.keys == null) return;
        if (!P2pWire.verify(hello, from, ownId)
                || !P2pWire.canonical(hello).equals(P2pWire.canonical(session.remoteHello))) return;
        session.remoteAddress = new InetSocketAddress(source.getAddress(), source.getPort());
        if (!session.connected) {
            session.connected = true;
            emit(PND_EVENT_CONNECTED, from, P2pUtil.peerInfo(session.remoteHello));
        }
        sendPacket(("PH1\n" + session.localHello.toString()).getBytes(StandardCharsets.UTF_8), session.remoteAddress);
    }

    private void receiveMessage(byte[] bytes, DatagramPacket source) throws Exception {
        P2pWire.Packet packet = open(bytes, "PMD1");
        Session session = packet == null ? null : secureSession(packet.from);
        if (session == null || packet.sequence <= session.messageReceiveSequence
                || packet.payload.length > P2pWire.MAX_MESSAGE_BYTES) return;
        session.messageReceiveSequence = packet.sequence;
        session.remoteAddress = new InetSocketAddress(source.getAddress(), source.getPort());
        emit(PND_EVENT_MESSAGE, packet.from, new String(packet.payload, StandardCharsets.UTF_8));
    }

    private void receiveControl(byte[] bytes, DatagramPacket source) throws Exception {
        P2pWire.Packet packet = open(bytes, "PCT1");
        Session session = packet == null ? null : secureSession(packet.from);
        if (session == null || packet.sequence <= session.controlReceiveSequence || packet.payload.length > 96) return;
        session.controlReceiveSequence = packet.sequence;
        session.remoteAddress = new InetSocketAddress(source.getAddress(), source.getPort());
        calls.receiveControl(packet.from, new String(packet.payload, StandardCharsets.US_ASCII));
    }

    private void receiveVoice(byte[] bytes, DatagramPacket source) throws Exception {
        P2pWire.Packet packet = open(bytes, "PVA1");
        Session session = packet == null ? null : secureSession(packet.from);
        if (session == null || packet.sequence <= session.audioReceiveSequence
                || packet.payload.length != VoiceCodec.FRAME_BYTES) return;
        session.audioReceiveSequence = packet.sequence;
        session.remoteAddress = new InetSocketAddress(source.getAddress(), source.getPort());
        calls.receiveVoice(packet.from, packet.payload);
    }

    private P2pWire.Packet open(byte[] bytes, String magic) {
        if (bytes == null || bytes.length < 37 || bytes[4] != 32) return null;
        String from = new String(bytes, 5, 32, StandardCharsets.US_ASCII);
        Session session = secureSession(from);
        if (session == null) return null;
        try { return P2pWire.open(bytes, magic, ownId, session.keys.receive, session.keys.sessionId); }
        catch (Exception invalidPacket) { return null; }
    }

    private boolean sendPacket(byte[] bytes, InetSocketAddress address) {
        try {
            if (!running || socket == null || socket.isClosed() || address == null) return false;
            socket.send(new DatagramPacket(bytes, bytes.length, address));
            return true;
        } catch (Exception ignored) { return false; }
    }

    void stop() {
        calls.stop();
        running = false;
        Thread poll = pollThread, receive = receiveThread;
        if (socket != null) socket.close();
        if (poll != null) poll.interrupt();
        if (receive != null) receive.interrupt();
        if (commands != null) commands.shutdownNow();
        sessions.clear();
        signal = null;
    }

    private void emit(int type, String peer, String data) { activity.emitMessengerEvent(type, peer, data); }

}
