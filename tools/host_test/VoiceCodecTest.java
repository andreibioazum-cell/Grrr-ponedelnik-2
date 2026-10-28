package com.ponedelnik.messenger;

public final class VoiceCodecTest {
    public static void main(String[] args) {
        require((VoiceCodec.encodeSample((short)0) & 255) == 255, "mu-law silence code");
        require(VoiceCodec.decodeSample((byte)0xFF) == 0, "mu-law silence decode");
        short[] samples = {-30000, -10000, -1000, -100, 0, 100, 1000, 10000, 30000};
        for (short sample : samples) {
            short decoded = VoiceCodec.decodeSample(VoiceCodec.encodeSample(sample));
            require(Math.abs((int)decoded - sample) <= 2500, "round-trip range at " + sample);
            if (sample < 0) require(decoded < 0, "negative polarity at " + sample);
            if (sample > 0) require(decoded > 0, "positive polarity at " + sample);
        }
        short[] frame = new short[VoiceCodec.FRAME_SAMPLES];
        for (int i = 0; i < frame.length; i++) frame[i] = (short)(Math.sin(i * 0.12) * 12000);
        byte[] encoded = VoiceCodec.encode(frame, frame.length);
        require(encoded.length == VoiceCodec.FRAME_BYTES, "20 ms frame size");
        require(VoiceCodec.decode(encoded).length == VoiceCodec.FRAME_SAMPLES, "decoded frame size");
        System.out.println("voice codec: G.711 mu-law vectors and 20 ms frame pass");
    }

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
}
