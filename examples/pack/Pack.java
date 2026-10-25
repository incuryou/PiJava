import java.io.DataInputStream;
import java.util.Hashtable;
import javax.microedition.io.Connector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Pack extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new PackCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class PackCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String jar = "JAR NG";
        try {
            DataInputStream in = Connector.openDataInputStream("note.bin");
            if (in.readInt() == 65) jar = "JAR";
            in.close();
        } catch (Exception e) { jar = "JAR NG"; }
        Hashtable map = new Hashtable();
        map.put("k", "v");
        String mapped = "v".equals(map.get("k")) ? "MAP" : "MAP NG";
        String dig = Character.isDigit('7') && !Character.isDigit('A') ? "DIG" : "DIG NG";
        String hex = "41".equals(Integer.toHexString(65)) ? "HEX" : "HEX NG";
        g.setColor(0xF2F2F2);
        g.drawString("PACK", 16, 16, 0);
        g.drawString(jar, 16, 32, 0);
        g.drawString(mapped, 64, 32, 0);
        g.drawString(dig, 112, 32, 0);
        g.drawString(hex, 160, 32, 0);
    }
}
