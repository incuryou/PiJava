import java.util.Random;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Tetris extends MIDlet {
    public void startApp() {
        Pit pit = new Pit();
        Display.getDisplay(this).setCurrent(pit);
        new Thread(pit).start();
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class Pit extends Canvas implements Runnable {
    int[] board;
    int[] shape;
    int piece, rot, px, py, next, score, lines, alive, pending;
    Random rnd;
    public Pit() {
        board = new int[220];
        shape = new int[224];
        fillShapes();
        rnd = new Random();
        score = 0; lines = 0; alive = 1; pending = -1;
        next = rnd.nextInt(7);
        spawn();
    }
    private void put(int p, int r, int i, int x, int y) {
        int o = ((p * 4 + r) * 4 + i) * 2;
        shape[o] = x; shape[o + 1] = y;
    }
    private void fillShapes() {
        put(0,0,0,0,1); put(0,0,1,1,1); put(0,0,2,2,1); put(0,0,3,3,1);
        put(0,1,0,2,0); put(0,1,1,2,1); put(0,1,2,2,2); put(0,1,3,2,3);
        put(0,2,0,0,2); put(0,2,1,1,2); put(0,2,2,2,2); put(0,2,3,3,2);
        put(0,3,0,1,0); put(0,3,1,1,1); put(0,3,2,1,2); put(0,3,3,1,3);
        put(1,0,0,1,0); put(1,0,1,2,0); put(1,0,2,1,1); put(1,0,3,2,1);
        put(1,1,0,1,0); put(1,1,1,2,0); put(1,1,2,1,1); put(1,1,3,2,1);
        put(1,2,0,1,0); put(1,2,1,2,0); put(1,2,2,1,1); put(1,2,3,2,1);
        put(1,3,0,1,0); put(1,3,1,2,0); put(1,3,2,1,1); put(1,3,3,2,1);
        put(2,0,0,1,0); put(2,0,1,0,1); put(2,0,2,1,1); put(2,0,3,2,1);
        put(2,1,0,1,0); put(2,1,1,1,1); put(2,1,2,2,1); put(2,1,3,1,2);
        put(2,2,0,0,1); put(2,2,1,1,1); put(2,2,2,2,1); put(2,2,3,1,2);
        put(2,3,0,1,0); put(2,3,1,0,1); put(2,3,2,1,1); put(2,3,3,1,2);
        put(3,0,0,1,0); put(3,0,1,2,0); put(3,0,2,0,1); put(3,0,3,1,1);
        put(3,1,0,1,0); put(3,1,1,1,1); put(3,1,2,2,1); put(3,1,3,2,2);
        put(3,2,0,1,1); put(3,2,1,2,1); put(3,2,2,0,2); put(3,2,3,1,2);
        put(3,3,0,0,0); put(3,3,1,0,1); put(3,3,2,1,1); put(3,3,3,1,2);
        put(4,0,0,0,0); put(4,0,1,1,0); put(4,0,2,1,1); put(4,0,3,2,1);
        put(4,1,0,2,0); put(4,1,1,1,1); put(4,1,2,2,1); put(4,1,3,1,2);
        put(4,2,0,0,1); put(4,2,1,1,1); put(4,2,2,1,2); put(4,2,3,2,2);
        put(4,3,0,1,0); put(4,3,1,0,1); put(4,3,2,1,1); put(4,3,3,0,2);
        put(5,0,0,0,0); put(5,0,1,0,1); put(5,0,2,1,1); put(5,0,3,2,1);
        put(5,1,0,1,0); put(5,1,1,2,0); put(5,1,2,1,1); put(5,1,3,1,2);
        put(5,2,0,0,1); put(5,2,1,1,1); put(5,2,2,2,1); put(5,2,3,2,2);
        put(5,3,0,1,0); put(5,3,1,1,1); put(5,3,2,0,2); put(5,3,3,1,2);
        put(6,0,0,2,0); put(6,0,1,0,1); put(6,0,2,1,1); put(6,0,3,2,1);
        put(6,1,0,1,0); put(6,1,1,1,1); put(6,1,2,1,2); put(6,1,3,2,2);
        put(6,2,0,0,1); put(6,2,1,1,1); put(6,2,2,2,1); put(6,2,3,0,2);
        put(6,3,0,0,0); put(6,3,1,1,0); put(6,3,2,1,1); put(6,3,3,1,2);
    }
    private int sx(int i) { return shape[((piece * 4 + rot) * 4 + i) * 2]; }
    private int sy(int i) { return shape[((piece * 4 + rot) * 4 + i) * 2 + 1]; }
    private boolean hit(int nx, int ny, int nr) {
        int old = rot; rot = nr; int i = 0;
        while (i < 4) {
            int x = nx + sx(i); int y = ny + sy(i);
            if (x < 0 || x > 9 || y > 21 || (y >= 0 && board[y * 10 + x] != 0)) { rot = old; return true; }
            i = i + 1;
        }
        rot = old; return false;
    }
    private void spawn() {
        piece = next; next = rnd.nextInt(7); rot = 0; px = 3; py = 0;
        if (hit(px, py, rot)) alive = 0;
    }
    private void lock() {
        int i = 0;
        while (i < 4) {
            int x = px + sx(i); int y = py + sy(i);
            if (y >= 0 && y < 22 && x >= 0 && x < 10) board[y * 10 + x] = piece + 1;
            i = i + 1;
        }
        int cleared = 0; int y = 21;
        while (y >= 0) {
            int full = 1; int x = 0;
            while (x < 10) { if (board[y * 10 + x] == 0) full = 0; x = x + 1; }
            if (full == 1) {
                int yy = y;
                while (yy > 0) {
                    int xx = 0;
                    while (xx < 10) { board[yy * 10 + xx] = board[(yy - 1) * 10 + xx]; xx = xx + 1; }
                    yy = yy - 1;
                }
                int xx = 0; while (xx < 10) { board[xx] = 0; xx = xx + 1; }
                cleared = cleared + 1;
            } else y = y - 1;
        }
        if (cleared > 0) { lines = lines + cleared; score = score + cleared * cleared * 100; }
        spawn();
    }
    private void step() {
        if (alive != 1) return;
        if (pending == 0 && !hit(px - 1, py, rot)) px = px - 1;
        if (pending == 1 && !hit(px + 1, py, rot)) px = px + 1;
        if (pending == 3) {
            int nr = rot + 1; if (nr > 3) nr = 0;
            if (!hit(px, py, nr)) rot = nr;
        }
        int drop = pending == 2 ? 1 : 0;
        pending = -1;
        if (!hit(px, py + 1, rot)) py = py + 1;
        else lock();
        if (drop == 1 && alive == 1 && !hit(px, py + 1, rot)) { py = py + 1; score = score + 1; }
    }
    public void run() {
        while (alive == 1) {
            step();
            repaint();
            int delay = 480 - lines * 20;
            if (delay < 90) delay = 90;
            try { Thread.sleep(delay); } catch (InterruptedException interrupted) { return; }
        }
        repaint();
    }
    private int color(int id) {
        if (id == 1) return 0x5AC8FA;
        if (id == 2) return 0xF4E2B0;
        if (id == 3) return 0xC084FC;
        if (id == 4) return 0x3DDC97;
        if (id == 5) return 0xE07A3D;
        if (id == 6) return 0x5B7CFA;
        if (id == 7) return 0xF472B6;
        return 0x1E3A34;
    }
    protected void paint(Graphics g) {
        g.setColor(0x142033); g.fillRect(0, 0, 240, 320);
        g.setColor(0xC6A15B); g.fillRect(12, 28, 104, 212);
        int y = 2;
        while (y < 22) {
            int x = 0;
            while (x < 10) {
                int id = board[y * 10 + x];
                g.setColor(id == 0 ? 0x0E1724 : color(id));
                g.fillRect(14 + x * 10, 30 + (y - 2) * 10, 9, 9);
                x = x + 1;
            }
            y = y + 1;
        }
        if (alive == 1) {
            g.setColor(color(piece + 1));
            int i = 0;
            while (i < 4) {
                int x = px + sx(i); int yy = py + sy(i);
                if (yy >= 2) g.fillRect(14 + x * 10, 30 + (yy - 2) * 10, 9, 9);
                i = i + 1;
            }
        }
        g.setColor(0xF4E2B0);
        g.drawString("TETRIS", 128, 28, 0);
        if (alive == 1) g.drawString("S" + score, 128, 48, 0);
        else g.drawString("DEAD " + score, 128, 48, 0);
        g.drawString("L" + lines, 128, 64, 0);
    }
    protected void keyPressed(int key) {
        if (key == 2) pending = 0;
        if (key == 5) pending = 1;
        if (key == 6) pending = 2;
        if (key == 1 || key == 8) pending = 3;
    }
}
