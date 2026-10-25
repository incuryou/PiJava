import javax.microedition.midlet.MIDlet;

// Test-only MIDlet; never copy its classes into production ROMFS.
public final class P1ThreadEndingChecks extends MIDlet {
    public static int entered, completed, caught;

    protected void startApp() {
        int mode = Integer.parseInt(getAppProperty("p1.test.mode"));
        entered = completed = caught = 0;
        if (mode == 3) { new Thread().start(); return; }
        if (mode == 5) {
            new Thread() { public void run() { entered++; completed++; } }.start();
            return;
        }
        new Thread(new Task(mode)).start();
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}

    private static final class Task implements Runnable {
        private final int mode;
        Task(int mode) { this.mode = mode; }
        public void run() {
            entered++;
            if (mode == 1) {
                try { throw new RuntimeException("p1 caught then return"); }
                catch (RuntimeException expected) { caught++; }
            } else if (mode == 2) {
                throw new RuntimeException("p1 uncaught worker");
            } else if (mode == 4) {
                try { Thread.sleep(60000); }
                catch (InterruptedException expected) { caught++; }
            } else if (mode == 6 || mode == 7 || mode == 8) {
                if (mode == 8) {
                    try { Thread.sleep(60000); }
                    catch (InterruptedException expected) { caught++; }
                }
                for (int i = 0; i < 40; i++) {
                    try { throw new RuntimeException("p1 rolling caught trace"); }
                    catch (RuntimeException expected) { caught++; }
                }
                if (mode == 7) {
                    try { Thread.sleep(60000); }
                    catch (InterruptedException expected) { caught++; }
                }
            }
            completed++;
        }
    }
}

// Separate MIDlet identity for launch -> menu -> different-game coverage.
class P1ThreadEndingOtherChecks extends MIDlet {
    protected void startApp() { new Thread().start(); }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
}
