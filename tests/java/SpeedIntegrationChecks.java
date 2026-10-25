import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
public class SpeedIntegrationChecks extends Canvas implements Runnable {
    static int paints, callbacks, workerSteps;
    static boolean continuous;
    protected void paint(Graphics g) { paints++; if (continuous) repaint(); }
    public static int setupCallbacks() {
        SpeedIntegrationChecks c = new SpeedIntegrationChecks();
        javax.microedition.lcdui.Display d = javax.microedition.lcdui.Display.getDisplay(null);
        d.setCurrent(c); d.callSerially(c); return 1;
    }
    public static void startWorker() {
        new Thread(new Runnable() {
            public void run() { while (true) { workerSteps++; Thread.yield(); } }
        }).start();
    }
    public void run() {
        callbacks++;
        if (!continuous) { repaint(); serviceRepaints(); }
        javax.microedition.lcdui.Display.getDisplay(null).callSerially(this);
    }
}
