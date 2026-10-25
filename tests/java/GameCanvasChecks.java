import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.midlet.MIDlet;

// Deliberately inherits paint: buffer-only games must be concrete and launch.
public class GameCanvasChecks extends GameCanvas {
    public static Graphics keptGraphics;
    public int presses, releases;
    public GameCanvasChecks(boolean suppress) { super(suppress); }
    protected void keyPressed(int key) { presses++; }
    protected void keyReleased(int key) { releases++; }
    public static int nullPaintCheck(GameCanvasChecks canvas) {
        try { canvas.paint(null); }
        catch (NullPointerException expected) { return 1; }
        return 0;
    }
}

class GameCanvasBootChecks extends MIDlet {
    protected void startApp() {
        GameCanvasChecks canvas = new GameCanvasChecks(true);
        Graphics g = canvas.getGraphics();
        g.setColor(0x123456);
        g.fillRect(0, 0, canvas.getWidth(), canvas.getHeight());
        Display.getDisplay(this).setCurrent(canvas);
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
}
