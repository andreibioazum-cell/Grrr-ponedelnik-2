static void draw_call_panel(float x, float w) {
    char label[64];
    roundrect(x, u(108), w, u(112), u(20), 0xFF151B30u);
    if (call_state == 1) {
        text_scaled("Звоним собеседнику…", x + u(16), u(124), 0xFFFFFFFFu, u(0.43f));
        draw_button("Отменить", x + u(16), u(163), w - u(32), u(40), 0);
    } else if (call_state == 2) {
        text_scaled("Входящий голосовой звонок", x + u(16), u(120), 0xFFFFFFFFu, u(0.43f));
        draw_button("Принять", x + u(12), u(164), w * 0.5f - u(18), u(40), 1);
        draw_button("Отклонить", x + w * 0.5f + u(6), u(164), w * 0.5f - u(18), u(40), 0);
    } else if (call_state == 3) {
        int seconds = (int)call_elapsed;
        snprintf(label, sizeof(label), "Звонок · %02d:%02d", seconds / 60, seconds % 60);
        text_scaled(label, x + u(16), u(120), 0xFFFFFFFFu, u(0.43f));
        draw_button(call_muted ? "Микрофон" : "Без звука", x + u(12), u(164), w * 0.5f - u(18), u(40), 0);
        draw_button("Завершить", x + w * 0.5f + u(6), u(164), w * 0.5f - u(18), u(40), 1);
    }
}
