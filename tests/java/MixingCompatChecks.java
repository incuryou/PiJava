import java.io.ByteArrayInputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;

/** Exact capability and reached game expression, executed by the custom VM. */
public final class MixingCompatChecks {
    private static int assertions;
    private static void check(boolean value, String label) {
        assertions++;
        if (!value) throw new RuntimeException(label);
    }
    public static int run() throws Exception {
        assertions = 0;
        // Watermelon's q.<init> calls equals on the returned property value.
        check(System.getProperty("supports.mixing").equals("true"), "MMAPI mixing capability is present");
        check(System.getProperty(new String("supports.mixing")).equals("true"), "property compares key content");
        check(System.getProperty("supports.mixing.unknown") == null, "unknown mixing property stays null");
        check(System.getProperty("SUPPORTS.MIXING") == null, "property key is case sensitive");
        check(System.getProperty("compat234.unknown") == null, "unknown system property stays null");
        check(System.getProperty("microedition.media.version").equals("1.0"), "existing MMAPI version preserved");
        Player first = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        Player second = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        first.start(); second.start(); Manager.playTone(69, 250, 10);
        check(first.getState() == Player.STARTED && second.getState() == Player.STARTED,
                "two players remain started alongside tone");
        first.stop();
        check(second.getState() == Player.STARTED, "stopping one player preserves the other");
        first.close(); second.close();
        return assertions;
    }
}
