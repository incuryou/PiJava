import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.rms.RecordStore;

/** Synthetic fixture, never a commercial City JAR reproduction. */
public class P1CityDiagnosticChecks extends MIDlet {
    public static int returned, caught, released, checksum;
    protected void startApp(){Display.getDisplay(this).setCurrent(new Surface());}
    protected void pauseApp(){}
    protected void destroyApp(boolean force){}
    public static void primeCaught(){try{throw new RuntimeException("synthetic owned caught snapshot");}catch(RuntimeException e){caught++;}}
    static final class Surface extends Canvas {
        protected void paint(Graphics g){}
        protected void keyReleased(int key){released++;}
        protected synchronized void keyPressed(int key){
            if(key==99){while(true)checksum++;}
            if(key==3){
                long end=System.currentTimeMillis()+1100;
                while(System.currentTimeMillis()<end)checksum++;
            }
            if(key==2)primeCaught();
            if(key==4){
                try{RecordStore store=RecordStore.openRecordStore("city-diagnostic-fixture",true);store.closeRecordStore();}
                catch(Exception e){throw new RuntimeException("synthetic RMS unexpected exception");}
            }
            returned++;
        }
    }
}
