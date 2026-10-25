import java.io.ByteArrayInputStream;
import javax.microedition.media.Control;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.VolumeControl;

/** Volume lifecycle state connected to the real decoded audio player. */
public final class VolumeControlChecks {
    private static int assertions;
    private static void check(boolean condition, String label) {
        assertions++;
        if (!condition) throw new RuntimeException(label);
    }
    private static final class Listener implements PlayerListener {
        int changes;
        Player source;
        Object control;
        int level;
        boolean muted;
        public void playerUpdate(Player player, String event, Object data) {
            if (!event.equals(VOLUME_CHANGED)) return;
            changes++;
            source = player;
            control = data;
            level = ((VolumeControl) data).getLevel();
            muted = ((VolumeControl) data).isMuted();
        }
    }
    public static int run() throws Exception {
        assertions = 0;
        Player player = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        boolean rejected = false;
        try { player.getControl("VolumeControl"); }
        catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "unrealized player control lookup rejected");
        player.realize();
        VolumeControl volume = (VolumeControl) player.getControl("VolumeControl");
        check(volume != null, "game VolumeControl lookup is nonnull");
        check(player.getControl("javax.microedition.media.control.VolumeControl") == volume,
                "short and qualified volume names share one control");
        Control[] controls = player.getControls();
        check(controls.length == 2 && controls[0] == volume && controls[1] == player.getControl("RateControl"), "volume and rate enumerated");
        Control[] other = player.getControls();
        check(other != controls && other[0] == volume, "control enumeration array is fresh");
        controls[0] = null;
        check(player.getControls()[0] == volume, "caller cannot mutate player control array");
        check(player.getControl("UnknownControl") == null, "unknown control stays null");
        check(player.getControl("ToneControl") == null, "tone decoder remains unavailable");
        rejected = false;
        try { player.getControl(null); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "null control lookup rejected");
        check(volume.getLevel() == 100 && !volume.isMuted(), "default volume and mute");

        Listener listener = new Listener();
        player.addPlayerListener(listener);
        check(volume.setLevel(60) == 60 && volume.getLevel() == 60, "game setLevel retains value");
        check(listener.changes == 1 && listener.source == player && listener.control == volume
                && listener.level == 60 && !listener.muted, "volume change carries committed control state");
        check(volume.setLevel(60) == 60 && listener.changes == 1, "unchanged level emits no event");
        check(volume.setLevel(-2147483648) == 0 && volume.getLevel() == 0,
                "negative level clamps to zero");
        check(listener.changes == 2 && listener.level == 0, "clamped low level emits event");
        check(volume.setLevel(-1) == 0 && listener.changes == 2, "same clamped level emits no event");
        check(volume.setLevel(2147483647) == 100 && volume.getLevel() == 100,
                "high level clamps to one hundred");
        check(listener.changes == 3 && listener.level == 100, "clamped high level emits event");
        check(volume.setLevel(101) == 100 && listener.changes == 3, "same high clamp emits no event");
        volume.setMute(true);
        check(volume.isMuted() && volume.getLevel() == 100, "mute retains stored level");
        check(listener.changes == 4 && listener.control == volume && listener.muted,
                "mute change carries control");
        volume.setMute(true);
        check(listener.changes == 4, "unchanged mute emits no event");
        volume.setMute(false);
        check(!volume.isMuted() && listener.changes == 5 && !listener.muted, "unmute emits event");

        player.start();
        check(volume.setLevel(37) == 37, "volume state available while started");
        player.stop();
        player.deallocate();
        check(player.getControl("VolumeControl") == volume && volume.getLevel() == 37,
                "control identity and level survive lifecycle transitions");
        player.removePlayerListener(listener);
        int previous = listener.changes;
        volume.setLevel(38);
        check(listener.changes == previous, "removed listener receives no volume event");
        player.close();
        rejected = false;
        try { volume.getLevel(); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "retained control getLevel rejects closed player");
        rejected = false;
        try { volume.setLevel(38); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "retained control setLevel rejects closed player");
        rejected = false;
        try { volume.isMuted(); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "retained control isMuted rejects closed player");
        rejected = false;
        try { volume.setMute(false); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "retained control setMute rejects closed player");
        rejected = false;
        try { player.getControls(); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "closed player control enumeration rejected");
        return assertions;
    }
}
