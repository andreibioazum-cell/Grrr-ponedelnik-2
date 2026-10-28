# Ponedelnik Messenger

Ponedelnik Messenger is a native Android peer-to-peer messenger with a C/Vulkan interface. The Android entry point runs as a `NativeActivity`; Java is limited to Android platform services such as networking, cryptography, runtime microphone permission, and audio input/output. The Android APK links the messenger and shared Vulkan renderer only; it does not compile or package any game target, Firebase, or a web UI.

## What is implemented

- End-to-end encrypted text over direct UDP using authenticated AES-GCM packets and per-session keys.
- Signed peer handshakes, ephemeral key agreement, device key fingerprints, and optional locally pinned contacts.
- Direct voice calls over the same peer-to-peer UDP session. Call control and G.711 μ-law voice frames use separate authenticated packet channels; audio uses 8 kHz mono, 20 ms frames.
- A small rendezvous/signaling service that stores peer registrations and queued signed setup invitations in memory only. It never accepts chat text or voice. STUN is used for address discovery and UDP hole punching; there is deliberately no TURN/media relay.
- Native C/Vulkan screens for connecting, exchanging peer IDs, verifying fingerprints, chatting, and controlling calls.

P2P is best-effort: restrictive or symmetric NATs, carrier firewalls, and networks that block UDP can prevent a direct connection. Those cases are not relayed. The signaling service can see registration and setup metadata, including peer IDs and network candidates, but it does not receive E2EE chat messages or audio. Restarting the sample service clears its in-memory registrations and pending invitations.

## Android APK build

The GitHub Actions workflow installs the Android SDK/NDK, compiles all Android Java sources and DEX, builds the native Vulkan library for `arm64-v8a`, `armeabi-v7a`, and `x86_64`, and packages a signed **debug** APK. Download the `Ponedelnik-Messenger-Android` workflow artifact to install it for testing. The workflow signing key is temporary; this is not a Play Store release key.

The native library can be built locally with CMake and the Android NDK. For example:

```sh
cmake -S . -B build/android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-arm64 --parallel
```

This produces `build/android-arm64/libponedelnik_messenger.so`. Packaging the full APK additionally requires the Android SDK platform 35, Build Tools 34.0.0, Java 17, and D8; the workflow is the reference end-to-end APK build.

## Signaling setup

For two real devices, enter the HTTPS URL of a publicly reachable signaling service in the app. The included Python service can be run on a private LAN for development:

```sh
python3 server/signaling.py --host 0.0.0.0 --port 8765 --dev-http
```

Do not expose cleartext HTTP on the public Internet. For a public deployment, supply a TLS certificate and key to the service (or terminate HTTPS in a correctly configured reverse proxy). The app accepts cleartext only for emulator/local-network development and requires HTTPS for external signaling URLs. The default `http://10.0.2.2:8765` address is for the Android emulator, not physical phones.

The signaling process is a minimal, in-memory rendezvous example and is not intended as a hardened or highly available public service. Keep the TLS private key, deployment logs, and operational controls outside the repository.

## Tests

Run the host-side checks with Python 3; Java 17 is needed for the G.711 codec test:

```sh
python3 tools/test_messenger_layout.py
python3 tools/test_messenger_assets.py
python3 tools/test_messenger_native.py
python3 tools/test_signaling.py
python3 tools/test_voice_codec.py
python3 tools/test_pona.py
python3 tools/test_stage_assets.py
python3 tools/check_source_file_size.py
```

The CI pipeline also compiles every Android Java source against the Android SDK, converts the classes to DEX, and builds/packages all three Android ABIs. Microphone, speaker routing, NAT traversal, and real device-to-device call quality still require on-device testing.
