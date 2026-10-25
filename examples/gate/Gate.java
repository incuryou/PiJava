import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.util.Calendar;
import java.util.Random;
import javax.microedition.io.Connector;
import javax.microedition.io.HttpConnection;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordEnumeration;
import javax.microedition.rms.RecordStore;
import com.nokia.mid.sound.Sound;

public class Gate extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new GateCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class GateCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String word = "UTF";
        try {
            byte[] raw = new byte[] {0, 3, 65, 68, 65};
            word = new DataInputStream(new ByteArrayInputStream(raw)).readUTF();
        } catch (Exception e) { word = "UTF"; }
        if (!word.equals("ADA")) word = "UTF";
        int year = Calendar.getInstance().get(Calendar.YEAR);
        String yr = year >= 2020 ? "YEAR" : "YEAR NG";
        int r = new Random(3).nextInt(8);
        String rnd = (r >= 0 && r < 8) ? "RND" : "RND NG";
        String cn = "CN NG";
        try { Class.forName("no.such.Game"); }
        catch (ClassNotFoundException e) { cn = "CN"; }
        catch (Exception e) { cn = "CN NG"; }
        String snd = new Sound(440, 20L).getState() == Sound.SOUND_STOPPED ? "SND" : "SND NG";
        String http = "HTTP NG";
        try {
            HttpConnection conn = (HttpConnection) Connector.open("http://game");
            if (conn.getResponseCode() == 200) http = "HTTP";
        } catch (javax.microedition.io.ConnectionNotFoundException unavailable) { http = "HTTP OFF"; }
        catch (Exception e) {}
        int[] px = new int[] {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000};
        Image img = Image.createRGBImage(px, 2, 2, false);
        Sprite a = new Sprite(img);
        Sprite b = new Sprite(img);
        a.setPosition(0, 0);
        b.setPosition(1, 1);
        String hit = a.collidesWith(b, false) ? "HIT" : "HIT NG";
        String rms = "RMS NG";
        try {
            RecordStore rs = RecordStore.openRecordStore("gate", true);
            if (rs.getNumRecords() == 0) rs.addRecord(new byte[] {1}, 0, 1);
            RecordEnumeration en = rs.enumerateRecords(null, null, false);
            if (en.hasNextElement() && en.nextRecord() != null) rms = "RMS";
            rs.closeRecordStore();
        } catch (Exception e) {}
        String font = Font.getDefaultFont().stringWidth("GATE") == 32 ? "FONT" : "FONT NG";
        String mem = Runtime.getRuntime().freeMemory() > 0 ? "MEM" : "MEM NG";
        System.out.println("gate");
        StringBuilder num = new StringBuilder();
        num.append(7L);
        String longOk = num.toString().equals("7") ? "L7" : "LNG";
        g.setColor(0xF2F2F2);
        g.drawString("GATE", 8, 12, 0);
        g.drawString(word, 8, 28, 0);
        g.drawString(yr, 48, 28, 0);
        g.drawString(rnd, 100, 28, 0);
        g.drawString(cn, 148, 28, 0);
        g.drawString(snd, 8, 44, 0);
        g.drawString(http, 48, 44, 0);
        g.drawString(hit, 100, 44, 0);
        g.drawString(rms, 148, 44, 0);
        g.drawString(font, 8, 60, 0);
        g.drawString(mem, 56, 60, 0);
        g.drawString(longOk, 104, 60, 0);
    }
}
