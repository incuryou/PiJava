package javax.microedition.media;

/** Decoded MIDI/PCM player. End events are delivered by the VM's main-thread pump. */
public final class NativePlayer implements Player {
    private static final NativePlayer[] players = new NativePlayer[64];
    private byte[] source;
    private final String contentType;
    private int handle;
    private int state = UNREALIZED;
    private int loopCount = 1;
    private PlayerListener[] listeners = new PlayerListener[0];
    private final NativeVolumeControl volume = new NativeVolumeControl();

    static native boolean nExtendedAudio();
    static native boolean nCodecSupported(String mime);
    static native int nLoad(byte[] data, String type);
    static native boolean nStart(int handle);
    static native boolean nStop(int handle);
    static native void nClose(int handle);
    static native long nSeek(int handle, long time);
    static native long nTime(int handle);
    static native long nDuration(int handle);
    static native int nVolume(int handle, int level);
    static native int nRate(int handle, int rate, RateControlImpl control);
    static native boolean nLoop(int handle, int count);
    static native int nState(int handle);
    static native boolean nTone(int note, int duration, int volume);

    NativePlayer(byte[] data, String type) { source = data; contentType = type; }
    private void checkOpen() {
        if (state == CLOSED) throw new IllegalStateException("player closed");
    }
    private void checkRealized() {
        checkOpen();
        if (state == UNREALIZED) throw new IllegalStateException("player unrealized");
    }
    int rateHandle() { checkRealized(); return handle; }
    public void realize() throws MediaException {
        checkOpen();
        if (state != UNREALIZED) return;
        int slot = 0;
        while (slot < players.length && players[slot] != null) slot++;
        if (slot == players.length) throw new MediaException("too many open players");
        int loaded = nLoad(source, contentType);
        if (loaded <= 0) throw new MediaException("unsupported or malformed media");
        handle = loaded;
        players[slot] = this;
        source = null;
        nLoop(handle, loopCount);
        nVolume(handle, volume.muted ? 0 : volume.level);
        state = REALIZED;
        notifyListeners(PlayerListener.DURATION_UPDATED, new Long(nDuration(handle)));
    }
    public void prefetch() throws MediaException {
        realize();
        checkOpen();
        if (state == REALIZED) state = PREFETCHED;
    }
    public void start() throws MediaException {
        prefetch();
        if (getState() == STARTED) return;
        if (!nStart(handle)) throw new MediaException("cannot start media");
        state = STARTED;
        notifyListeners(PlayerListener.STARTED, new Long(nTime(handle)));
    }
    public void stop() throws MediaException {
        checkOpen();
        if (state != STARTED) return;
        if (!nStop(handle)) throw new MediaException("cannot stop media");
        state = PREFETCHED;
        notifyListeners(PlayerListener.STOPPED, new Long(nTime(handle)));
    }
    public void deallocate() {
        checkOpen();
        if (state == STARTED) {
            nStop(handle);
            state = PREFETCHED;
            notifyListeners(PlayerListener.STOPPED, new Long(nTime(handle)));
        }
        checkOpen();
        if (state == PREFETCHED) state = REALIZED;
    }
    public void close() {
        if (state == CLOSED) return;
        state = CLOSED;
        RateControlImpl.release(this);
        source = null;
        if (handle != 0) nClose(handle);
        int i = 0;
        while (i < players.length) { if (players[i] == this) players[i] = null; i++; }
        handle = 0;
        notifyListeners(PlayerListener.CLOSED, null);
        listeners = new PlayerListener[0];
    }
    public int getState() {
        if (state == STARTED && nState(handle) == PREFETCHED) return PREFETCHED;
        return state;
    }
    public void setLoopCount(int count) {
        checkOpen();
        if (getState() == STARTED) throw new IllegalStateException("player started");
        if (count == 0 || count < -1) throw new IllegalArgumentException("loop count");
        loopCount = count;
        if (handle != 0) nLoop(handle, count);
    }
    public long setMediaTime(long time) throws MediaException {
        checkRealized();
        long result = nSeek(handle, time < 0 ? 0 : time);
        if (result < 0) throw new MediaException("cannot seek media");
        return result;
    }
    public long getMediaTime() { checkOpen(); return handle == 0 ? 0 : nTime(handle); }
    public long getDuration() { checkOpen(); return handle == 0 ? TIME_UNKNOWN : nDuration(handle); }
    public String getContentType() { checkRealized(); return contentType; }
    public Control getControl(String type) {
        checkRealized();
        if (type == null) throw new IllegalArgumentException("null control type");
        if (type.equals("VolumeControl") || type.equals("javax.microedition.media.control.VolumeControl")) return volume;
        if (type.equals("RateControl") || type.equals("javax.microedition.media.control.RateControl")) return RateControlImpl.forPlayer(this);
        return null;
    }
    public Control[] getControls() { checkRealized(); return new Control[] { volume, RateControlImpl.forPlayer(this) }; }
    public void addPlayerListener(PlayerListener listener) {
        checkOpen();
        if (listener == null) return;
        int i = 0;
        while (i < listeners.length) { if (listeners[i] == listener) return; i++; }
        PlayerListener[] next = new PlayerListener[listeners.length + 1];
        i = 0;
        while (i < listeners.length) { next[i] = listeners[i]; i++; }
        next[i] = listener;
        listeners = next;
    }
    public void removePlayerListener(PlayerListener listener) {
        checkOpen();
        int i = 0;
        while (i < listeners.length && listeners[i] != listener) i++;
        if (i == listeners.length) return;
        PlayerListener[] next = new PlayerListener[listeners.length - 1];
        int j = 0;
        while (j < next.length) { next[j] = listeners[j < i ? j : j + 1]; j++; }
        listeners = next;
    }
    private void notifyListeners(String event, Object data) {
        PlayerListener[] snapshot = listeners;
        int i = 0;
        while (i < snapshot.length) {
            try { snapshot[i].playerUpdate(this, event, data); }
            catch (RuntimeException ignored) { }
            i++;
        }
    }
    /** Called only by the VM main-thread completion pump, never the audio callback. */
    public static void dispatchEnd(int endedHandle) {
        int i = 0;
        while (i < players.length) {
            NativePlayer player = players[i];
            if (player != null && player.handle == endedHandle && player.state == STARTED
                    && nState(endedHandle) == PREFETCHED) {
                player.state = PREFETCHED;
                player.notifyListeners(PlayerListener.END_OF_MEDIA, new Long(nTime(endedHandle)));
                return;
            }
            i++;
        }
    }
    /** VM teardown: release handles and roots without running application callbacks. */
    public static void resetAll() {
        int i = 0;
        while (i < players.length) {
            NativePlayer player = players[i];
            if (player != null) {
                RateControlImpl.release(player);
                if (player.handle != 0) nClose(player.handle);
                player.handle = 0;
                player.source = null;
                player.state = CLOSED;
                player.listeners = new PlayerListener[0];
                players[i] = null;
            }
            i++;
        }
    }
    private final class NativeVolumeControl implements javax.microedition.media.control.VolumeControl {
        private int level = 100;
        private boolean muted;
        public int setLevel(int value) {
            checkOpen();
            int next = value < 0 ? 0 : value > 100 ? 100 : value;
            if (next != level) {
                level = next;
                if (handle != 0) nVolume(handle, muted ? 0 : level);
                notifyListeners(PlayerListener.VOLUME_CHANGED, this);
            }
            return level;
        }
        public int getLevel() { checkOpen(); return level; }
        public void setMute(boolean value) {
            checkOpen();
            if (muted != value) {
                muted = value;
                if (handle != 0) nVolume(handle, muted ? 0 : level);
                notifyListeners(PlayerListener.VOLUME_CHANGED, this);
            }
        }
        public boolean isMuted() { checkOpen(); return muted; }
    }
}
