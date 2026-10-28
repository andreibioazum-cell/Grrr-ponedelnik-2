package com.ponedelnik.messenger;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

/** HTTPS/HTTP transport for short-lived offer/answer metadata only. */
final class SignalClient {
    private final String baseUrl;
    private final String peerId;
    private final String token;

    SignalClient(String baseUrl, String peerId, String token) {
        String trimmed = baseUrl == null ? "" : baseUrl.trim();
        while (trimmed.endsWith("/")) trimmed = trimmed.substring(0, trimmed.length() - 1);
        if (!trimmed.startsWith("https://") && !trimmed.startsWith("http://"))
            throw new IllegalArgumentException("URL must start with https:// (http:// is local-test only)");
        this.baseUrl = trimmed;
        this.peerId = peerId;
        this.token = token;
    }

    static JSONObject register(String baseUrl) throws Exception {
        return request(baseUrl, "/v1/register", null, new JSONObject(), false, 8000, 8000);
    }

    JSONObject poll() throws Exception {
        String id = URLEncoder.encode(peerId, "UTF-8");
        return request(baseUrl, "/v1/poll?id=" + id, null, null, true, 8000, 24000);
    }

    JSONObject send(String target, String kind, JSONObject payload) throws Exception {
        JSONObject body = new JSONObject();
        body.put("from", peerId);
        body.put("to", target);
        body.put("kind", kind);
        body.put("payload", payload);
        return request(baseUrl, "/v1/signal", token, body, false, 8000, 8000);
    }

    private static JSONObject request(String base, String path, String token, JSONObject body,
                                      boolean get, int connectTimeout, int readTimeout) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(base + path).openConnection();
        connection.setConnectTimeout(connectTimeout);
        connection.setReadTimeout(readTimeout);
        connection.setUseCaches(false);
        connection.setRequestProperty("Accept", "application/json");
        connection.setRequestProperty("Cache-Control", "no-store");
        if (token != null) connection.setRequestProperty("Authorization", "Bearer " + token);
        if (get) {
            connection.setRequestMethod("GET");
        } else {
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] bytes = (body == null ? "{}" : body.toString()).getBytes(StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(bytes.length);
            try (OutputStream output = connection.getOutputStream()) { output.write(bytes); }
        }
        int status = connection.getResponseCode();
        InputStream input = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
        byte[] bytes = readBounded(input, 20 * 1024);
        connection.disconnect();
        if (status < 200 || status >= 300) {
            String message = new String(bytes, StandardCharsets.UTF_8);
            throw new java.io.IOException("signaling HTTP " + status + ": " + message);
        }
        return new JSONObject(new String(bytes, StandardCharsets.UTF_8));
    }

    private static byte[] readBounded(InputStream input, int maxBytes) throws Exception {
        if (input == null) return new byte[0];
        try (InputStream in = input; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[2048];
            int count;
            while ((count = in.read(buffer)) != -1) {
                if (out.size() + count > maxBytes) throw new java.io.IOException("signaling reply too large");
                out.write(buffer, 0, count);
            }
            return out.toByteArray();
        }
    }
}
