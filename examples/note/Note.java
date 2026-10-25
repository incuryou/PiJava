import javax.microedition.io.Connection;
import javax.microedition.io.Connector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.TextField;
import javax.microedition.media.Manager;
import javax.microedition.midlet.MIDlet;

public class Note extends MIDlet implements CommandListener {
    TextField field;
    public void startApp() {
        try { Manager.playTone(60, 20, 80); }
        catch (javax.microedition.media.MediaException ignored) {}
        try {
            Connection link = Connector.open("http://local");
            link.close();
        } catch (java.io.IOException unavailable) { /* Optional network capability. */ }
        Form form = new Form("NOTE");
        form.append("LINE");
        field = new TextField("NAME", "ADA", 16, TextField.ANY);
        form.append(field);
        form.addCommand(new Command("OK", Command.OK, 1));
        form.setCommandListener(this);
        Display.getDisplay(this).setCurrent(form);
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
    public void commandAction(Command c, Displayable d) {
        Display.getDisplay(this).setCurrent(new NotePage(field.getString()));
    }
}

class NotePage extends Canvas {
    String name;
    public NotePage(String name) { this.name = name; }
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF4E2B0);
        g.drawString("SAVED " + name, 16, 80, 0);
    }
}
