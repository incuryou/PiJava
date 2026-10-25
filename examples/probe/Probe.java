import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;
import java.lang.System;

public class Probe extends MIDlet {
    public void startApp() {
        Display.getDisplay(this).setCurrent(new ProbeScreen(getAppProperty("MIDlet-Name")));
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class ProbeScreen extends Canvas {
    String msg;
    public ProbeScreen(String name) {
        String bad = "";
        String profiles = System.getProperty("microedition.profiles");
        if (profiles == null || !profiles.equals("MIDP-2.0")) bad = bad + "prop ";
        if (!"AbC".equalsIgnoreCase("abc")) bad = bad + "case ";
        if (!"Hi-X".replace('-', ' ').equals("Hi X")) bad = bad + "repl ";
        if (Font.getDefaultFont().stringWidth("PROBE") != 40) bad = bad + "font ";
        if (name == null || !name.equals("Probe")) bad = bad + "mf ";
        byte[] data = new byte[2];
        data[0] = 65;
        data[1] = 66;
        try {
        RecordStore rs = RecordStore.openRecordStore("p", true);
        int id = rs.addRecord(data, 0, 2);
        byte[] back = rs.getRecord(id);
        if (back == null || back.length != 2 || back[0] != 65) bad = bad + "rms ";
        } catch (javax.microedition.rms.RecordStoreException problem) { bad = bad + "rms "; }
        if (bad.length() == 0) msg = "OK " + profiles;
        else msg = "NG " + bad;
    }
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.translate(16, 48);
        g.setColor(0xC6A15B);
        g.fillRect(0, 0, 80, 18);
        g.setColor(0xF4E2B0);
        g.drawString(msg, 0, 36, 0);
    }
}