import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Text extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new TextCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class TextCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        StringBuffer sb = new StringBuffer();
        sb.append("A");
        sb.append('B');
        sb.insert(1, "C");
        sb.delete(1, 2);
        int n = sb.length();
        String word = sb.reverse().toString();
        int at = "xxBA".indexOf("BA", 1);
        g.setColor(0xF2F2F2);
        g.drawString("TEXT", 16, 16, 0);
        g.drawString(word.equals("BA") ? "BA" : "NG", 16, 28, 0);
        g.drawString(n == 2 ? "LEN 2" : "LEN NG", 16, 40, 0);
        g.drawString(at == 2 ? "IDX" : "IDX NG", 80, 40, 0);
    }
}
