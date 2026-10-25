import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
public final class R4Blocking extends MIDlet {
    protected void startApp(){Display.getDisplay(this).setCurrent(new Canvas(){protected void paint(Graphics g){
        long start=System.currentTimeMillis();while(System.currentTimeMillis()<start+1000){};
    }});}
    protected void pauseApp(){} protected void destroyApp(boolean b){}
}
