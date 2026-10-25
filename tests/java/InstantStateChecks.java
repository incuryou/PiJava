import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.game.GameCanvas;

/** A real cooperative class initializer is suspended while a second thread waits. */
public class InstantStateChecks extends Thread {
    public static int finished;
    public static int setup() {
        new InstantStateChecks().start();
        new InstantStateChecks().start();
        return 2;
    }
    public void run() { int initialized = InstantStateSlow.value; finished += initialized; }
}
class InstantStateSlow {
    static int value;
    static {
        try { Thread.sleep(100000); } catch (InterruptedException expected) {}
        value = 1;
    }
}
class InstantStateCanvas extends GameCanvas {
    InstantStateCanvas() { super(true); }
    public void paint(Graphics g) {}
}
class InstantStateConstant {
    static final String text = "preflight constant";
}
