import java.io.ByteArrayInputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;

/** Driven by media_event_checks.cpp to verify audio completion and host callbacks. */
public final class MediaEventChecks {
    private static Player player;
    private static int ended;
    private static int closed;
    private static long endedAt;
    private static boolean closeOnEnd;
    public static int setup(int loops, int close) throws Exception {
        if (player != null) player.close();
        player = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()),"audio/wav");
        ended=0; closed=0; endedAt=0; closeOnEnd=close!=0;
        player.addPlayerListener(new PlayerListener() {
            public void playerUpdate(Player source,String event,Object data) {
                if (event.equals(END_OF_MEDIA)) {
                    ended++; endedAt=((Long)data).longValue();
                    if (closeOnEnd) source.close();
                } else if (event.equals(CLOSED)) closed++;
            }
        });
        player.setLoopCount(loops); player.start();
        return player.getState();
    }
    public static int ended() { return ended; }
    public static int closed() { return closed; }
    public static long endedAt() { return endedAt; }
    public static int state() { return player.getState(); }
    public static int close() { player.close(); return closed; }
    public static int tone() throws Exception { Manager.playTone(60,20,100); return 1; }
}
