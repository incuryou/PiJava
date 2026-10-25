package javax.microedition.media;
/** MIDP 2.0 media ABI. The bundled implementation decodes MIDI and PCM WAV audio. */
public interface Player extends Controllable {
    int CLOSED = 0;
    int UNREALIZED = 100;
    int REALIZED = 200;
    int PREFETCHED = 300;
    int STARTED = 400;
    long TIME_UNKNOWN = -1L;
    void realize() throws MediaException;
    void prefetch() throws MediaException;
    void start() throws MediaException;
    void stop() throws MediaException;
    void close();
    void deallocate();
    void setLoopCount(int count);
    long setMediaTime(long now) throws MediaException;
    long getMediaTime();
    long getDuration();
    int getState();
    String getContentType();
    void addPlayerListener(PlayerListener listener);
    void removePlayerListener(PlayerListener listener);
}
