package com.ponedelnik.messenger;

import org.json.JSONArray;
import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.Map;

/** Minimal RFC 5389 Binding request; candidates are for direct UDP hole punching only. */
final class StunClient {
    private static final int MAGIC = 0x2112A442;
    private static final String STUN_HOST = "stun.l.google.com";
    private static final int STUN_PORT = 19302;
    private static final int MAX_CANDIDATES = 8;

    private StunClient() { }

    static JSONArray discover(DatagramSocket socket) throws Exception {
        Map<String, Integer> found = new LinkedHashMap<>();
        InetSocketAddress mapped = mappedAddress(socket);
        if (mapped != null) found.put(mapped.getAddress().getHostAddress(), mapped.getPort());
        int localPort = socket.getLocalPort();
        Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
        while (interfaces != null && interfaces.hasMoreElements() && found.size() < MAX_CANDIDATES) {
            NetworkInterface network = interfaces.nextElement();
            if (!network.isUp() || network.isLoopback()) continue;
            Enumeration<InetAddress> addresses = network.getInetAddresses();
            while (addresses.hasMoreElements() && found.size() < MAX_CANDIDATES) {
                InetAddress address = addresses.nextElement();
                if (!(address instanceof Inet4Address) || address.isLoopbackAddress() || address.isLinkLocalAddress()) continue;
                found.put(address.getHostAddress(), localPort);
            }
        }
        JSONArray result = new JSONArray();
        for (Map.Entry<String, Integer> candidate : found.entrySet()) {
            JSONObject item = new JSONObject();
            item.put("host", candidate.getKey());
            item.put("port", candidate.getValue());
            result.put(item);
        }
        if (result.length() == 0) throw new java.io.IOException("no UDP network address available");
        return result;
    }

    static ArrayList<InetSocketAddress> parse(JSONArray values) throws Exception {
        ArrayList<InetSocketAddress> result = new ArrayList<>();
        for (int i = 0; i < values.length() && i < MAX_CANDIDATES; i++) {
            JSONObject value = values.getJSONObject(i);
            String host = value.getString("host");
            int port = value.getInt("port");
            if (!host.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}") || port < 1 || port > 65535) continue;
            InetAddress address = InetAddress.getByName(host);
            if (address instanceof Inet4Address) result.add(new InetSocketAddress(address, port));
        }
        return result;
    }

    private static InetSocketAddress mappedAddress(DatagramSocket socket) {
        byte[] transaction = new byte[12];
        byte[] request = new byte[20];
        new SecureRandom().nextBytes(transaction);
        put16(request, 0, 0x0001);
        put16(request, 2, 0);
        put32(request, 4, MAGIC);
        System.arraycopy(transaction, 0, request, 8, transaction.length);
        try {
            InetAddress server = InetAddress.getByName(STUN_HOST);
            DatagramPacket sent = new DatagramPacket(request, request.length, server, STUN_PORT);
            int oldTimeout = socket.getSoTimeout();
            socket.setSoTimeout(1500);
            socket.send(sent);
            byte[] response = new byte[512];
            DatagramPacket received = new DatagramPacket(response, response.length);
            socket.receive(received);
            socket.setSoTimeout(oldTimeout);
            if (received.getLength() < 20 || get16(response, 0) != 0x0101 || get32(response, 4) != MAGIC) return null;
            for (int i = 0; i < 12; i++) if (response[8 + i] != transaction[i]) return null;
            int end = 20 + get16(response, 2);
            for (int offset = 20; offset + 4 <= end && offset + 4 <= received.getLength();) {
                int type = get16(response, offset), length = get16(response, offset + 2);
                int value = offset + 4;
                if (value + length > received.getLength()) return null;
                if ((type == 0x0020 || type == 0x0001) && length >= 8 && response[value + 1] == 1) {
                    int portMask = type == 0x0020 ? 0x2112 : 0;
                    int addressMask = type == 0x0020 ? MAGIC : 0;
                    int port = get16(response, value + 2) ^ portMask;
                    byte[] ip = new byte[4];
                    for (int i = 0; i < 4; i++)
                        ip[i] = (byte)(response[value + 4 + i] ^ ((addressMask >>> (24 - 8 * i)) & 255));
                    return new InetSocketAddress(InetAddress.getByAddress(ip), port);
                }
                offset = value + ((length + 3) & ~3);
            }
        } catch (Exception ignored) {
            try { socket.setSoTimeout(0); } catch (Exception ignoredAgain) { }
        }
        return null;
    }

    private static int get16(byte[] data, int offset) {
        return ((data[offset] & 255) << 8) | (data[offset + 1] & 255);
    }
    private static int get32(byte[] data, int offset) {
        return ((data[offset] & 255) << 24) | ((data[offset + 1] & 255) << 16)
                | ((data[offset + 2] & 255) << 8) | (data[offset + 3] & 255);
    }
    private static void put16(byte[] data, int offset, int value) {
        data[offset] = (byte)(value >>> 8);
        data[offset + 1] = (byte)value;
    }
    private static void put32(byte[] data, int offset, int value) {
        data[offset] = (byte)(value >>> 24);
        data[offset + 1] = (byte)(value >>> 16);
        data[offset + 2] = (byte)(value >>> 8);
        data[offset + 3] = (byte)value;
    }
}
