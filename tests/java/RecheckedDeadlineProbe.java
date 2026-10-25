import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import java.util.Timer;
import java.util.TimerTask;
public class RecheckedDeadlineProbe extends Canvas implements Runnable {
    static volatile int updates, longDone, backgroundDone, interrupted, interruptedAt;
    static volatile long longElapsed, backgroundElapsed;
    static Thread worker;
    static int sink, mode;
    static final Object lock=new Object();
    private long anchor;
    public static void start(int m) {
        mode=m;updates=longDone=backgroundDone=interrupted=interruptedAt=0;
        RecheckedDeadlineProbe c=new RecheckedDeadlineProbe();
        Display.getDisplay(null).setCurrent(m==3?new RecheckedDeadlineProbe():c);
        new Thread(new Background()).start();
        if(m==8)new Thread(new Signaler()).start();
        if(m==7)new Thread(new Painter(c)).start();
        if(m==4)new Timer().schedule(new Tick(c),1);else {worker=new Thread(c);worker.start();}
        if(m==9)new Thread(new Interruptor()).start();
    }
    static class Tick extends TimerTask {
        final RecheckedDeadlineProbe c;
        Tick(RecheckedDeadlineProbe p){c=p;}
        public void run(){c.run();}
    }
    static class Background implements Runnable {
        public void run() {
            long began=System.currentTimeMillis();
            try {synchronized(lock){lock.wait(170);}}catch(Exception e){}
            backgroundElapsed=System.currentTimeMillis()-began;backgroundDone=1;
        }
    }
    static class Interruptor implements Runnable {
        public void run(){try{Thread.sleep(350);}catch(Exception e){}worker.interrupt();}
    }
    static class Signaler implements Runnable {
        public void run(){try{Thread.sleep(90);}catch(Exception e){}synchronized(lock){lock.notify();}}
    }
    static class Painter implements Runnable {
        final RecheckedDeadlineProbe c;
        Painter(RecheckedDeadlineProbe p){c=p;}
        public void run(){for(;;){c.repaint();c.serviceRepaints();try{Thread.sleep(40);}catch(Exception e){}}}
    }
    protected void paint(Graphics g){g.setColor(updates);g.fillRect(0,0,12,12);}
    public void run() {
        int first=1;
        anchor=System.currentTimeMillis();
        for(;;) {
            long now=System.currentTimeMillis();
            int remaining=(int)(40-(now-anchor));
            if(first!=0){remaining=Math.max(1,remaining);first=0;}
            if(remaining>0) {
                try{Thread.sleep(remaining);}catch(Exception e){
                    interruptedAt=updates;
                    // A pending interrupt must reach this sleep before another update.
                    if(mode!=10 || updates==12)interrupted++;
                    if(mode==9){try{Thread.sleep(80);}catch(Exception ignored){}}
                }
                continue;
            }
            anchor=now;updates++;
            if(mode!=2 && mode!=7){repaint();serviceRepaints();}
            if(mode==1){repaint();serviceRepaints();}
            if(mode==10 && updates==12) {
                long began=System.currentTimeMillis();
                while(System.currentTimeMillis()-began<24)sink++;
                Thread.currentThread().interrupt();
            }
            if(mode==5 && updates==12) {
                long began=System.currentTimeMillis();
                try{Thread.sleep(1000);}catch(Exception e){}
                longElapsed=System.currentTimeMillis()-began;longDone=1;
            }
        }
    }
}
