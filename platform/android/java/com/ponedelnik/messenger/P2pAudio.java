package com.ponedelnik.messenger;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.AudioAttributes;
import android.os.Process;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Half-duplex-independent microphone and speaker streams for one encrypted UDP call. */
final class P2pAudio {
    interface Listener {
        void sendVoiceFrame(byte[] frame);
        void onAudioFailure(String message);
    }

    private final Context context;
    private final Listener listener;
    private final AudioManager.OnAudioFocusChangeListener focusListener =
            new AudioManager.OnAudioFocusChangeListener() {
                @Override public void onAudioFocusChange(int focusChange) { }
            };
    private final ArrayBlockingQueue<byte[]> playback = new ArrayBlockingQueue<>(12);
    private volatile boolean running;
    private volatile boolean muted;
    private AudioManager audioManager;
    private AudioRecord recorder;
    private AudioTrack player;
    private int oldAudioMode;
    private boolean oldSpeakerphone;
    private boolean audioFocus;
    private Thread captureThread;
    private Thread playbackThread;

    P2pAudio(Context context, Listener listener) {
        this.context = context.getApplicationContext();
        this.listener = listener;
    }

    synchronized void start() throws Exception {
        if (running) return;
        if (context.checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                != android.content.pm.PackageManager.PERMISSION_GRANTED)
            throw new SecurityException("Разрешите доступ к микрофону для звонка");
        int input = AudioFormat.CHANNEL_IN_MONO;
        int output = AudioFormat.CHANNEL_OUT_MONO;
        int encoding = AudioFormat.ENCODING_PCM_16BIT;
        int minRecord = AudioRecord.getMinBufferSize(VoiceCodec.SAMPLE_RATE, input, encoding);
        int minPlayer = AudioTrack.getMinBufferSize(VoiceCodec.SAMPLE_RATE, output, encoding);
        if (minRecord <= 0 || minPlayer <= 0) throw new IllegalStateException("Аудиоустройство не поддерживает 8 кГц");
        int framePcmBytes = VoiceCodec.FRAME_SAMPLES * 2;
        audioManager = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
        try {
            if (audioManager != null) {
                oldAudioMode = audioManager.getMode();
                oldSpeakerphone = audioManager.isSpeakerphoneOn();
                audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
                audioManager.setSpeakerphoneOn(true);
                audioFocus = audioManager.requestAudioFocus(focusListener, AudioManager.STREAM_VOICE_CALL,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
            }
            recorder = new AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                    VoiceCodec.SAMPLE_RATE, input, encoding, Math.max(minRecord, framePcmBytes * 8));
            AudioFormat format = new AudioFormat.Builder().setSampleRate(VoiceCodec.SAMPLE_RATE)
                    .setChannelMask(output).setEncoding(encoding).build();
            AudioAttributes attributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build();
            player = new AudioTrack.Builder().setAudioAttributes(attributes).setAudioFormat(format)
                    .setBufferSizeInBytes(Math.max(minPlayer, framePcmBytes * 8))
                    .setTransferMode(AudioTrack.MODE_STREAM).build();
            if (recorder.getState() != AudioRecord.STATE_INITIALIZED
                    || player.getState() != AudioTrack.STATE_INITIALIZED)
                throw new IllegalStateException("Не удалось открыть микрофон или аудиовыход");
            running = true;
            player.play();
            recorder.startRecording();
            captureThread = new Thread(new Runnable() {
                @Override public void run() { captureLoop(); }
            }, "pnd-audio-capture");
            playbackThread = new Thread(new Runnable() {
                @Override public void run() { playbackLoop(); }
            }, "pnd-audio-playback");
            captureThread.start();
            playbackThread.start();
        } catch (Exception error) {
            stop();
            throw error;
        }
    }

    void setMuted(boolean value) { muted = value; }
    boolean isMuted() { return muted; }

    void receive(byte[] frame) {
        if (!running || frame == null || frame.length != VoiceCodec.FRAME_BYTES) return;
        byte[] copy = frame.clone();
        if (!playback.offer(copy)) {
            playback.poll();
            playback.offer(copy);
        }
    }

    private void captureLoop() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
        short[] pcm = new short[VoiceCodec.FRAME_SAMPLES];
        int filled = 0;
        try {
            while (running) {
                int count = recorder.read(pcm, filled, pcm.length - filled, AudioRecord.READ_BLOCKING);
                if (count < 0) throw new IllegalStateException("Ошибка чтения микрофона: " + count);
                filled += count;
                if (filled == pcm.length) {
                    if (muted) java.util.Arrays.fill(pcm, (short)0);
                    listener.sendVoiceFrame(VoiceCodec.encode(pcm, pcm.length));
                    filled = 0;
                }
            }
        } catch (Exception error) {
            if (running) listener.onAudioFailure(message(error));
        }
    }

    private void playbackLoop() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
        try {
            while (running) {
                byte[] frame = playback.poll(300, TimeUnit.MILLISECONDS);
                if (frame == null) continue;
                short[] pcm = VoiceCodec.decode(frame);
                int written = 0;
                while (running && written < pcm.length) {
                    int count = player.write(pcm, written, pcm.length - written, AudioTrack.WRITE_BLOCKING);
                    if (count < 0) throw new IllegalStateException("Ошибка воспроизведения: " + count);
                    written += count;
                }
            }
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
        } catch (Exception error) {
            if (running) listener.onAudioFailure(message(error));
        }
    }

    synchronized void stop() {
        if (!running && recorder == null && player == null && audioManager == null) return;
        running = false;
        if (captureThread != null) captureThread.interrupt();
        if (playbackThread != null) playbackThread.interrupt();
        try { if (recorder != null && recorder.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) recorder.stop(); }
        catch (IllegalStateException ignored) { }
        try { if (player != null && player.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) player.stop(); }
        catch (IllegalStateException ignored) { }
        join(captureThread);
        join(playbackThread);
        if (recorder != null) recorder.release();
        if (player != null) player.release();
        recorder = null;
        player = null;
        captureThread = null;
        playbackThread = null;
        playback.clear();
        if (audioManager != null) {
            if (audioFocus) audioManager.abandonAudioFocus(focusListener);
            audioManager.setSpeakerphoneOn(oldSpeakerphone);
            audioManager.setMode(oldAudioMode);
            audioManager = null;
        }
        audioFocus = false;
    }

    private static void join(Thread thread) {
        if (thread == null || thread == Thread.currentThread()) return;
        try { thread.join(250); } catch (InterruptedException error) { Thread.currentThread().interrupt(); }
    }

    private static String message(Exception error) {
        String text = error.getMessage();
        return text == null || text.trim().isEmpty() ? "Аудиопоток остановлен" : text;
    }
}
