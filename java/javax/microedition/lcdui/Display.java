package javax.microedition.lcdui;
import javax.microedition.midlet.MIDlet;
public class Display {
    public static native Display getDisplay(MIDlet midlet);
    private native void setCurrent0(Displayable next, int timeout, Displayable returnTo);
    public void setCurrent(Displayable next) {
        if (next instanceof Alert) {
            Alert a = (Alert) next;
            Displayable old = getCurrent();
            if (old == a) return;
            if (old instanceof Alert) old = ((Alert) old).returnScreen;
            a.returnScreen = old;
            setCurrent0(a, a.getTimeout(), old);
        } else setCurrent0(next, -1, null);
    }
    public void setCurrent(Alert alert, Displayable next) {
        if (alert == null || next == null) throw new NullPointerException();
        if (next instanceof Alert) throw new IllegalArgumentException();
        alert.returnScreen = next;
        setCurrent0(alert, alert.getTimeout(), next);
    }
    public native Displayable getCurrent();
    public boolean vibrate(int millis) { if (millis < 0) throw new IllegalArgumentException(); return false; }
    public int numColors() { return 65536; }
    public boolean isColor() { return true; }
    public native void callSerially(Runnable r);
    public boolean flashBacklight(int millis) { if (millis < 0) throw new IllegalArgumentException(); return false; }
    public int numAlphaLevels() { return 256; }
}
