import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

/** Real VM callbacks: frame lifetime, synchronous service, and explicit blocking. */
public class PaintSchedulingChecks extends Canvas {
    static PaintSchedulingChecks canvas;
    static int paints, mode, checksum, workerDone;
    static byte[] resource;
    static boolean entered, afterService;

    protected void paint(Graphics g) {
        paints++;
        entered = true;
        if (mode == 1) {
            // Exceeds the instruction budget on every host, independent of CPU speed.
            for (int i = 0; i < 100000; i++) checksum += resource[0];
        } else if (mode == 2) {
            try { Thread.sleep(2); }
            catch (InterruptedException e) { throw new RuntimeException(e.toString()); }
            if (workerDone != 1) throw new RuntimeException("paint sleep prevented worker progress");
        } else if (mode == 3) {
            repaint();
            serviceRepaints(); // Must leave the new request pending, not recurse.
        } else if (mode == 4) {
            throw new IllegalStateException("paint failure");
        }
    }
    public static int setup() {
        canvas = new PaintSchedulingChecks();
        Display.getDisplay(null).setCurrent(canvas);
        mode = 1; resource = new byte[] { 7 };
        new Thread() { public void run() { resource = null; workerDone++; } }.start();
        return 1;
    }
    public static int syncService() {
        mode = 0;
        int before = paints;
        canvas.repaint(); canvas.repaint(); canvas.serviceRepaints();
        if (paints != before + 1) throw new RuntimeException("pending frame not completed exactly once");
        canvas.serviceRepaints();
        if (paints != before + 1) throw new RuntimeException("service without pending frame");
        PaintSchedulingChecks hidden = new PaintSchedulingChecks();
        canvas.repaint(); hidden.serviceRepaints();
        if (paints != before + 1) throw new RuntimeException("hidden canvas consumed current frame");
        canvas.serviceRepaints();
        if (paints != before + 2) throw new RuntimeException("current pending frame lost");
        mode = 3; canvas.repaint(); canvas.serviceRepaints();
        if (paints != before + 3) throw new RuntimeException("recursive service paint");
        mode = 0; canvas.serviceRepaints();
        if (paints != before + 4) throw new RuntimeException("paint request lost inside paint");
        return 4;
    }
    public static int setupBlocking() {
        mode = 2; workerDone = 0;
        new Thread() { public void run() { workerDone++; } }.start();
        return 1;
    }
    public static int setupWorkerService() {
        mode = 0; afterService = false;
        new Thread() { public void run() {
            int before = paints;
            canvas.repaint(); canvas.serviceRepaints();
            if (paints != before + 1) throw new RuntimeException("worker continued before paint");
            afterService = true;
        } }.start();
        return 1;
    }
    public static int setupWorkerException() {
        mode = 4; afterService = false;
        new Thread() { public void run() {
            try { canvas.repaint(); canvas.serviceRepaints(); }
            catch (IllegalStateException expected) { afterService = true; }
        } }.start();
        return 1;
    }
}
