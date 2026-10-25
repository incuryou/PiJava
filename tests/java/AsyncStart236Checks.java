import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
public class AsyncStart236Checks extends MIDlet {
    public static int entered,ticks,keys,pauses,starts;
    public static boolean stop;
    protected void startApp() throws MIDletStateChangeException {
        starts++;entered=1;Display.getDisplay(this).setCurrent(new Screen());
        while(!stop){ticks++;try{Thread.sleep(5);}catch(InterruptedException e){}}
    }
    protected void pauseApp(){pauses++;}
    protected void destroyApp(boolean unconditional){stop=true;}
    private static class Screen extends Canvas {
        protected void paint(Graphics g){g.setColor(0x336699);g.fillRect(0,0,getWidth(),getHeight());}
        protected void keyPressed(int key){keys++;}
    }
}
