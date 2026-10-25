import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;

public class AutoFpsChecks extends Canvas implements Runnable {
    static volatile int updates, paints;
    static int mode;
    static int workload;
    protected void paint(Graphics g) { paints++; g.setColor(updates); g.fillRect(0,0,16,16); }
    public static void start(int requested) {
        mode=requested; updates=paints=0;
        AutoFpsChecks c=new AutoFpsChecks();
        Display.getDisplay(null).setCurrent(c);
        new Thread(c).start();
    }
    public void run() {
        long previous=System.currentTimeMillis();
        while(true) {
            if(mode==7) {
                for(int i=0;i<3000000;i++)workload=workload*3+i;
            } else if(mode==0 || mode==3) {
                try {Thread.sleep(mode==0?70:130);} catch(Exception e) {}
            } else {
                long gate=mode==4 || mode==5 ? 50:70;
                while(System.currentTimeMillis()-previous<gate) {
                    if(mode==1 || mode==4 || mode==5)Thread.yield();
                    if(mode==2)try {Thread.sleep(1);} catch(Exception e) {}
                    // mode 6 polls its clock without an explicit scheduling hint.
                }
                previous=System.currentTimeMillis();
            }
            updates++;
            repaint(); serviceRepaints();
            if(mode==4 || (mode==5 && updates>=8)) {repaint();serviceRepaints();}
        }
    }
}
