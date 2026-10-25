import javax.microedition.midlet.MIDlet;
import javax.microedition.midlet.MIDletStateChangeException;
import javax.microedition.lcdui.Alert;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.game.GameCanvas;

/** Exercises lifecycle notifications through actual interpreted Java callbacks. */
public class LifecycleChecks extends MIDlet {
    static LifecycleChecks app;
    static Display display;
    static Screen a, b, c;
    static PollCanvas poll;
    static int starts, pauses, destroys, unconditional, paints, keys, releases, held;
    static int showA, hideA, showB, hideB, showC, hideC;
    static int startMode, pauseMode, destroyMode, redirectShow, redirectHide;
    static int violations, serials, afterDestroy, workerTicks, alertDismissals;

    public LifecycleChecks() {
        app = this;
        display = Display.getDisplay(this);
        a = new Screen(1); b = new Screen(2); c = new Screen(3);
        poll = new PollCanvas();
    }
    protected void startApp() throws MIDletStateChangeException {
        starts++;
        display.setCurrent(a);
        if (startMode == 1) throw new MIDletStateChangeException("retry start");
        if (startMode == 2) throw new IllegalStateException("start fixture failure");
        if (startMode == 3) notifyPaused();
    }
    protected void pauseApp() {
        pauses++;
        if (pauseMode == 1) throw new IllegalStateException("pause fixture failure");
    }
    protected void destroyApp(boolean force) throws MIDletStateChangeException {
        destroys++;
        if (force) unconditional++;
        if (destroyMode == 1) throw new MIDletStateChangeException("destroy fixture failure");
    }
    static class Screen extends Canvas {
        final int id;
        Screen(int number) { id = number; }
        protected void paint(Graphics g) { paints++; }
        protected void keyPressed(int code) { keys++; held = code; }
        protected void keyReleased(int code) { releases++; if (held == code) held = 0; }
        protected void showNotify() {
            if (!isShown()) violations++;
            if (id == 1) showA++; else if (id == 2) showB++; else showC++;
            if (id == 1 && redirectShow != 0) {
                redirectShow = 0;
                int before = showB;
                display.setCurrent(b);
                if (showB != before) violations++; // Native setCurrent must not recurse.
            }
        }
        protected void hideNotify() {
            if (isShown() || (id == 1 && held != 0)) violations++;
            if (id == 1) hideA++; else if (id == 2) hideB++; else hideC++;
            if (id == 1 && redirectHide != 0) {
                redirectHide = 0;
                int before = showC;
                display.setCurrent(c);
                if (showC != before) violations++;
            }
        }
    }
    static class PollCanvas extends GameCanvas {
        PollCanvas() { super(false); }
        public void paint(Graphics g) {}
    }
    public static class RetryStart extends LifecycleChecks {
        public RetryStart() { startMode = 1; }
    }
    public static class BrokenStart extends LifecycleChecks {
        public BrokenStart() { startMode = 2; }
    }
    public static class SelfPausedStart extends LifecycleChecks {
        public SelfPausedStart() { startMode = 3; }
    }
    public static int shown() { return a.isShown() ? 1 : 0; }
    public static int currentIsC() { return display.getCurrent() == c ? 1 : 0; }
    public static int polledKeys() { return poll.getKeyStates(); }
    public static int selfPause() { app.notifyPaused(); return pauses; }
    public static int requestResume() { app.resumeRequest(); return starts; }
    public static int switchB() {
        int before = showB;
        display.setCurrent(b);
        if (showB != before) violations++;
        return 1;
    }
    public static int sameCurrent() { display.setCurrent(display.getCurrent()); return 1; }
    public static int hideCurrent() { display.setCurrent(null); return 1; }
    public static int showAAgain() { display.setCurrent(a); return 1; }
    public static int showRedirect() { redirectShow = 1; display.setCurrent(a); return 1; }
    public static int hideRedirect() { redirectHide = 1; display.setCurrent(b); return 1; }
    public static int queueSerial() {
        final byte[] data = new byte[] { 7, 11 };
        display.callSerially(new Runnable() {
            public void run() {
                serials += data[0] + data[1];
                display.callSerially(new Runnable() { public void run() { serials++; } });
            }
        });
        return serials;
    }
    public static int queueAfterPaint() {
        final int before = paints;
        ((Canvas) display.getCurrent()).repaint();
        display.callSerially(new Runnable() { public void run() {
            if (paints <= before) violations++;
            serials++;
        } });
        return serials;
    }
    public static int nullSerial() {
        try { display.callSerially(null); }
        catch (NullPointerException expected) { return 1; }
        return 0;
    }
    public static int alert() {
        Alert notice = new Alert("temporary");
        notice.setTimeout(60000);
        display.setCurrent(notice, b);
        return 1;
    }
    public static int foreverAlert() {
        Alert notice = new Alert("persistent");
        notice.setTimeout(Alert.FOREVER);
        display.setCurrent(notice, a);
        return 1;
    }
    public static int listenerAlert() {
        Alert notice = new Alert("listener");
        notice.setTimeout(60000);
        notice.setCommandListener(new CommandListener() {
            public void commandAction(Command command, Displayable screen) {
                if (command != Alert.DISMISS_COMMAND) violations++;
                alertDismissals++;
                display.setCurrent(c);
            }
        });
        display.setCurrent(notice, b);
        return 1;
    }
    public static int workers() {
        new Thread() { public void run() {
            while (true) {
                workerTicks++;
                try { Thread.sleep(1); } catch (InterruptedException ignored) { return; }
            }
        } }.start();
        return 1;
    }
    public static int printNotice() {
        // 0.2.112 F2: what an expired trial prints right before notifyDestroyed().
        System.out.println("  \u6388\u6743\u8fc7\u671f\t");
        System.out.println("   ");
        return 1;
    }
    public static int selfDestroy() {
        byte[] local = new byte[] { 3, 5, 7 };
        app.notifyDestroyed();
        // Destruction must wait until these frames return to the host.
        afterDestroy = local[0] + local[1] + local[2];
        display.callSerially(new Runnable() { public void run() { violations++; } });
        return afterDestroy;
    }
}
