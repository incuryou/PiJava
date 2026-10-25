import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

/** Scheduling skeleton of ZXX v.run; body work is intentionally minimal. */
public final class GatedCanvas extends Canvas implements Runnable {
    public static boolean sync;
    public static int updates, counter, paints;
    public static GatedCanvas canvas;
    public static int setup(boolean synchronous) {
        sync=synchronous;
        canvas=new GatedCanvas();
        Display.getDisplay(null).setCurrent(canvas);
        new Thread(canvas).start();
        return 1;
    }
    public void run() {
        long previous=0L;
        long aux=0L;
        while(true) {
            Thread.yield();
            if(System.currentTimeMillis()-aux>=100L) {
                aux=System.currentTimeMillis();
                counter++;
                if(counter>999)counter=0;
            }
            if(System.currentTimeMillis()-previous>=50L) {
                previous=System.currentTimeMillis();
                if(!sync) {
                    repaint();
                } else {
                    repaint();
                    serviceRepaints();
                }
                updates++;
            }
        }
    }
    protected void paint(Graphics g) {
        paints++;
        g.setColor(updates);
        g.fillRect(0,0,16,16);
    }
}
