import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
import javax.microedition.media.Manager;
import java.util.*;
import java.io.*;
public final class R4Fixture extends MIDlet {
    public static int progress,checksum,paints,pressed,released,timers,explicit,defaultRandom,stream,table,errors;
    public static long start,slept,waited,pollEnd,dateTime,calendarTime;
    public static long joined;
    public static int interrupts,initializedA,initializedB;
    static final Object lock=new Object();
    static View view;
    public static ByteArrayInputStream costStream;
    public static int meterData;
    public static int rmsRecords;
    static void checkMeters() throws Exception {
        costStream=new ByteArrayInputStream(new byte[]{3,4,5});
        int sum=0;
        for(int round=0;round<20;round++){
            ByteArrayInputStream b=new ByteArrayInputStream(new byte[]{1,2,3,4,(byte)255});
            b.mark(0);sum+=b.read();sum+=b.available();b.reset();sum+=(int)b.skip(1);sum+=(int)b.skip(-1);
            byte[] dst=new byte[4];sum+=b.read(dst,1,2);sum+=dst[1]+dst[2];sum+=b.read(dst,0,0);
            DataInputStream d=new DataInputStream(new ByteArrayInputStream(new byte[]{0,1,2,3,4,5,6,7}));
            sum+=d.readUnsignedByte()+d.readUnsignedShort()+d.readInt();
            try {d.readUnsignedShort();errors++;}catch(EOFException expected){}
            Hashtable h=new Hashtable();for(int i=0;i<8;i++)h.put(new Integer(i),new Integer(i+1));
            for(int i=0;i<8;i++)sum+=((Integer)h.get(new Integer(i))).intValue();
            sum+=new Long(7).equals(new Long(7))?1:0;
            sum+=new Integer(7).equals(new Integer(8))?100:1;
        }
        meterData=sum;
    }
    public static int leaf(int x){int a=0;for(int i=0;i<20;i++)a=(a*31)^(i+x);return a;}
    static class SlowInit {static int value;static{try{Thread.sleep(4);}catch(Exception e){errors++;}value=31;}}
    public static class View extends Canvas {
        protected void paint(Graphics g){paints++;g.setColor(checksum&0xffffff);g.fillRect(0,0,getWidth(),getHeight());g.setColor(0xffffff);g.fillRect(pressed, released,3,4);}
        protected void keyPressed(int k){pressed=pressed*17+k;}
        protected void keyReleased(int k){released=released*17+k;}
    }
    protected void startApp(){
        start=System.currentTimeMillis();dateTime=new Date().getTime();calendarTime=Calendar.getInstance().getTime().getTime();
        explicit=new Random(0).nextInt();defaultRandom=new Random().nextInt();
        view=new View();Display.getDisplay(this).setCurrent(view);
        try {Manager.playTone(69,100,50);}catch(Exception e){errors++;}
        new Thread(new Runnable(){public void run(){
            try {Thread.sleep(5);slept=System.currentTimeMillis();}catch(InterruptedException e){errors++;}
            while(progress<100){checksum=leaf(progress);progress++;view.repaint();try{Thread.sleep(3);}catch(Exception e){errors++;}}
        }}).start();
        new Thread(new Runnable(){public void run(){
            synchronized(lock){try{lock.wait(7);waited=System.currentTimeMillis();}catch(InterruptedException e){errors++;}}
        }}).start();
        new Thread(new Runnable(){public void run(){while(System.currentTimeMillis()<start+17)checksum^=1;pollEnd=System.currentTimeMillis();}}).start();
        final Timer t=new Timer();t.schedule(new TimerTask(){public void run(){timers++;if(timers==8)t.cancel();}},4,7);
        try{ByteArrayInputStream b=new ByteArrayInputStream(new byte[]{1,2,3});stream=b.read()+b.available();}catch(Exception e){errors++;}
        Hashtable h=new Hashtable();h.put(new Integer(11),new Integer(19));table=((Integer)h.get(new Integer(11))).intValue();
        try{checkMeters();}catch(Exception e){errors++;}
        try{
            javax.microedition.rms.RecordStore r=javax.microedition.rms.RecordStore.openRecordStore("r4-owned",true);
            if(r.getNumRecords()!=0)errors++;r.addRecord(new byte[]{7,8},0,2);rmsRecords=r.getNumRecords();r.closeRecordStore();
        }catch(Exception e){errors++;}
        final Thread target=new Thread(new Runnable(){public void run(){try{Thread.sleep(9);}catch(Exception e){errors++;}}});target.start();
        new Thread(new Runnable(){public void run(){try{target.join();joined=System.currentTimeMillis();}catch(Exception e){errors++;}}}).start();
        final Thread victim=new Thread(new Runnable(){public void run(){try{Thread.sleep(100);}catch(InterruptedException e){interrupts++;}}});victim.start();
        new Thread(new Runnable(){public void run(){try{Thread.sleep(6);victim.interrupt();}catch(Exception e){errors++;}}}).start();
        new Thread(new Runnable(){public void run(){initializedA=SlowInit.value;}}).start();
        new Thread(new Runnable(){public void run(){initializedB=SlowInit.value;}}).start();
        try{Thread.sleep(-1);errors++;}catch(IllegalArgumentException correct){}catch(Exception e){errors++;}
    }
    protected void pauseApp(){}
    protected void destroyApp(boolean unconditional){}
}
