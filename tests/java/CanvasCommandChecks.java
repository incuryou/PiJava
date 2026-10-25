import javax.microedition.lcdui.*;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.midlet.MIDlet;

public class CanvasCommandChecks extends MIDlet implements CommandListener {
    public static CanvasCommandChecks app;
    public static int actions, lastType, presses, releases, mode;
    public static Displayable next;
    public void startApp() { app = this; configure(0, 2, true); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
    public void commandAction(Command c, Displayable d) {
        actions++; lastType = c.getCommandType();
        if (mode == 1) Display.getDisplay(this).setCurrent(next);
        if (mode == 2) notifyDestroyed();
        if (mode == 3) System.gc();
        if (mode == 4) throw new RuntimeException("command callback");
    }
    public static void configure(int kind, int count, boolean listen) {
        Displayable d;
        if (kind == 1) d = new Full();
        else if (kind == 2) d = new Nokia();
        else if (kind == 3) d = new Poll();
        else if (kind == 4) d = new Form("form");
        else d = new Plain();
        Command ok = new Command("OK", Command.OK, 3);
        if (count > 0) { d.addCommand(ok); d.addCommand(ok); }
        if (count > 1) d.addCommand(new Command("Exit", Command.EXIT, 2));
        if (count > 2) d.addCommand(new Command("Screen", Command.SCREEN, 4));
        if (count > 3) d.addCommand(new Command("Help", Command.HELP, 5));
        if (listen) d.setCommandListener(app);
        Display.getDisplay(app).setCurrent(d);
        actions = lastType = presses = releases = mode = 0;
    }
    public static class Plain extends Canvas {
        protected void paint(Graphics g) {}
        protected void keyPressed(int code) { presses++; }
        protected void keyReleased(int code) { releases++; }
    }
    public static class Full extends Plain { public Full() { setFullScreenMode(true); } }
    public static class Nokia extends com.nokia.mid.ui.FullCanvas {
        protected void paint(Graphics g) {}
        protected void keyPressed(int code) { presses++; }
        protected void keyReleased(int code) { releases++; }
    }
    public static class Poll extends GameCanvas {
        public Poll() { super(false); }
        protected void keyPressed(int code) { presses++; }
        protected void keyReleased(int code) { releases++; }
    }
}
