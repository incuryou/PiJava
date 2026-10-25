import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
public class MixedFpsChecks extends Canvas implements Runnable {
    static volatile int updates, paints;
    static int mode;
    static long fieldPrevious, fieldElapsed;
    static volatile boolean fieldGateEnabled;
    static volatile int fieldAttack, fieldAttackDone;
    static class AnchorWriter implements Runnable {
        public void run() {
            while(true) {
                if(fieldAttackDone==0 && fieldAttack!=0) {
                    if(fieldAttack==1)fieldPrevious+=1;
                    else if(fieldAttack==2)fieldPrevious+=500;
                    else fieldPrevious-=500;
                    fieldAttackDone=1;
                }
                try{Thread.sleep(1);}catch(Exception e){}
            }
        }
    }
    protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,16,16);}
    public static void start(int requested) {
        mode=requested;updates=paints=0;fieldGateEnabled=true;fieldAttack=fieldAttackDone=0;
        if(mode==14 || mode==15)new Thread(new AnchorWriter()).start();
        MixedFpsChecks c=new MixedFpsChecks();Display.getDisplay(null).setCurrent(c);new Thread(c).start();
    }
    public static void changeCanvas() {Display.getDisplay(null).setCurrent(new MixedFpsChecks());}
    public void run() {
        long previous=System.currentTimeMillis();
        while(true) {
            if(mode==16) {
                if(!fieldGateEnabled)try{Thread.sleep(22);}catch(Exception e){}
                else {
                    while(System.currentTimeMillis()-previous<52)Thread.yield();
                    previous=System.currentTimeMillis();
                }
                updates++;repaint();serviceRepaints();
                if(fieldGateEnabled)try{Thread.sleep(10);}catch(Exception e){}
                continue;
            }
            if(mode==14 || mode==15) {
                fieldPrevious=System.currentTimeMillis();
                if(fieldAttack!=0) {
                    fieldAttackDone=0;
                    while(fieldAttackDone==0)Thread.yield();
                }
                updates++;repaint();
                if(mode==15)serviceRepaints();
                if(!fieldGateEnabled) {
                    try{Thread.sleep(22);}catch(Exception e){}
                } else if(mode==14) {
                    try{Thread.sleep(10);}catch(Exception e){}
                    fieldElapsed=System.currentTimeMillis()-fieldPrevious;
                    while(fieldElapsed>0 && fieldElapsed<66) {
                        Thread.yield();fieldElapsed=System.currentTimeMillis()-fieldPrevious;
                    }
                } else {
                    while(System.currentTimeMillis()-fieldPrevious<52)Thread.yield();
                    try{Thread.sleep(10);}catch(Exception e){}
                }
                continue;
            }
            if(mode==0 || ((mode==10 || mode==11 || mode==13) && updates>=100)) {try{Thread.sleep(70);}catch(Exception e){}}
            else if((mode==12 || mode==13) && updates%24==23) {
                try{Thread.sleep(8);}catch(Exception e){}
                previous=System.currentTimeMillis();
            } else if(mode==12 || mode==13) {
                long gate=70;
                while(System.currentTimeMillis()-previous<gate)Thread.yield();
                previous=System.currentTimeMillis();
                try{Thread.sleep(8);}catch(Exception e){}
            }
            else if(mode==8 || mode==10) {
                long gate=50;
                while(System.currentTimeMillis()-previous<gate)Thread.yield();
                previous=System.currentTimeMillis();
            } else if(mode==9) {
                while(System.currentTimeMillis()-previous<50)Thread.yield();
                previous=System.currentTimeMillis();
            } else if(mode>=5) {
                while(System.currentTimeMillis()-previous<70)Thread.yield();
                previous=System.currentTimeMillis();
                if(mode==6 || mode==11)try{Thread.sleep(1);}catch(Exception e){}
                if(mode==7 && updates%30==29)try{Thread.sleep(8);}catch(Exception e){}
            } else {
                long gate=70;
                while(System.currentTimeMillis()-previous<gate) {
                    if(mode==2)try{Thread.sleep(1);}catch(Exception e){}
                    else Thread.yield();
                }
                previous=System.currentTimeMillis();
                if(mode==3)try{Thread.sleep(8);}catch(Exception e){}
                if(mode==4 && updates%30==29)try{Thread.sleep(20);}catch(Exception e){}
            }
            updates++;repaint();serviceRepaints();
            if(mode==8 || mode==9){repaint();serviceRepaints();}
        }
    }
}
