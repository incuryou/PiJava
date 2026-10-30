package com.samsung.util;

/**
 * Samsung backlight compatibility API. Like J2ME-Loader and FreeJ2ME, the
 * emulated API is available while the host retains control of its backlight.
 * Requests return immediately and do not alter the game's rendered pixels.
 */
public class LCDLight {
    public static void on(int duration) {}
    public static void off() {}
    public static boolean isSupported() { return true; }
}
