import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

/** Production sleep/worker/serviceRepaints path; no fabricated lock or clock. */
public final class CityMonitorContinuationChecks {
    static Surface self;
    static int mode, armed, lockHeld, keySleeping, unlockReady;
    static int paints, beforeService, afterService, workerDone, keyDone;
    static int seenToken, expectedToken, plainWhileLocked, order;
    static void require(boolean ok, String message) {
        if (!ok) throw new RuntimeException(message);
    }
    static void recordPaint(Graphics g) {
        // dispatch_key may issue an implicit host repaint after the worker has
        // already finished. This fixture observes the synchronous service span.
        if (armed == 0 || afterService != 0) return;
        if (mode != 2) require(lockHeld == 0 && unlockReady == 1,
                "locked paint ran before key released monitor");
        else {
            require(lockHeld == 1 && keySleeping == 1,
                    "plain paint unnecessarily blocked behind Canvas monitor");
            plainWhileLocked++;
        }
        paints++; seenToken = expectedToken; order = order * 10 + 2;
        g.setColor(expectedToken); g.fillRect(0, 0, 2, 2);
    }
    static class Surface extends Canvas {
        protected void paint(Graphics g) { recordPaint(g); }
        protected void keyPressed(int code) {
            synchronized (this) {
                require(code == 53 && lockHeld == 0, "key argument or reentry changed");
                lockHeld = 1; keySleeping = 1;
                new Thread(new Painter(this, expectedToken)).start();
                try { Thread.sleep(20); }
                catch (InterruptedException e) { throw new RuntimeException("unexpected interrupt"); }
                keySleeping = 0;
                if (mode != 2) require(paints == 0 && afterService == 0,
                        "serviceRepaints returned before blocked paint completed");
                else require(paints == 1 && afterService == 1,
                        "unlocked paint did not progress during sleep");
                lockHeld = 0; unlockReady = 1;
            }
            keyDone++; order = order * 10 + 1;
        }
    }
    static final class MethodLockedSurface extends Surface {
        protected synchronized void paint(Graphics g) { recordPaint(g); }
    }
    static final class BlockLockedSurface extends Surface {
        protected void paint(Graphics g) { synchronized (this) { recordPaint(g); } }
    }
    static void service(Surface surface, int token) {
        require(surface == self && token == expectedToken, "service helper parameters changed");
        beforeService++;
        surface.repaint(); surface.serviceRepaints();
        require(paints == 1 && seenToken == token, "worker continued before paint completion");
        require(surface == self && token == expectedToken, "native retry lost receiver or locals");
        afterService++; order = order * 10 + 3;
    }
    static final class Painter implements Runnable {
        final Surface surface; final int token;
        Painter(Surface s, int t) { surface = s; token = t; }
        public void run() { service(surface, token); workerDone++; }
    }
    public static int setup(int which) {
        mode = which; armed = 0;
        self = which == 0 ? new MethodLockedSurface() :
                which == 1 ? new BlockLockedSurface() : new Surface();
        Display.getDisplay(null).setCurrent(self); return 1;
    }
    public static int arm(int token) {
        require(lockHeld == 0, "previous key retained lock");
        paints = beforeService = afterService = workerDone = keyDone = 0;
        seenToken = plainWhileLocked = order = unlockReady = keySleeping = 0;
        expectedToken = token; armed = 1; return 1;
    }
}
