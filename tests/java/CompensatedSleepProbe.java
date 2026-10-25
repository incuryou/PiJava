import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
public class CompensatedSleepProbe extends Canvas implements Runnable {
    static volatile int updates, backgroundDone;
    static volatile long backgroundElapsed;
    static long period;
    static int mode;
    static volatile long business;
    public static void start(int m) {
        mode=m;period=m==5?53:37;updates=backgroundDone=0;
        CompensatedSleepProbe c=m==4?new ReusedError():m==6?new ReusedRequest():new CompensatedSleepProbe();
        Display.getDisplay(null).setCurrent(m==3?new CompensatedSleepProbe():c);
        new Thread(new Background()).start();new Thread(c).start();
    }
    public static class App extends javax.microedition.midlet.MIDlet {
        protected void startApp(){CompensatedSleepProbe.start(10);}
        protected void pauseApp(){}
        protected void destroyApp(boolean unconditional){}
    }
    static class Background implements Runnable {
        public void run() {
            long began=System.currentTimeMillis();
            try{Thread.sleep(175);}catch(Exception e){}
            backgroundElapsed=System.currentTimeMillis()-began;backgroundDone=1;
        }
    }
    protected void paint(Graphics g){g.setColor(updates);g.fillRect(0,0,12,12);}
    public void run() {
        long now=0,elapsed=0,oversleep=0;
        long anchor=System.currentTimeMillis();
        for(;;) {
            updates++;
            if(mode!=2){repaint();serviceRepaints();}
            elapsed=(now=System.currentTimeMillis())-anchor;
            long remaining;
            if((remaining=period-elapsed-oversleep)>0) {
                try{Thread.sleep(remaining);}catch(InterruptedException e){}
                oversleep=System.currentTimeMillis()-now-remaining;
            } else oversleep=0;
            anchor=System.currentTimeMillis();
        }
    }
    static class ReusedError extends CompensatedSleepProbe {
        public void run() {
        long now=0,elapsed=0,oversleep=0;
        long anchor=System.currentTimeMillis();
        for(;;) {
            updates++;repaint();serviceRepaints();
            elapsed=(now=System.currentTimeMillis())-anchor;
            long remaining;
            if((remaining=period-elapsed-oversleep)>0) {
                try{Thread.sleep(remaining);}catch(InterruptedException e){}
                oversleep=System.currentTimeMillis()-now-remaining;
            } else oversleep=0;
            anchor=System.currentTimeMillis();
            business=oversleep;
        }
        }
    }
    static class ReusedRequest extends CompensatedSleepProbe {
        public void run() {
        long now=0,elapsed=0,oversleep=0;
        long anchor=System.currentTimeMillis();
        for(;;) {
            updates++;repaint();serviceRepaints();
            elapsed=(now=System.currentTimeMillis())-anchor;
            long remaining;
            if((remaining=period-elapsed-oversleep)>0) {
                try{Thread.sleep(remaining);}catch(InterruptedException e){}
                oversleep=System.currentTimeMillis()-now-remaining;
            } else oversleep=0;
            anchor=System.currentTimeMillis();
            business=remaining;
        }
        }
    }
}
