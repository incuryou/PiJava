package javax.microedition.media;
public interface PlayerListener {
    String STARTED = "started";
    String STOPPED = "stopped";
    String END_OF_MEDIA = "endOfMedia";
    String DURATION_UPDATED = "durationUpdated";
    String DEVICE_UNAVAILABLE = "deviceUnavailable";
    String DEVICE_AVAILABLE = "deviceAvailable";
    String VOLUME_CHANGED = "volumeChanged";
    String ERROR = "error";
    String CLOSED = "closed";
    // Additional JSR-135 event used by some precompiled games.
    String STOPPED_AT_TIME = "stoppedAtTime";
    void playerUpdate(Player player, String event, Object data);
}
