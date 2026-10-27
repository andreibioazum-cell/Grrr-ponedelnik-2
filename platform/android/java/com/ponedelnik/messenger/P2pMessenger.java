package com.ponedelnik.messenger;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

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
    private static final int MAX_PACKET = 1400;
    private static final long PUNCH_MS = 12000;
    private final MessengerActivity activity;
    private final SecureRandom random = new SecureRandom();
    private final Map<String, Session> sessions = new ConcurrentHashMap<>();
    private final SharedPreferences prefs;
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
        volatile long sendSequence, receiveSequence;
        Session(String id, boolean isOutgoing) { peerId = id; outgoing = isOutgoing; }
    }

    P2pMessenger(MessengerActivity activity) {
        this.activity = activity;
        this.prefs = activity.getSharedPreferences("ponedelnik-p2p", Context.MODE_PRIVATE);
    }

    void start(String url, String nick) {
        if (running) stop();
        running = true;
        serverUrl = url.trim().replaceAll("/+$", "");
        nickname = cleanNickname(nick);
        commands = Executors.newSingleThreadExecutor();
        commands.execute(() -> initialize());
    }

    private void initialize() {
        try {
            validateServerUrl();
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
            emit(PND_EVENT_ERROR, "", safeMessage(e));
        }
    }

    private void validateServerUrl() throws Exception {
        java.net.URI uri = new java.net.URI(serverUrl);
        String scheme = uri.getScheme();
        String host = uri.getHost();
        if (host == null || !("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme)))
            throw new IllegalArgumentException("укажи корректный URL сигналинга");
        if ("https".equalsIgnoreCase(scheme)) return;
        String h = host.toLowerCase(java.util.Locale.US);
        if (h.equals("localhost") || h.equals("10.0.2.2")) return;
        if (!h.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}"))
            throw new SecurityException("для внешнего сервера обязателен HTTPS");
        String[] parts = h.split("\\.");
        int[] octets = new int[4];
        for (int i = 0; i < 4; i++) {
            octets[i] = Integer.parseInt(parts[i]);
            if (octets[i] > 255) throw new SecurityException("неверный локальный IP");
        }
        boolean local = octets[0] == 10 || octets[0] == 127
                || (octets[0] == 192 && octets[1] == 168)
                || (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31)
                || (octets[0] == 169 && octets[1] == 254);
        if (!local) throw new SecurityException("для внешнего сервера обязателен HTTPS");
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
        pollThread = new Thread(() -> {
            while (running) {
                try {
                    JSONObject response = signal.poll();
                    JSONObject item = response.optJSONObject("signal");
                    if (item != null) handleSignal(item);
                } catch (Exception e) {
                    if (!running) return;
                    if (String.valueOf(e.getMessage()).contains("HTTP 401")) {
                        try { register(); } catch (Exception retry) { emit(PND_EVENT_ERROR, "", safeMessage(retry)); }
                    } else {
                        emit(PND_EVENT_STATUS, "", "Сигналинг недоступен; повторяем подключение…");
                        sleep(900);
                    }
                }
            }
        }, "pnd-signal-poll");
        pollThread.setDaemon(true);
        pollThread.start();
    }

    private void startReceiver() {
        receiveThread = new Thread(() -> {
            byte[] buffer = new byte[MAX_PACKET];
            while (running && socket != null && !socket.isClosed()) {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                try {
                    socket.receive(packet);
                    byte[] data = Arrays.copyOfRange(packet.getData(), packet.getOffset(), packet.getOffset() + packet.getLength());
                    if (starts(data, "PH1\n")) receiveHello(data, packet);
                    else if (starts(data, "PMD1")) receiveMessage(data, packet);
                } catch (java.net.SocketTimeoutException ignored) {
                } catch (Exception e) {
                    if (running && socket != null && !socket.isClosed()) emit(PND_EVENT_ERROR, "", safeMessage(e));
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
        if (!pinned.isEmpty() && !sameFingerprint(pinned, fingerprint)) {
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
            emit(PND_EVENT_INCOMING, from, peerInfo(hello));
            if (session.verified) emit(PND_EVENT_VERIFIED, from, fingerprint);
        } else if ("answer".equals(kind) && session != null && session.outgoing) {
            session.remoteHello = hello;
            session.remoteFingerprint = fingerprint;
            session.verified = !pinned.isEmpty();
            session.remoteCandidates = StunClient.parse(hello.getJSONArray("candidates"));
            emit(PND_EVENT_PEER_INFO, from, peerInfo(hello));
            if (session.verified) emit(PND_EVENT_VERIFIED, from, fingerprint);
            prepareSession(session);
        }
    }

    void connect(String peerId) {
        if (!running || commands == null) { emit(PND_EVENT_ERROR, peerId, "Сначала подключите сигналинг"); return; }
        commands.execute(() -> {
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
            } catch (Exception e) { emit(PND_EVENT_ERROR, peerId, safeMessage(e)); }
        });
    }

    void accept(String peerId) {
        if (commands == null) return;
        commands.execute(() -> {
            try {
                Session session = sessions.get(peerId);
                if (session == null || session.remoteHello == null || session.outgoing)
                    throw new IllegalStateException("запрос уже истёк");
                session.localHello = createHello(peerId, session);
                JSONObject payload = new JSONObject();
                payload.put("hello", session.localHello);
                signal.send(peerId, "answer", payload);
                prepareSession(session);
                emit(PND_EVENT_PEER_INFO, peerId, peerInfo(session.remoteHello));
            } catch (Exception e) { emit(PND_EVENT_ERROR, peerId, safeMessage(e)); }
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
        String entered = normalizeFingerprint(fingerprint);
        if (!entered.equals(normalizeFingerprint(session.remoteFingerprint))) {
            emit(PND_EVENT_ERROR, peerId, "Отпечаток не совпал. Ничего не отправлено.");
            return;
        }
        prefs.edit().putString("pin_" + peerId, session.remoteFingerprint).apply();
        session.verified = true;
        emit(PND_EVENT_VERIFIED, peerId, session.remoteFingerprint);
    }

    void send(String peerId, String text) {
        Session session = sessions.get(peerId);
        if (session == null || !session.connected || !session.verified || session.keys == null) {
            emit(PND_EVENT_ERROR, peerId, "Нет проверенного прямого соединения");
            return;
        }
        try {
            byte[] plain = text.getBytes(StandardCharsets.UTF_8);
            if (plain.length == 0 || plain.length > P2pWire.MAX_MESSAGE_BYTES)
                throw new IllegalArgumentException("лимит сообщения — 900 байт UTF-8");
            long sequence = ++session.sendSequence;
            byte[] nonce = new byte[12];
            random.nextBytes(nonce);
            byte[] aad = P2pWire.associatedData(ownId, peerId, sequence, session.keys.sessionId);
            byte[] cipher = MessengerCrypto.encrypt(session.keys.send, nonce, aad, plain);
            byte[] id = ownId.getBytes(StandardCharsets.US_ASCII);
            ByteBuffer packet = ByteBuffer.allocate(4 + 1 + id.length + 8 + 8 + 12 + cipher.length).order(ByteOrder.BIG_ENDIAN);
            packet.put(new byte[] {'P','M','D','1'}).put((byte)id.length).put(id).putLong(sequence);
            packet.put(session.keys.sessionId).put(nonce).put(cipher);
            sendPacket(packet.array(), session.remoteAddress);
            emit(PND_EVENT_SENT, peerId, text);
        } catch (Exception e) { emit(PND_EVENT_ERROR, peerId, safeMessage(e)); }
    }

    private void punch(Session session) {
        Thread thread = new Thread(() -> {
            long end = System.currentTimeMillis() + PUNCH_MS;
            byte[] hello = ("PH1\n" + session.localHello.toString()).getBytes(StandardCharsets.UTF_8);
            while (running && !session.connected && System.currentTimeMillis() < end) {
                for (InetSocketAddress candidate : session.remoteCandidates) sendPacket(hello, candidate);
                sleep(250);
            }
            if (running && !session.connected) emit(PND_EVENT_STATUS, session.peerId,
                    "Прямой маршрут не найден. Эта NAT-сеть может не поддерживать P2P без relay.");
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
            emit(PND_EVENT_CONNECTED, from, peerInfo(session.remoteHello));
        }
        sendPacket(("PH1\n" + session.localHello.toString()).getBytes(StandardCharsets.UTF_8), session.remoteAddress);
    }

    private void receiveMessage(byte[] packet, DatagramPacket source) throws Exception {
        ByteBuffer input = ByteBuffer.wrap(packet).order(ByteOrder.BIG_ENDIAN);
        input.position(4);
        int idLength = input.get() & 255;
        if (idLength != 32 || input.remaining() < idLength + 8 + 8 + 12 + 16) return;
        byte[] idBytes = new byte[idLength];
        input.get(idBytes);
        String from = new String(idBytes, StandardCharsets.US_ASCII);
        Session session = sessions.get(from);
        if (session == null || !session.connected || !session.verified || session.keys == null) return;
        long sequence = input.getLong();
        byte[] sessionId = new byte[8]; input.get(sessionId);
        byte[] nonce = new byte[12]; input.get(nonce);
        byte[] cipher = new byte[input.remaining()]; input.get(cipher);
        if (!MessageDigest.isEqual(sessionId, session.keys.sessionId) || sequence <= session.receiveSequence) return;
        byte[] aad = P2pWire.associatedData(from, ownId, sequence, sessionId);
        byte[] plain = MessengerCrypto.decrypt(session.keys.receive, nonce, aad, cipher);
        if (plain.length > P2pWire.MAX_MESSAGE_BYTES) return;
        session.receiveSequence = sequence;
        session.remoteAddress = new InetSocketAddress(source.getAddress(), source.getPort());
        emit(PND_EVENT_MESSAGE, from, new String(plain, StandardCharsets.UTF_8));
    }

    private void sendPacket(byte[] bytes, InetSocketAddress address) {
        try {
            if (running && socket != null && !socket.isClosed() && address != null)
                socket.send(new DatagramPacket(bytes, bytes.length, address));
        } catch (Exception ignored) { }
    }

    void stop() {
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
    private static boolean starts(byte[] value, String prefix) {
        byte[] p = prefix.getBytes(StandardCharsets.US_ASCII);
        if (value.length < p.length) return false;
        for (int i = 0; i < p.length; i++) if (value[i] != p[i]) return false;
        return true;
    }
    private static String peerInfo(JSONObject hello) throws Exception {
        return P2pWire.fingerprint(hello) + "\n" + hello.getString("nickname");
    }
    private static String cleanNickname(String raw) {
        String source = raw == null ? "" : raw;
        StringBuilder clean = new StringBuilder(32);
        for (int i = 0; i < source.length();) {
            int cp = source.codePointAt(i);
            int chars = Character.charCount(cp);
            if (!Character.isISOControl(cp) && clean.length() + chars <= 32) clean.appendCodePoint(cp);
            i += chars;
        }
        String result = clean.toString().trim();
        return result.isEmpty() ? "Guest" : result;
    }
    private static String normalizeFingerprint(String value) {
        return value == null ? "" : value.replaceAll("\\s+", "").toUpperCase();
    }
    private static boolean sameFingerprint(String first, String second) {
        return normalizeFingerprint(first).equals(normalizeFingerprint(second));
    }
    private static String safeMessage(Exception e) {
        String message = e.getMessage();
        if (message == null || message.trim().isEmpty()) return "Сбой соединения";
        return message.length() > 120 ? message.substring(0, 120) : message;
    }
    private static void sleep(long ms) {
        try { Thread.sleep(ms); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }
}
