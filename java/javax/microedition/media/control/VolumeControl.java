package javax.microedition.media.control;
import javax.microedition.media.Control;
/** Volume/mute state; the silent backend stores state without producing sound. */
public interface VolumeControl extends Control {
    int setLevel(int level);
    int getLevel();
    void setMute(boolean mute);
    boolean isMuted();
}
