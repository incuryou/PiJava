package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Graphics;
public abstract class Layer {
    int x, y, w, h;
    boolean visible;
    protected Layer(int width, int height) { w = width; h = height; visible = true; }
    public void setPosition(int nx, int ny) { x = nx; y = ny; }
    public void move(int dx, int dy) { x = x + dx; y = y + dy; }
    public final int getX() { return x; }
    public final int getY() { return y; }
    public final int getWidth() { return w; }
    public final int getHeight() { return h; }
    public void setVisible(boolean v) { visible = v; }
    public final boolean isVisible() { return visible; }
    public abstract void paint(Graphics g);
}
