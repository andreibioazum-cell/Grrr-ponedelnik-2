package com.ponedelnik.messenger;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.PublicKey;
import java.security.Signature;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.Locale;

import javax.crypto.Cipher;
import javax.crypto.KeyAgreement;
import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** Android platform crypto: ECDSA identity, ephemeral P-256 ECDH, HKDF-SHA256, AES-GCM. */
final class MessengerCrypto {
    private static final String ID_ALIAS = "ponedelnik.identity.p256.v1";
    private static final String CURVE = "secp256r1";

    private MessengerCrypto() { }

    static KeyPair identity() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (!store.containsAlias(ID_ALIAS)) {
            KeyPairGenerator gen = KeyPairGenerator.getInstance("EC", "AndroidKeyStore");
            KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(ID_ALIAS,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setAlgorithmParameterSpec(new ECGenParameterSpec(CURVE))
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setUserAuthenticationRequired(false)
                    .build();
            gen.initialize(spec);
            gen.generateKeyPair();
            store.load(null);
        }
        java.security.cert.Certificate cert = store.getCertificate(ID_ALIAS);
        java.security.PrivateKey secret = (java.security.PrivateKey) store.getKey(ID_ALIAS, null);
        if (cert == null || secret == null) throw new IllegalStateException("identity key unavailable");
        return new KeyPair(cert.getPublicKey(), secret);
    }

    static KeyPair ephemeral() throws Exception {
        KeyPairGenerator gen = KeyPairGenerator.getInstance("EC");
        gen.initialize(new ECGenParameterSpec(CURVE));
        return gen.generateKeyPair();
    }

    static byte[] sign(KeyPair identity, byte[] data) throws Exception {
        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initSign(identity.getPrivate());
        signature.update(data);
        return signature.sign();
    }

    static boolean verify(byte[] encodedPublic, byte[] data, byte[] sig) throws Exception {
        PublicKey key = KeyFactory.getInstance("EC").generatePublic(new X509EncodedKeySpec(encodedPublic));
        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initVerify(key);
        signature.update(data);
        return signature.verify(sig);
    }

    static byte[] agree(KeyPair ephemeral, byte[] remoteEncoded) throws Exception {
        PublicKey remote = KeyFactory.getInstance("EC").generatePublic(new X509EncodedKeySpec(remoteEncoded));
        KeyAgreement agreement = KeyAgreement.getInstance("ECDH");
        agreement.init(ephemeral.getPrivate());
        agreement.doPhase(remote, true);
        return agreement.generateSecret();
    }

    static byte[] derive(byte[] input, byte[] salt, String info) throws Exception {
        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(salt, "HmacSHA256"));
        byte[] prk = mac.doFinal(input);
        mac.init(new SecretKeySpec(prk, "HmacSHA256"));
        mac.update(info.getBytes(StandardCharsets.UTF_8));
        mac.update((byte) 1);
        byte[] output = mac.doFinal();
        java.util.Arrays.fill(prk, (byte) 0);
        return output;
    }

    static byte[] sha256(byte[] data) throws Exception {
        return MessageDigest.getInstance("SHA-256").digest(data);
    }

    static byte[] encrypt(byte[] key, byte[] nonce, byte[] aad, byte[] plaintext) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"), new GCMParameterSpec(128, nonce));
        cipher.updateAAD(aad);
        return cipher.doFinal(plaintext);
    }

    static byte[] decrypt(byte[] key, byte[] nonce, byte[] aad, byte[] ciphertext) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"), new GCMParameterSpec(128, nonce));
        cipher.updateAAD(aad);
        return cipher.doFinal(ciphertext);
    }

    static String encode(byte[] data) { return Base64.encodeToString(data, Base64.NO_WRAP); }
    static byte[] decode(String data) { return Base64.decode(data, Base64.NO_WRAP); }

    private static javax.crypto.SecretKey storageKey() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        final String alias = "ponedelnik.local-storage.aes.v1";
        if (!store.containsAlias(alias)) {
            KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
            gen.init(new KeyGenParameterSpec.Builder(alias,
                    KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setRandomizedEncryptionRequired(true)
                    .setKeySize(256)
                    .build());
            gen.generateKey();
            store.load(null);
        }
        return (javax.crypto.SecretKey) store.getKey(alias, null);
    }

    static String encryptLocal(String text) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, storageKey());
        byte[] iv = cipher.getIV();
        byte[] encrypted = cipher.doFinal(text.getBytes(StandardCharsets.UTF_8));
        byte[] combined = new byte[iv.length + encrypted.length];
        System.arraycopy(iv, 0, combined, 0, iv.length);
        System.arraycopy(encrypted, 0, combined, iv.length, encrypted.length);
        return encode(combined);
    }

    static String decryptLocal(String stored) throws Exception {
        byte[] combined = decode(stored);
        if (combined.length < 28) throw new IllegalArgumentException("invalid local data");
        byte[] iv = java.util.Arrays.copyOfRange(combined, 0, 12);
        byte[] encrypted = java.util.Arrays.copyOfRange(combined, 12, combined.length);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, storageKey(), new GCMParameterSpec(128, iv));
        return new String(cipher.doFinal(encrypted), StandardCharsets.UTF_8);
    }

    static String fingerprint(byte[] publicKey) throws Exception {
        byte[] digest = sha256(publicKey);
        StringBuilder out = new StringBuilder(79);
        for (int i = 0; i < digest.length; i++) {
            if (i > 0 && i % 2 == 0) out.append(' ');
            out.append(String.format(Locale.US, "%02X", digest[i] & 255));
        }
        return out.toString();
    }
}
