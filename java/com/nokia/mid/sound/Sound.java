package com.nokia.mid.sound;

import java.io.ByteArrayInputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.VolumeControl;

/** Nokia sound adapter for real PCM WAV playback and frequency tones. */
public class Sound {
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 5;
    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 3;
    private Player player;
    private SoundListener listener;
    private int gain = 255;
    public Sound(int freq, long duration) { init(freq, duration); }
    public Sound(byte[] data, int type) { init(data, type); }
    public void init(byte[] data, int type) {
        if (data == null || (type != FORMAT_WAV && type != FORMAT_TONE)) throw new IllegalArgumentException("unsupported Nokia sound format");
        Player next;
        try {
            next = Manager.createPlayer(new ByteArrayInputStream(data), type == FORMAT_TONE ? "audio/x-nokia-ota" : "audio/wav");
            try { next.realize(); }
            catch (Exception failure) { next.close(); throw failure; }
        } catch (Exception failure) { throw new IllegalArgumentException("invalid or unsupported Nokia sound"); }
        if (player != null) player.close();
        player = next;
        ((VolumeControl) player.getControl("VolumeControl")).setLevel(gain * 100 / 255);
        player.addPlayerListener(new PlayerListener() {
            public void playerUpdate(Player source, String event, Object data) {
                if (source != player || listener == null) return;
                if (event.equals(STARTED)) notifyState(SOUND_PLAYING);
                else if (event.equals(STOPPED) || event.equals(END_OF_MEDIA)) notifyState(SOUND_STOPPED);
            }
        });
        notifyState(SOUND_STOPPED);
    }
    public void init(int freq, long duration) {
        if (freq < 1 || freq > 20000 || duration <= 0 || duration > 95000)
            throw new IllegalArgumentException("tone range");
        int frames = (int) (duration * 44100L / 1000L);
        byte[] data = new byte[44 + frames * 2];
        data[0] = 'R'; data[1] = 'I'; data[2] = 'F'; data[3] = 'F';
        put32(data, 4, data.length - 8);
        data[8] = 'W'; data[9] = 'A'; data[10] = 'V'; data[11] = 'E';
        data[12] = 'f'; data[13] = 'm'; data[14] = 't'; data[15] = ' ';
        put32(data, 16, 16); put16(data, 20, 1); put16(data, 22, 1);
        put32(data, 24, 44100); put32(data, 28, 88200); put16(data, 32, 2); put16(data, 34, 16);
        data[36] = 'd'; data[37] = 'a'; data[38] = 't'; data[39] = 'a';
        put32(data, 40, frames * 2);
        int phase = 0;
        int i = 0;
        while (i < frames) {
            put16(data, 44 + i * 2, phase < 22050 ? 8192 : -8192);
            phase = (phase + freq) % 44100;
            i++;
        }
        init(data, FORMAT_WAV);
    }
    private static void put16(byte[] data, int at, int value) {
        data[at] = (byte) value; data[at + 1] = (byte) (value >> 8);
    }
    private static void put32(byte[] data, int at, int value) {
        put16(data, at, value); put16(data, at + 2, value >> 16);
    }
    public void play(int loop) {
        if (loop < 0) throw new IllegalArgumentException("loop count");
        if (player == null) return;
        try {
            player.stop();
            player.setMediaTime(0);
            player.setLoopCount(loop == 0 ? -1 : loop);
            player.start();
        } catch (javax.microedition.media.MediaException failure) { throw new IllegalStateException("cannot play sound"); }
    }
    public void stop() {
        if (player == null) return;
        try { player.stop(); }
        catch (javax.microedition.media.MediaException failure) { throw new IllegalStateException("cannot stop sound"); }
    }
    public void resume() {
        if (player == null) return;
        try { player.start(); }
        catch (javax.microedition.media.MediaException failure) { throw new IllegalStateException("cannot resume sound"); }
    }
    public void release() {
        if (player == null) return;
        Player old = player;
        player = null;
        old.close();
        notifyState(SOUND_UNINITIALIZED);
    }
    public int getState() {
        return player == null ? SOUND_UNINITIALIZED : player.getState() == Player.STARTED ? SOUND_PLAYING : SOUND_STOPPED;
    }
    public void setGain(int value) {
        gain = value < 0 ? 0 : value > 255 ? 255 : value;
        if (player != null) ((VolumeControl) player.getControl("VolumeControl")).setLevel(gain * 100 / 255);
    }
    public int getGain() { return gain; }
    public void setSoundListener(SoundListener next) { listener = next; }
    private void notifyState(int state) {
        if (listener == null) return;
        try { listener.soundStateChanged(this, state); } catch (RuntimeException ignored) { }
    }
    public static int getConcurrentSoundCount(int type) { return type == FORMAT_WAV || type == FORMAT_TONE ? 64 : 0; }
    public static int[] getSupportedFormats() { return new int[] { FORMAT_TONE, FORMAT_WAV }; }
}
