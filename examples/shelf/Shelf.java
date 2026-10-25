import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.List;
import javax.microedition.midlet.MIDlet;

public class Shelf extends MIDlet implements CommandListener {
    List list;
    public void startApp() {
        String title = ioOk() ? "SHELF" : "NG IO";
        list = new List(title, List.IMPLICIT);
        list.append("ONE", null);
        list.append("TWO", null);
        list.append("THREE", null);
        list.setCommandListener(this);
        Display.getDisplay(this).setCurrent(list);
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
    public void commandAction(Command c, Displayable d) {
        Display.getDisplay(this).setCurrent(new ShelfPage(list.getSelectedIndex()));
    }
    static boolean ioOk() {
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream(bos);
            dos.writeUTF("Hi");
            dos.writeInt(42);
            DataInputStream din = new DataInputStream(new ByteArrayInputStream(bos.toByteArray()));
            String s = din.readUTF();
            int n = din.readInt();
            return s != null && s.equals("Hi") && n == 42;
        } catch (Exception e) {
            return false;
        }
    }
}

class ShelfPage extends Canvas {
    int index;
    public ShelfPage(int index) { this.index = index; }
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF4E2B0);
        g.drawString("PICK " + index, 24, 80, 0);
    }
}
