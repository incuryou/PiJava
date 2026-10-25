import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
public class GateRecovery234Checks extends Canvas implements Runnable {
    public static volatile int updates;
    public static void start() {
        updates=0;
        GateRecovery234Checks c=new GateRecovery234Checks();
        Display.getDisplay(null).setCurrent(c);
        new Thread(c).start();
    }
    protected void paint(Graphics g) {g.setColor(updates);g.fillRect(0,0,16,16);}
    public void run() {
        long previous=System.currentTimeMillis();
        while(true) {
            while(System.currentTimeMillis()-previous<50)Thread.yield();
            previous=System.currentTimeMillis();updates++;
            repaint();serviceRepaints();
        }
    }
}
