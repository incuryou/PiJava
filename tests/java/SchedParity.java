import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
/** P2.6: thread-switch order and fairness under the shared scheduler core. Three yielding workers append their id
 *  to a shared log; a fourth sleeps 10 ms per step; the canvas repaints every turn. */
public final class SchedParity extends MIDlet {
    static final int N = 400;
    static int[] log = new int[3 * N + 16];
    static int len, sleeperSteps, paints, done;
    static final Object lock = new Object();
    static Canvas c = new Canvas() { protected void paint(Graphics g) { paints++; g.setColor(paints); g.fillRect(0, 0, 8, 8); } };
    protected void startApp() {
        Display.getDisplay(this).setCurrent(c);
        for (int t = 0; t < 3; t++) { final int id = t + 1; new Thread() { public void run() {
            for (int i = 0; i < N; i++) { synchronized (lock) { log[len++] = id; } Thread.yield(); }
            synchronized (lock) { done++; } } }.start(); }
        new Thread() { public void run() { for (int i = 0; i < 20; i++) { try { Thread.sleep(10); } catch (InterruptedException e) {} sleeperSteps++; c.repaint(); } } }.start();
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean u) {}
}
