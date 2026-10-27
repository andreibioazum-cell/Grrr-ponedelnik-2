package com.ponedelnik.messenger;

import android.app.NativeActivity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.FrameLayout;

public final class MessengerActivity extends NativeActivity {

    private static boolean nativeReady;
    static {
        try {
            System.loadLibrary("ponedelnik_messenger");
            nativeReady = true;
        } catch (UnsatisfiedLinkError error) {
            nativeReady = false;
        }
    }

    private EditText chatEditor;
    private boolean syncingFromNative;
    private boolean keyboardWasVisible;

    private volatile boolean wantKeyboard;

    private volatile boolean editorActive;

    private volatile boolean imeLooksVisible;
    private int showAttempts;

    private long lastDeleteAt;
    private int deleteStreak;
    private boolean pendingDelete;

    private native void nativeReplaceText(String text);
    private native void nativeSubmitText();
    private native void nativeKeyboardHidden();
    private native void nativeMessengerEvent(int type, String peer, String data);
    private P2pMessenger messengerTransport;

    void emitMessengerEvent(int type, String peer, String data) {
        if (nativeReady) try { nativeMessengerEvent(type, peer, data); } catch (UnsatisfiedLinkError ignored) { }
    }

    public void startMessenger(String serverUrl, String nickname) {
        stopMessenger();
        messengerTransport = new P2pMessenger(this);
        messengerTransport.start(serverUrl, nickname);
    }

    public void connectMessenger(String peerId) {
        if (messengerTransport != null) messengerTransport.connect(peerId);
    }

    public void acceptMessenger(String peerId) {
        if (messengerTransport != null) messengerTransport.accept(peerId);
    }

    public void verifyMessenger(String peerId, String fingerprint) {
        if (messengerTransport != null) messengerTransport.verify(peerId, fingerprint);
    }

    public void sendMessenger(String peerId, String message) {
        if (messengerTransport != null) messengerTransport.send(peerId, message);
    }

    public void shareMessenger(final String peerId, final String fingerprint) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                android.content.Intent intent = new android.content.Intent(android.content.Intent.ACTION_SEND);
                intent.setType("text/plain");
                intent.putExtra(android.content.Intent.EXTRA_TEXT,
                        "Ponedelnik Peer ID: " + peerId + "\nKey fingerprint: " + fingerprint);
                startActivity(android.content.Intent.createChooser(intent, "Share secure contact"));
            }
        });
    }

    public void stopMessenger() {
        if (messengerTransport != null) {
            messengerTransport.stop();
            messengerTransport = null;
        }
    }

    private void replaceTextNative(String text) {
        if (nativeReady) try { nativeReplaceText(text); } catch (UnsatisfiedLinkError ignored) { }
    }
    private void submitTextNative() {
        if (nativeReady) try { nativeSubmitText(); } catch (UnsatisfiedLinkError ignored) { }
    }
    private void keyboardHiddenNative() {
        if (nativeReady) try { nativeKeyboardHidden(); } catch (UnsatisfiedLinkError ignored) { }
    }

    @SuppressWarnings("deprecation")
    private void enterImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        enterImmersiveMode();

        chatEditor = new EditText(this);
        chatEditor.setSingleLine(true);

        chatEditor.setTextColor(Color.TRANSPARENT);
        chatEditor.setHintTextColor(Color.TRANSPARENT);
        chatEditor.setBackgroundColor(Color.TRANSPARENT);
        chatEditor.setCursorVisible(false);
        chatEditor.setAlpha(1f);
        chatEditor.setGravity(Gravity.TOP | Gravity.START);
        chatEditor.setFocusable(true);
        chatEditor.setFocusableInTouchMode(true);
        chatEditor.setClickable(false);
        chatEditor.setLongClickable(false);

        chatEditor.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, android.view.MotionEvent event) {
                if (event.getAction() == android.view.MotionEvent.ACTION_DOWN) {
                    hideMessengerKeyboard();
                }
                return true;
            }
        });

        chatEditor.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                | InputType.TYPE_TEXT_VARIATION_FILTER);
        chatEditor.setImeOptions(EditorInfo.IME_ACTION_DONE
                | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN);
        chatEditor.setFilters(new InputFilter[] { new InputFilter.LengthFilter(360) });
        chatEditor.setVisibility(View.INVISIBLE);

        float density = getResources().getDisplayMetrics().density;
        int editorH = Math.max(48, (int) (48f * density));
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, editorH, Gravity.TOP);
        addContentView(chatEditor, params);

        chatEditor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {

                pendingDelete = !syncingFromNative && count > 0 && after == 0;
            }
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) { }
            @Override public void afterTextChanged(Editable value) {
                if (syncingFromNative) return;
                replaceTextNative(value.toString());
                if (pendingDelete) {
                    long now = android.os.SystemClock.elapsedRealtime();
                    deleteStreak = (now - lastDeleteAt <= 200) ? deleteStreak + 1 : 1;
                    lastDeleteAt = now;
                    pendingDelete = false;

                    if (deleteStreak >= 6 && chatEditor.length() > 0) {
                        deleteStreak = 0;
                        chatEditor.post(new Runnable() {
                            @Override public void run() {
                                if (chatEditor.length() > 0) chatEditor.setText("");
                            }
                        });
                    }
                } else {
                    deleteStreak = 0;
                }
            }
        });
        chatEditor.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View view, boolean focused) {
                if (focused) {
                    editorActive = nativeReady && wantKeyboard
                            && chatEditor.getVisibility() == View.VISIBLE;
                    return;
                }

                if (wantKeyboard && chatEditor.getVisibility() == View.VISIBLE) {
                    chatEditor.post(new Runnable() {
                        @Override public void run() { claimEditorFocus(); }
                    });
                } else {
                    editorActive = false;
                }
            }
        });
        chatEditor.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
                boolean enter = actionId == EditorInfo.IME_ACTION_SEND
                        || actionId == EditorInfo.IME_ACTION_DONE
                        || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                            && event.getAction() == KeyEvent.ACTION_DOWN);
                if (enter) {
                    submitTextNative();
                    return true;
                }
                return false;
            }
        });

        chatEditor.getRootView().getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        View root = chatEditor.getRootView();
                        Rect visible = new Rect();
                        root.getWindowVisibleDisplayFrame(visible);
                        boolean keyboardVisible = root.getHeight() - visible.bottom
                                > root.getHeight() * 0.15f;
                        imeLooksVisible = keyboardVisible;
                        if (wantKeyboard) claimEditorFocus();
                        if (keyboardVisible) {
                            keyboardWasVisible = true;
                        } else if (keyboardWasVisible) {
                            keyboardWasVisible = false;
                            keyboardHiddenNative();
                        }
                    }
                });
    }

    private void claimEditorFocus() {
        if (chatEditor == null || !wantKeyboard) return;
        if (chatEditor.getVisibility() != View.VISIBLE) chatEditor.setVisibility(View.VISIBLE);
        if (!chatEditor.hasFocus()) chatEditor.requestFocus();
        editorActive = nativeReady;
    }

    public void showMessengerKeyboard(final String currentText) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null) return;
                wantKeyboard = true;
                chatEditor.setVisibility(View.VISIBLE);
                chatEditor.bringToFront();
                replaceEditorText(currentText);
                claimEditorFocus();
                getWindow().setSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                if (imeLooksVisible) {

                    claimEditorFocus();
                    return;
                }
                showAttempts = 0;
                requestShowWhenReady();
            }
        });
    }

    private void requestShowWhenReady() {
        if (chatEditor == null) return;
        final int attempt = showAttempts++;
        if (attempt >= 10) return;
        chatEditor.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null || !wantKeyboard || imeLooksVisible) return;
                claimEditorFocus();
                InputMethodManager input = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (input != null) {
                    if (!input.isActive(chatEditor)) chatEditor.requestFocus();
                    input.showSoftInput(chatEditor, InputMethodManager.SHOW_FORCED);
                }
                requestShowWhenReady();
            }
        }, attempt == 0 ? 60 : 120);
    }

    public void setMessengerKeyboardText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                replaceEditorText(text);
                if (wantKeyboard) claimEditorFocus();
            }
        });
    }

    public boolean messengerKeyboardActive() {
        return wantKeyboard;
    }

    public void hideMessengerKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null) return;
                wantKeyboard = false;
                InputMethodManager input = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (input != null) {
                    input.hideSoftInputFromWindow(chatEditor.getWindowToken(), 0);
                }
                editorActive = false;
                imeLooksVisible = false;
                showAttempts = 10;
                getWindow().setSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                chatEditor.clearFocus();
                chatEditor.setVisibility(View.INVISIBLE);

                keyboardHiddenNative();
            }
        });
    }

    private void replaceEditorText(String text) {
        if (chatEditor == null) return;
        String safe = text == null ? "" : text;
        if (safe.contentEquals(chatEditor.getText())) return;
        boolean shrinking = safe.length() < chatEditor.length();
        syncingFromNative = true;
        chatEditor.setText(safe);
        chatEditor.setSelection(chatEditor.length());
        syncingFromNative = false;

        if (shrinking) {
            InputMethodManager input = (InputMethodManager)
                    getSystemService(Context.INPUT_METHOD_SERVICE);
            if (input != null) input.restartInput(chatEditor);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) enterImmersiveMode();
        if (hasFocus && wantKeyboard && chatEditor != null) {
            claimEditorFocus();
            if (!imeLooksVisible) {
                showAttempts = 0;
                requestShowWhenReady();
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        enterImmersiveMode();
    }

    @Override
    protected void onPause() {
        hideMessengerKeyboard();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        stopMessenger();
        super.onDestroy();
    }
}
