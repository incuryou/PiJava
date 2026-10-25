import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Han extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new HanCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class HanCanvas extends Canvas {
    public void paint(Graphics g) {
        StringBuilder sb = new StringBuilder();
        sb.append((char) 0x88C5);
        sb.append((char) 0x7532);
        String word = sb.toString();
        boolean ok = word.length() == 2 && word.charAt(0) == 0x88C5 && word.charAt(1) == 0x7532;
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(ok ? word : "HAN NG", 8, 16, 0);
    }
}
