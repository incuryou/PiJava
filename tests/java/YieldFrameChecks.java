import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

/** Java yields share a bounded host slice instead of costing a display tick. */
public class YieldFrameChecks extends Canvas implements Runnable {
    static YieldFrameChecks canvas;
    static int phase, paints;
    static volatile boolean stop;
    static int firstTurns, secondTurns;

    static volatile boolean paintReady;
    static boolean handoffFixture;
    static int turns, paintOnHost, callbackOnHost, callbackSawPaint;
    static Thread hostThread, workerThread;

    protected void paint(Graphics graphics) {
        paints++;
        if (handoffFixture) {
            paintOnHost = Thread.currentThread() == hostThread &&
                Thread.currentThread() != workerThread ? 1 : -1;
            paintReady = true;
        }
    }

    public void run() {
        callbackOnHost = Thread.currentThread() == hostThread &&
            Thread.currentThread() != workerThread ? 1 : -1;
        callbackSawPaint = paintReady ? 1 : -1;
    }

    // 0: repaint then wait for paint at one yield PC; the host must service it.
    // 1: monitor held; 2: hidden Canvas; 3: repaint before each yield;
    // 4: yields from a foreign owner; 5: alternating yield PCs;
    // 6: grouped update; 7: Native mode. Negative cases run finite loops so
    // completion in one host slice proves they were not handed off early.
    public static int setupHandoff(final int scenario) {
        handoffFixture = true;
        phase = paints = turns = paintOnHost = callbackOnHost = callbackSawPaint = 0;
        paintReady = false;
        hostThread = Thread.currentThread();
        workerThread = null;
        canvas = new YieldFrameChecks();
        final YieldFrameChecks hidden = new YieldFrameChecks();
        final Display display = Display.getDisplay(null);
        display.setCurrent(canvas);
        if (scenario == 4) {
            new Thread() {
                public void run() {
                    canvas.repaint();
                    try { Thread.sleep(1000); }
                    catch (InterruptedException unexpected) {
                        throw new RuntimeException("foreign requester interrupted");
                    }
                }
            }.start();
        }
        new Thread() {
            public void run() {
                workerThread = Thread.currentThread();
                if (scenario != 4) {
                    if (scenario == 2) hidden.repaint();
                    else canvas.repaint();
                }
                display.callSerially(canvas);
                phase = 1;
                if (scenario == 0) {
                    while (!paintReady) { turns++; Thread.yield(); }
                } else if (scenario == 1) {
                    synchronized (canvas) {
                        for (int i = 0; i < 4; i++) { turns++; Thread.yield(); }
                    }
                } else if (scenario == 3) {
                    for (int i = 0; i < 4; i++) {
                        canvas.repaint(); turns++; Thread.yield();
                    }
                } else if (scenario == 5) {
                    for (int i = 0; i < 4; i++) {
                        turns++; Thread.yield();
                        turns++; Thread.yield();
                    }
                } else {
                    for (int i = 0; i < 4; i++) { turns++; Thread.yield(); }
                }
                phase = 2;
                try { Thread.sleep(1000); }
                catch (InterruptedException unexpected) {
                    throw new RuntimeException("handoff worker interrupted");
                }
                phase = 3;
            }
        }.start();
        return scenario == 4 ? 2 : 1;
    }

    public static int setup() {
        handoffFixture = false;
        phase = 0;
        canvas = new YieldFrameChecks();
        Display.getDisplay(null).setCurrent(canvas);
        new Thread() {
            public void run() {
                canvas.repaint();
                phase = 1;
                Thread.yield();
                phase = 2;
                try { Thread.sleep(1000); }
                catch (InterruptedException unexpected) {
                    throw new RuntimeException("unexpected frame worker interrupt");
                }
                phase = 3;
            }
        }.start();
        return 1;
    }

    // The host must call thread_resume exactly once between setup and verify.
    public static int verify() {
        if (phase != 2) throw new RuntimeException("yield consumed a host frame");
        return 1;
    }

    public static int setupYielding() {
        stop = false;
        firstTurns = secondTurns = 0;
        new Thread() {
            public void run() {
                while (!stop) { firstTurns++; Thread.yield(); }
            }
        }.start();
        new Thread() {
            public void run() {
                while (!stop) { secondTurns++; Thread.yield(); }
            }
        }.start();
        return 2;
    }

    public static int verifyYielding() {
        if (firstTurns <= 0 || secondTurns <= 0)
            throw new RuntimeException("yield worker starved its peer");
        stop = true;
        return 2;
    }
}
