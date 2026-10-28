package com.ponedelnik.messenger;

/** G.711 mu-law encoder/decoder for low-bandwidth 8 kHz voice frames. */
final class VoiceCodec {
    static final int SAMPLE_RATE = 8000;
    static final int FRAME_SAMPLES = 160;
    static final int FRAME_MILLIS = 20;
    static final int FRAME_BYTES = 160;
    private static final int BIAS = 0x84;
    private static final int CLIP = 32635;
    private static final int[] SEGMENT_END = {
        0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF
    };

    private VoiceCodec() { }

    static byte[] encode(short[] pcm, int count) {
        if (pcm == null || count < 1 || count > pcm.length) throw new IllegalArgumentException("invalid PCM frame");
        byte[] encoded = new byte[count];
        for (int i = 0; i < count; i++) encoded[i] = encodeSample(pcm[i]);
        return encoded;
    }

    static short[] decode(byte[] encoded) {
        if (encoded == null || encoded.length == 0) throw new IllegalArgumentException("empty voice frame");
        short[] pcm = new short[encoded.length];
        for (int i = 0; i < encoded.length; i++) pcm[i] = decodeSample(encoded[i]);
        return pcm;
    }

    static byte encodeSample(short sample) {
        int value = sample;
        int mask;
        if (value < 0) {
            value = BIAS - value;
            mask = 0x7F;
        } else {
            value += BIAS;
            mask = 0xFF;
        }
        if (value > CLIP) value = CLIP;
        int segment = 0;
        while (segment < SEGMENT_END.length && value > SEGMENT_END[segment]) segment++;
        if (segment >= SEGMENT_END.length) segment = SEGMENT_END.length - 1;
        int code = (segment << 4) | ((value >> (segment + 3)) & 0x0F);
        return (byte)(code ^ mask);
    }

    static short decodeSample(byte code) {
        int value = (~code) & 0xFF;
        int magnitude = ((value & 0x0F) << 3) + BIAS;
        magnitude <<= (value & 0x70) >> 4;
        return (short)((value & 0x80) != 0 ? BIAS - magnitude : magnitude - BIAS);
    }
}
