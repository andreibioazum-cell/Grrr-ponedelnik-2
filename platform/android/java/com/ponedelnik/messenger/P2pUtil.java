package com.ponedelnik.messenger;

import org.json.JSONObject;

import java.nio.charset.StandardCharsets;

final class P2pUtil {
    private P2pUtil() { }

    static void validateServerUrl(String value) throws Exception {
        java.net.URI uri = new java.net.URI(value);
        String scheme = uri.getScheme(), host = uri.getHost();
        if (host == null || !("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme)))
            throw new IllegalArgumentException("укажи корректный URL сигналинга");
        if ("https".equalsIgnoreCase(scheme)) return;
        String address = host.toLowerCase(java.util.Locale.US);
        if (address.equals("localhost") || address.equals("10.0.2.2")) return;
        if (!address.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}"))
            throw new SecurityException("для внешнего сервера обязателен HTTPS");
        String[] parts = address.split("\\.");
        int a = Integer.parseInt(parts[0]), b = Integer.parseInt(parts[1]);
        int c = Integer.parseInt(parts[2]), d = Integer.parseInt(parts[3]);
        if (a > 255 || b > 255 || c > 255 || d > 255) throw new SecurityException("неверный локальный IP");
        boolean local = a == 10 || a == 127 || (a == 192 && b == 168)
                || (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254);
        if (!local) throw new SecurityException("для внешнего сервера обязателен HTTPS");
    }

    static String cleanNickname(String raw) {
        String source = raw == null ? "" : raw;
        StringBuilder clean = new StringBuilder(32);
        for (int i = 0; i < source.length();) {
            int cp = source.codePointAt(i), chars = Character.charCount(cp);
            if (!Character.isISOControl(cp) && clean.length() + chars <= 32) clean.appendCodePoint(cp);
            i += chars;
        }
        String result = clean.toString().trim();
        return result.isEmpty() ? "Guest" : result;
    }

    static String normalizeFingerprint(String value) {
        return value == null ? "" : value.replaceAll("\\s+", "").toUpperCase(java.util.Locale.US);
    }

    static String safeMessage(Exception error) {
        String message = error.getMessage();
        if (message == null || message.trim().isEmpty()) return "Сбой соединения";
        return message.length() > 120 ? message.substring(0, 120) : message;
    }

    static void sleep(long ms) {
        try { Thread.sleep(ms); } catch (InterruptedException error) { Thread.currentThread().interrupt(); }
    }

    static String peerInfo(JSONObject hello) throws Exception {
        return P2pWire.fingerprint(hello) + "\n" + hello.getString("nickname");
    }

    static boolean starts(byte[] value, String prefix) {
        byte[] marker = prefix.getBytes(StandardCharsets.US_ASCII);
        if (value.length < marker.length) return false;
        for (int i = 0; i < marker.length; i++) if (value[i] != marker[i]) return false;
        return true;
    }

}
