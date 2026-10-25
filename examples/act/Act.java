import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Act extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new ActCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class ActCanvas extends Canvas {
    public void paint(Graphics g) {
        boolean ok = getGameAction(-1) == UP && getGameAction(-2) == DOWN
            && getGameAction(-3) == LEFT && getGameAction(-4) == RIGHT
            && getGameAction(-5) == FIRE && getGameAction(8) == FIRE
            && getKeyCode(UP) == -1 && getKeyCode(DOWN) == -2
            && getKeyCode(LEFT) == -3 && getKeyCode(RIGHT) == -4
            && getKeyCode(FIRE) == 8;
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(ok ? "ACT" : "ACT NG", 8, 16, 0);
    }
}
