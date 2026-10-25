import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.lcdui.Display;
import javax.microedition.midlet.MIDlet;

public class Keys extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new KeysCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class KeysCanvas extends GameCanvas implements Runnable {
    int x;
    Image mark;
    public KeysCanvas() {
        super(true);
        x = 40;
        int[] rgb = new int[16];
        int i = 0;
        while (i < 16) { rgb[i] = 0xFFC6A15B; i = i + 1; }
        mark = Image.createRGBImage(rgb, 4, 4, true);
        new Thread(this).start();
    }
    public void paint(Graphics g) {}
    public void run() {
        Graphics g = getGraphics();
        while (true) {
            int ks = getKeyStates();
            if ((ks & RIGHT_PRESSED) != 0) x = x + 8;
            if ((ks & LEFT_PRESSED) != 0) x = x - 8;
            g.setColor(0x142033);
            g.fillRect(0, 0, getWidth(), getHeight());
            g.drawImage(mark, x, 120, 0);
            g.setColor(0xF2F2F2);
            g.drawString("KEYS", 16, 24, 0);
            g.drawString("X " + x, 16, 44, 0);
            flushGraphics();
            try { Thread.sleep(16); } catch (InterruptedException interrupted) { return; }
        }
    }
}
