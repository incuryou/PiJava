import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
public class CachedElapsedGateFixture extends Canvas implements Runnable {
    public static long anchor,elapsed;
    public static volatile boolean done;
    public static int updates,paints,mode,auxiliary;
    public static void start(int value) {
        mode=value;updates=paints=auxiliary=0;
        CachedElapsedGateFixture c=new CachedElapsedGateFixture();
        Display.getDisplay(null).setCurrent(c);new Thread(c).start();
    }
    protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,16,16);done=true;}
    public void run() {
        while(true) {
            anchor=System.currentTimeMillis();updates++;
            done=false;repaint();
            while(!done) {try {Thread.sleep(10);}catch(Exception ignored) {}}
            if(mode==0) {
                elapsed=System.currentTimeMillis()-anchor;
                while(elapsed>0 && elapsed<50) {
                    try {Thread.sleep(10);}catch(Exception ignored) {}
                    elapsed=System.currentTimeMillis()-anchor;
                }
            } else if(mode==1) {
                elapsed=System.currentTimeMillis()-anchor;
                while(elapsed>0 && elapsed<50) {
                    auxiliary++;try {Thread.sleep(10);}catch(Exception ignored) {}
                    elapsed=System.currentTimeMillis()-anchor;
                }
            } else {
                elapsed=System.currentTimeMillis()-anchor;
                while(elapsed>0 && elapsed<1000) {
                    try {Thread.sleep(10);}catch(Exception ignored) {}
                    elapsed=System.currentTimeMillis()-anchor;
                }
            }
        }
    }
}
