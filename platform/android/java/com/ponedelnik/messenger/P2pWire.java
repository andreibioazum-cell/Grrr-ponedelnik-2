package com.ponedelnik.messenger;

import org.json.JSONArray;
import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.util.Arrays;

/** Canonical signed handshake and short UDP message framing for the native app. */
final class P2pWire {
    static final int MAX_MESSAGE_BYTES = 900;
    static final class Keys {
        final byte[] send;
        final byte[] receive;
        final byte[] sessionId;
        Keys(byte[] send, byte[] receive, byte[] sessionId) {
            this.send = send;
            this.receive = receive;
            this.sessionId = sessionId;
        }
    }

    private P2pWire() { }

    static JSONObject hello(String from, String to, String nickname, KeyPair identity, KeyPair ephemeral,
                            JSONArray candidates, String nonce) throws Exception {
        JSONObject value = new JSONObject();
        value.put("version", 1);
        value.put("from", from);
        value.put("to", to);
        value.put("nickname", nickname);
        value.put("identity", MessengerCrypto.encode(identity.getPublic().getEncoded()));
        value.put("ephemeral", MessengerCrypto.encode(ephemeral.getPublic().getEncoded()));
        value.put("nonce", nonce);
        value.put("candidates", candidates);
        value.put("signature", MessengerCrypto.encode(MessengerCrypto.sign(identity,
                canonical(value).getBytes(StandardCharsets.UTF_8))));
        return value;
    }

    static String fingerprint(JSONObject hello) throws Exception {
        return MessengerCrypto.fingerprint(MessengerCrypto.decode(hello.getString("identity")));
    }

    static boolean verify(JSONObject hello, String expectedFrom, String expectedTo) throws Exception {
        String nickname = hello.getString("nickname");
        if (hello.getInt("version") != 1 || !expectedFrom.equals(hello.getString("from"))
                || !expectedTo.equals(hello.getString("to")) || nickname.trim().isEmpty() || nickname.length() > 32)
            return false;
        for (int i = 0; i < nickname.length(); i++)
            if (Character.isISOControl(nickname.charAt(i))) return false;
        byte[] identity = MessengerCrypto.decode(hello.getString("identity"));
        byte[] ephemeral = MessengerCrypto.decode(hello.getString("ephemeral"));
        byte[] nonce = MessengerCrypto.decode(hello.getString("nonce"));
        byte[] sig = MessengerCrypto.decode(hello.getString("signature"));
        if (identity.length > 256 || ephemeral.length > 256 || nonce.length != 16 || sig.length > 128) return false;
        JSONArray list = hello.getJSONArray("candidates");
        if (list.length() < 1 || list.length() > 8) return false;
        for (int i = 0; i < list.length(); i++) {
            JSONObject c = list.getJSONObject(i);
            if (!isIpv4(c.getString("host")) || c.getInt("port") < 1 || c.getInt("port") > 65535)
                return false;
        }
        return MessengerCrypto.verify(identity, canonical(hello).getBytes(StandardCharsets.UTF_8), sig);
    }

    static String canonical(JSONObject hello) throws Exception {
        StringBuilder candidates = new StringBuilder();
        JSONArray list = hello.getJSONArray("candidates");
        for (int i = 0; i < list.length(); i++) {
            JSONObject c = list.getJSONObject(i);
            candidates.append(c.getString("host")).append(':').append(c.getInt("port")).append(',');
        }
        return "PONEDLNIK-P2P-1\n" + hello.getString("from") + '\n' + hello.getString("to") + '\n'
                + hello.getString("nickname") + '\n' + hello.getString("identity") + '\n' + hello.getString("ephemeral") + '\n'
                + hello.getString("nonce") + '\n' + candidates;
    }

    static Keys derive(String ownId, JSONObject local, JSONObject remote, KeyPair ephemeral) throws Exception {
        byte[] shared = MessengerCrypto.agree(ephemeral, MessengerCrypto.decode(remote.getString("ephemeral")));
        JSONObject first = ownId.compareTo(remote.getString("from")) < 0 ? local : remote;
        JSONObject second = first == local ? remote : local;
        byte[] transcript = (canonical(first) + "\n" + canonical(second)).getBytes(StandardCharsets.UTF_8);
        byte[] salt = MessengerCrypto.sha256(transcript);
        byte[] root = MessengerCrypto.derive(shared, salt, "Ponedelnik P2P session v1");
        String idA = first.getString("from"), idB = second.getString("from");
        byte[] keyAB = MessengerCrypto.derive(root, salt, "Ponedelnik direction " + idA + ">" + idB);
        byte[] keyBA = MessengerCrypto.derive(root, salt, "Ponedelnik direction " + idB + ">" + idA);
        byte[] sessionId = Arrays.copyOf(salt, 8);
        Arrays.fill(shared, (byte) 0);
        Arrays.fill(root, (byte) 0);
        return ownId.equals(idA) ? new Keys(keyAB, keyBA, sessionId) : new Keys(keyBA, keyAB, sessionId);
    }

    private static boolean isIpv4(String host) {
        if (host == null) return false;
        String[] parts = host.split("\\.");
        if (parts.length != 4) return false;
        for (String part : parts) {
            if (part.isEmpty() || part.length() > 3) return false;
            try {
                int value = Integer.parseInt(part);
                if (value < 0 || value > 255) return false;
            } catch (NumberFormatException e) { return false; }
        }
        return true;
    }

    static byte[] associatedData(String from, String to, long sequence, byte[] sessionId) {
        return ("PDM1|" + from + "|" + to + "|" + sequence + "|" + MessengerCrypto.encode(sessionId))
                .getBytes(StandardCharsets.UTF_8);
    }
}
