package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public abstract class GameCanvas extends Canvas {
    public static final int UP_PRESSED = 1 << Canvas.UP;
    public static final int DOWN_PRESSED = 1 << Canvas.DOWN;
    public static final int LEFT_PRESSED = 1 << Canvas.LEFT;
    public static final int RIGHT_PRESSED = 1 << Canvas.RIGHT;
    public static final int FIRE_PRESSED = 1 << Canvas.FIRE;
    public static final int GAME_A_PRESSED = 1 << Canvas.GAME_A;
    public static final int GAME_B_PRESSED = 1 << Canvas.GAME_B;
    public static final int GAME_C_PRESSED = 1 << Canvas.GAME_C;
    public static final int GAME_D_PRESSED = 1 << Canvas.GAME_D;
    private boolean suppressKeyEvents;
    private int keyHeld, keyLatched;
    protected GameCanvas(boolean suppress) {
        suppressKeyEvents = suppress;
        getBuffer();
    }
    private native Image getBuffer();
    public void paint(Graphics g) {
        g.drawImage(getBuffer(), 0, 0, Graphics.TOP | Graphics.LEFT);
    }
    public native int getWidth();
    public native int getHeight();
    public native Graphics getGraphics();
    public native void flushGraphics();
    public native void flushGraphics(int x, int y, int width, int height);
    public native int getKeyStates();
}
