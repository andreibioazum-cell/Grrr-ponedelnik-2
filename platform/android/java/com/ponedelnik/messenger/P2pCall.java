package com.ponedelnik.messenger;

import android.content.Context;

import java.security.SecureRandom;
import java.util.Locale;

/** Authenticated in-band call signalling and audio lifecycle for one P2P peer. */
final class P2pCall {
    static final int EVENT_INCOMING = 10;
    static final int EVENT_STATE = 11;
    static final int EVENT_ERROR = 12;
    private static final int IDLE = 0;
    private static final int OUTGOING = 1;
    private static final int INCOMING = 2;
    private static final int ACTIVE = 3;

    interface Sink {
        boolean sendControl(String peer, String message);
        void sendVoice(String peer, byte[] frame);
        void emit(int type, String peer, String data);
    }

    private final Context context;
    private final Sink sink;
    private final SecureRandom random = new SecureRandom();
    private int state;
    private String peer = "";
    private String callId = "";
    private P2pAudio audio;

    P2pCall(Context context, Sink sink) {
        this.context = context;
        this.sink = sink;
    }

    synchronized void call(String target) {
        if (state != IDLE) { error(target, "Уже выполняется другой звонок"); return; }
        peer = target;
        callId = newCallId();
        state = OUTGOING;
        if (!send("CALL")) { reset(); error(target, "Не удалось отправить запрос на звонок"); return; }
        sink.emit(EVENT_STATE, peer, "outgoing");
    }

    synchronized void accept(String target) {
        if (state != INCOMING || !peer.equals(target)) { error(target, "Входящий звонок уже завершён"); return; }
        try {
            startAudio();
            if (!send("ACCEPT")) throw new IllegalStateException("Не удалось подтвердить звонок");
            state = ACTIVE;
            sink.emit(EVENT_STATE, peer, "active");
        } catch (Exception failure) {
            send("END");
            reset();
            error(target, message(failure));
        }
    }

    synchronized void decline(String target) {
        if (state != INCOMING || !peer.equals(target)) return;
        send("DECLINE");
        reset();
        sink.emit(EVENT_STATE, target, "declined");
    }

    synchronized void end(String target) {
        if (state == IDLE || !peer.equals(target)) return;
        send("END");
        reset();
        sink.emit(EVENT_STATE, target, "ended");
    }

    synchronized void setMuted(String target, boolean value) {
        if (state != ACTIVE || !peer.equals(target) || audio == null) return;
        audio.setMuted(value);
        sink.emit(EVENT_STATE, target, value ? "muted" : "unmuted");
    }

    synchronized void receiveControl(String from, String value) {
        String[] parts = value.split("\\|", -1);
        if (parts.length != 2 || !validId(parts[1])) return;
        String kind = parts[0];
        String incomingId = parts[1];
        if ("CALL".equals(kind)) {
            if (state != IDLE) {
                if (!peer.equals(from) || !callId.equals(incomingId))
                    sink.sendControl(from, "BUSY|" + incomingId);
                return;
            }
            peer = from;
            callId = incomingId;
            state = INCOMING;
            sink.emit(EVENT_INCOMING, from, "");
        } else if (!peer.equals(from) || !callId.equals(incomingId)) {
            return;
        } else if ("ACCEPT".equals(kind) && state == OUTGOING) {
            try {
                startAudio();
                state = ACTIVE;
                sink.emit(EVENT_STATE, peer, "active");
            } catch (Exception failure) {
                send("END");
                reset();
                error(from, message(failure));
            }
        } else if ("DECLINE".equals(kind) && state == OUTGOING) {
            reset();
            sink.emit(EVENT_STATE, from, "declined");
        } else if ("BUSY".equals(kind) && state == OUTGOING) {
            reset();
            sink.emit(EVENT_STATE, from, "busy");
        } else if ("END".equals(kind) && state != IDLE) {
            reset();
            sink.emit(EVENT_STATE, from, "ended");
        }
    }

    synchronized void receiveVoice(String from, byte[] frame) {
        if (state == ACTIVE && peer.equals(from) && audio != null) audio.receive(frame);
    }

    synchronized void stop() {
        if (state != IDLE) send("END");
        reset();
    }

    private void startAudio() throws Exception {
        P2pAudio next = new P2pAudio(context, new P2pAudio.Listener() {
            @Override public void sendVoiceFrame(byte[] frame) { sendAudio(frame); }
            @Override public void onAudioFailure(String text) { audioFailed(text); }
        });
        next.start();
        audio = next;
    }

    private synchronized void sendAudio(byte[] frame) {
        if (state == ACTIVE && audio != null) sink.sendVoice(peer, frame);
    }

    private synchronized void audioFailed(String text) {
        String failedPeer = peer;
        if (state == IDLE) return;
        send("END");
        reset();
        error(failedPeer, text);
    }

    private boolean send(String kind) {
        return !peer.isEmpty() && !callId.isEmpty() && sink.sendControl(peer, kind + "|" + callId);
    }

    private void reset() {
        P2pAudio old = audio;
        audio = null;
        state = IDLE;
        peer = "";
        callId = "";
        if (old != null) old.stop();
    }

    private void error(String target, String text) { sink.emit(EVENT_ERROR, target, text); }

    private String newCallId() {
        byte[] bytes = new byte[16];
        random.nextBytes(bytes);
        StringBuilder out = new StringBuilder(32);
        for (byte value : bytes) out.append(String.format(Locale.US, "%02X", value & 255));
        return out.toString();
    }

    private static boolean validId(String value) { return value.matches("[0-9A-F]{32}"); }
    private static String message(Exception failure) {
        String value = failure.getMessage();
        return value == null || value.trim().isEmpty() ? "Не удалось запустить аудиовызов" : value;
    }
}
