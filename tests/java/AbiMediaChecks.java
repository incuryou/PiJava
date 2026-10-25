import java.io.ByteArrayInputStream;
import javax.microedition.media.Control;
import javax.microedition.media.Controllable;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.NativePlayer;
import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;

/** Executed by the VM harness; failures identify the violated ABI/behavior. */
public final class AbiMediaChecks {
    private static void check(boolean condition, String label) {
        if (!condition) throw new RuntimeException(label);
    }
    public static int run() throws Exception {
        StringBuffer buffer = new StringBuffer(2);
        check(buffer.capacity() == 2 && buffer.length() == 0, "buffer capacity ctor");
        check(buffer.append('A') == buffer, "buffer append return");
        buffer.append(true).append(7).append(-9223372036854775808L);
        check(buffer.toString().equals("Atrue7-9223372036854775808"), "buffer primitive append ABI");
        int grown = buffer.capacity();
        check(grown >= buffer.length(), "buffer automatic capacity");
        buffer.setLength(1);
        check(buffer.capacity() == grown, "buffer shrink retains capacity");
        buffer.ensureCapacity(grown + 1);
        check(buffer.capacity() >= grown + 1, "buffer ensure capacity");
        buffer.append(new char[] { 'B', '\u263A', 'C' }, 1, 2);
        check(buffer.toString().equals("A\u263AC"), "buffer array append ABI");
        check(buffer.insert(1, 'B') == buffer && buffer.insert(2, 12) == buffer, "buffer insert return ABI");
        check(buffer.toString().equals("AB12\u263AC"), "buffer insert native descriptors");
        buffer.delete(2, 4).deleteCharAt(1).reverse();
        check(buffer.toString().equals("C\u263AA"), "buffer mutations ABI");
        char[] copied = new char[3];
        buffer.getChars(0, 3, copied, 0);
        check(copied[1] == '\u263A', "buffer getChars");
        buffer.setLength(4);
        check(buffer.charAt(3) == 0, "buffer length zero padding");
        buffer.setCharAt(3, 'D');
        check(buffer.substring(1, 4).equals("\u263AAD"), "buffer substring");
        StringBuffer values = new StringBuffer("v");
        values.append((Object)null).append(false).append(new char[] {'X'});
        values.insert(0, (Object)null).insert(0, false).insert(0, 99L);
        check(values.toString().equals("99falsenullvnullfalseX"), "buffer object bool long insert ABI");
        StringBuffer arrays = new StringBuffer().append(new StringBuffer("AB"));
        arrays.insert(1, new char[] { 'X', 'Y' }).insert(0, new char[] { 'Q', 'Z' }, 1, 1);
        check(arrays.toString().equals("ZAXYB"), "buffer buffer and char array insert ABI");
        StringBuffer decimals = new StringBuffer("X");
        decimals.insert(0, 1.5f).insert(0, 2.5);
        check(decimals.toString().startsWith("2.5"), "buffer float double insert ABI");
        check(new StringBuffer().append(1.5f).append(2.5).toString().startsWith("1.5"), "buffer float double ABI");
        boolean rejected = false;
        try { new StringBuffer(-1); } catch (NegativeArraySizeException expected) { rejected = true; }
        check(rejected, "buffer negative capacity");
        rejected = false;
        try { buffer.setLength(-1); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "buffer negative length");
        rejected = false;
        try { buffer.insert(-1, "x"); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "buffer invalid insert");
        rejected = false;
        try { buffer.delete(3, 2); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "buffer invalid delete");
        rejected = false;
        try { buffer.setCharAt(buffer.length(), 'x'); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "buffer invalid setCharAt");

        Player player = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        check(player instanceof NativePlayer && player.getState() == Player.UNREALIZED, "Player interface and initial state");
        check(player.getDuration() == Player.TIME_UNKNOWN && player.getMediaTime() == 0, "unrealized times");
        rejected = false;
        try { player.getControls(); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "unrealized controls rejected");
        MediaListener listener = new MediaListener();
        player.addPlayerListener(listener);
        player.addPlayerListener(listener);
        player.addPlayerListener(null);
        try {
            player.realize();
            check(player.getState() == Player.REALIZED, "realize state");
            check(player.setMediaTime(1234567890123L) == 1000000L, "setMediaTime long return ABI");
            Controllable controllable = player;
            Control[] controls = controllable.getControls();
            check(controls.length == 2 && controls[0] == controllable.getControl("VolumeControl")
                    && controls[1] == controllable.getControl("RateControl")
                    && controls[0] instanceof VolumeControl, "control return ABI and stored volume state");
            check(controllable.getControl("UnknownControl") == null, "unknown control remains unsupported");
            check(player.getControl("ToneControl") == null, "no fabricated tone decoder");
            player.setLoopCount(-1);
            player.prefetch();
            check(player.getState() == Player.PREFETCHED, "prefetch state");
            player.start();
            player.start();
            check(player.getState() == Player.STARTED && listener.started == 1, "logical start event");
            check(player.getMediaTime() == 0 && listener.time == 0, "decoded restart and Long callback");
            rejected = false;
            try { player.setLoopCount(1); } catch (IllegalStateException expected) { rejected = true; }
            check(rejected, "started loop mutation rejected");
            player.stop();
            player.stop();
            check(player.getState() == Player.PREFETCHED && listener.stopped == 1, "stop event");
            player.deallocate();
            check(player.getState() == Player.REALIZED, "deallocate state");
            check(player.setMediaTime(-5L) == 0L, "negative media time clamp");
        } catch (javax.microedition.media.MediaException unexpected) {
            throw new RuntimeException("native lifecycle failed");
        }
        player.close();
        player.close();
        check(player.getState() == Player.CLOSED && listener.closed == 1 && listener.ended == 0, "closed once and no fake completion");
        rejected = false;
        try { player.getMediaTime(); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "closed time rejected");
        String[] content = Manager.getSupportedContentTypes(null);
        boolean midi = false, wav = false, ott = false;
        for (int i = 0; i < content.length; i++) {
            midi |= content[i].equals("audio/midi");
            wav |= content[i].equals("audio/wav");
            ott |= content[i].equals("audio/x-nokia-ota");
            check(Manager.getSupportedProtocols(content[i]).length == 1,
                  "every advertised type has a supported resource protocol");
        }
        check(midi && wav && ott && Manager.getSupportedProtocols(null).length == 1 &&
              Manager.getSupportedProtocols("audio/not-implemented").length == 0 &&
              Manager.getSupportedContentTypes("network-not-implemented").length == 0,
              "honest extensible media capabilities");
        rejected = false;
        try { Manager.createPlayer((String)null); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "null media source rejected");
        ByteArrayInputStream source = new ByteArrayInputStream(NativeMediaChecks.wav());
        Player streamPlayer = Manager.createPlayer(source, "audio/wav");
        try { streamPlayer.realize(); } catch (javax.microedition.media.MediaException unexpected) { throw new RuntimeException("WAV stream realize"); }
        check(streamPlayer.getContentType().equals("audio/wav"), "decoded content type");
        streamPlayer.close();
        check(source.available() == 0, "native backend consumes input");
        VolumeControl volume = new AbiVolume();
        Control control = volume;
        check(control != null && volume.setLevel(120) == 100 && volume.getLevel() == 100, "VolumeControl int return ABI");
        volume.setMute(true);
        check(volume.isMuted() && ToneControl.C4 == 60 && ToneControl.VERSION == -2, "control ABI");
        return 1;
    }
}

final class MediaListener implements PlayerListener {
    int started;
    int stopped;
    int closed;
    int ended;
    long time;
    public void playerUpdate(Player player, String event, Object data) {
        if (event.equals(STARTED)) { started = started + 1; time = ((Long)data).longValue(); }
        if (event.equals(STOPPED)) stopped = stopped + 1;
        if (event.equals(CLOSED)) closed = closed + 1;
        if (event.equals(END_OF_MEDIA)) ended = ended + 1;
    }
}
final class AbiVolume implements VolumeControl {
    int level;
    boolean muted;
    public int setLevel(int next) { level = next < 0 ? 0 : next > 100 ? 100 : next; return level; }
    public int getLevel() { return level; }
    public void setMute(boolean value) { muted = value; }
    public boolean isMuted() { return muted; }
}
