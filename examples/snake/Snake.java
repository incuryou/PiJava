import java.util.Random;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Snake extends MIDlet {
    public void startApp() {
        Board board = new Board();
        Display.getDisplay(this).setCurrent(board);
        new Thread(board).start();
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class Board extends Canvas implements Runnable {
    int[] xs; int[] ys; int len; int dx; int dy; int fx; int fy; int score; int alive; Random rnd;
    public Board() {
        xs = new int[64]; ys = new int[64]; len = 4;
        int i = 0;
        while (i < len) { xs[i] = 6 - i; ys[i] = 8; i = i + 1; }
        dx = 1; dy = 0; fx = 12; fy = 8; score = 0; alive = 1; rnd = new Random();
    }
    public void run() {
        while (alive == 1) { step(); repaint(); try { Thread.sleep(80); } catch (InterruptedException interrupted) { return; } }
        repaint();
    }
    private void step() {
        int i = len - 1;
        while (i > 0) { xs[i] = xs[i - 1]; ys[i] = ys[i - 1]; i = i - 1; }
        xs[0] = xs[0] + dx; ys[0] = ys[0] + dy;
        if (xs[0] < 0 || ys[0] < 0 || xs[0] > 14 || ys[0] > 14) { alive = 0; return; }
        i = 1;
        while (i < len) {
            if (xs[i] == xs[0] && ys[i] == ys[0]) { alive = 0; return; }
            i = i + 1;
        }
        if (xs[0] == fx && ys[0] == fy) {
            if (len < 60) { xs[len] = xs[len - 1]; ys[len] = ys[len - 1]; len = len + 1; }
            score = score + 1;
            fx = rnd.nextInt(13) + 1; fy = rnd.nextInt(13) + 1;
        }
    }
    protected void paint(Graphics g) {
        g.setColor(0x142033); g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0x3DDC97); g.fillRect(8 + fx * 14, 8 + fy * 14, 12, 12);
        int i = 0;
        while (i < len) {
            if (i == 0) g.setColor(0xF4E2B0); else g.setColor(0xE07A3D);
            g.fillRect(8 + xs[i] * 14, 8 + ys[i] * 14, 12, 12);
            i = i + 1;
        }
        g.setColor(0xF4E2B0);
        if (alive == 1) g.drawString("S" + score, 8, 236, 0);
        else g.drawString("DEAD " + score, 8, 236, 0);
    }
    protected void keyPressed(int keyCode) {
        if (keyCode == 5 && dx != -1) { dx = 1; dy = 0; }
        if (keyCode == 2 && dx != 1) { dx = -1; dy = 0; }
        if (keyCode == 1 && dy != 1) { dy = -1; dx = 0; }
        if (keyCode == 6 && dy != -1) { dy = 1; dx = 0; }
    }
}
