import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

/** Actual Java blocking operations driven by deterministic host frame tests. */
public class FrameScheduleChecks extends MIDlet {
    static FrameScheduleChecks app;
    static Screen screen;
    static Thread sleeper, joiner, waiter;
    static int ticks, paints, paintedTicks, interrupts, joins, waits, starts;
    static int sleepMillis = 60000;

    public FrameScheduleChecks() { app = this; screen = new Screen(); }
    protected void startApp() {
        starts++;
        Display.getDisplay(this).setCurrent(screen);
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}

    static class Screen extends Canvas {
        protected void paint(Graphics graphics) { paints++; paintedTicks = ticks; }
    }

    public static int startSleeper() {
        sleeper = new Thread() {
            public void run() {
                ticks++;
                while (true) {
                    try { Thread.sleep(sleepMillis); }
                    catch (InterruptedException expected) { interrupts++; return; }
                    ticks++;
                    screen.repaint();
                }
            }
        };
        sleeper.start();
        return 1;
    }
    public static int interruptSleeper() { sleeper.interrupt(); return 1; }
    public static int startJoiner() {
        joiner = new Thread() {
            public void run() {
                try { sleeper.join(60000); joins++; }
                catch (InterruptedException unexpected) { throw new RuntimeException("join interrupted"); }
            }
        };
        joiner.start();
        return 1;
    }
    public static int startWaiter() {
        waiter = new Thread() {
            public void run() {
                synchronized (screen) {
                    try { screen.wait(60000); waits++; }
                    catch (InterruptedException unexpected) { throw new RuntimeException("wait interrupted"); }
                }
            }
        };
        waiter.start();
        return 1;
    }
    public static int selfPause() { app.notifyPaused(); return 1; }
}
