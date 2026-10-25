package javax.microedition.media;

/**
 * Silent, non-decoding Player used to keep game lifecycle code operational.
 * STARTED means only that this compatibility handle was started: it does not
 * indicate audio rendering. Media time is a stored caller-supplied marker and
 * never advances; duration remains unknown and END_OF_MEDIA is never emitted.
 * VolumeControl stores level and mute state without decoding or rendering audio.
 * Listener callbacks run synchronously on the caller
 * (a deliberate limitation of this cooperative VM, not full MMAPI conformance).
 * No source stream is retained, read, or closed by this implementation.
 */
public final class SilentPlayer implements Player {
    private int state = UNREALIZED;
    private long mediaTime;
    private int loopCount = 1;
    private String contentType;
    private PlayerListener[] listeners = new PlayerListener[0];
    private final javax.microedition.media.control.VolumeControl volumeControl = new SilentVolumeControl();

    private final class SilentVolumeControl implements javax.microedition.media.control.VolumeControl {
        private int level = 100;
        private boolean muted;
        public int setLevel(int value) {
            checkOpen();
            int next = value < 0 ? 0 : value > 100 ? 100 : value;
            if (level != next) {
                level = next;
                notifyListeners(PlayerListener.VOLUME_CHANGED, this);
            }
            return level;
        }
        public int getLevel() { checkOpen(); return level; }
        public void setMute(boolean value) {
            checkOpen();
            if (muted != value) {
                muted = value;
                notifyListeners(PlayerListener.VOLUME_CHANGED, this);
            }
        }
        public boolean isMuted() { checkOpen(); return muted; }
    }

    SilentPlayer(String type) { contentType = type; }
    private void checkOpen() {
        if (state == CLOSED) throw new IllegalStateException("player closed");
    }
    private void checkRealized() {
        checkOpen();
        if (state == UNREALIZED) throw new IllegalStateException("player unrealized");
    }
    public void realize() {
        checkOpen();
        if (state == UNREALIZED) state = REALIZED;
    }
    public void prefetch() {
        realize();
        if (state == REALIZED) state = PREFETCHED;
    }
    public void start() {
        prefetch();
        if (state != STARTED) {
            state = STARTED;
            notifyListeners(PlayerListener.STARTED, new Long(mediaTime));
        }
    }
    public void stop() {
        checkOpen();
        if (state == STARTED) {
            state = PREFETCHED;
            notifyListeners(PlayerListener.STOPPED, new Long(mediaTime));
        }
    }
    public void deallocate() {
        checkOpen();
        stop();
        if (state == PREFETCHED) state = REALIZED;
    }
    public void close() {
        if (state == CLOSED) return;
        state = CLOSED;
        notifyListeners(PlayerListener.CLOSED, null);
        listeners = new PlayerListener[0];
    }
    public int getState() { return state; }
    public void setLoopCount(int count) {
        checkOpen();
        if (state == STARTED) throw new IllegalStateException("player started");
        if (count == 0 || count < -1) throw new IllegalArgumentException("loop count");
        // Store for compatibility; no decoder means no actual looping or completion.
        loopCount = count;
    }
    public long setMediaTime(long now) {
        checkRealized();
        mediaTime = now < 0 ? 0 : now;
        return mediaTime;
    }
    public long getMediaTime() { checkOpen(); return mediaTime; }
    public long getDuration() { checkOpen(); return TIME_UNKNOWN; }
    public String getContentType() { checkRealized(); return contentType; }
    public Control getControl(String type) {
        checkRealized();
        if (type == null) throw new IllegalArgumentException("null control type");
        if (type.equals("VolumeControl") || type.equals("javax.microedition.media.control.VolumeControl"))
            return volumeControl;
        return null;
    }
    public Control[] getControls() { checkRealized(); return new Control[] { volumeControl }; }
    public void addPlayerListener(PlayerListener listener) {
        checkOpen();
        if (listener == null) return;
        int i = 0;
        while (i < listeners.length) { if (listeners[i] == listener) return; i = i + 1; }
        PlayerListener[] next = new PlayerListener[listeners.length + 1];
        i = 0;
        while (i < listeners.length) { next[i] = listeners[i]; i = i + 1; }
        next[i] = listener;
        listeners = next;
    }
    public void removePlayerListener(PlayerListener listener) {
        checkOpen();
        int i = 0;
        while (i < listeners.length && listeners[i] != listener) i = i + 1;
        if (i == listeners.length) return;
        PlayerListener[] next = new PlayerListener[listeners.length - 1];
        int j = 0;
        while (j < next.length) { next[j] = listeners[j < i ? j : j + 1]; j = j + 1; }
        listeners = next;
    }
    private void notifyListeners(String event, Object data) {
        // Snapshot preserves delivery if a callback changes registration.
        PlayerListener[] current = listeners;
        int i = 0;
        while (i < current.length) {
            try { current[i].playerUpdate(this, event, data); }
            catch (RuntimeException ignored) { /* A listener cannot undo the transition. */ }
            i = i + 1;
        }
    }
}
