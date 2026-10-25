import java.io.*;
import javax.microedition.io.*;
import javax.wireless.messaging.*;
/** 0.2.107: SMS and SMS payment always succeed (user decision). Covers the call styles found in the user's JARs. */
public final class SmsOfflineChecks {
    static int checks,callbacks;
    static MessageConnection held;
    static BinaryMessage kept;
    static void check(boolean b,String label){if(!b)throw new RuntimeException(label);checks++;}
    static String repeat(String s,int n){StringBuffer b=new StringBuffer();for(int i=0;i<n;i++)b.append(s);return b.toString();}
    public static int baseline(){try{Connector.open("sms://10086");return 1;}catch(IOException unavailable){return 0;}}
    static final class Midlet extends javax.microedition.midlet.MIDlet{protected void startApp(){}protected void pauseApp(){}protected void destroyApp(boolean u){}}
    public static int run() throws Exception {
        checks=callbacks=0;
        // Style A (81 JARs): Connector.open("sms://N") + newMessage("text") + setPayloadText + send + close.
        MessageConnection c=(MessageConnection)Connector.open("sms://10669588");
        TextMessage t=(TextMessage)c.newMessage(MessageConnection.TEXT_MESSAGE);
        check(t.getAddress().equals("sms://10669588")&&t.getTimestamp()==null,"default destination");
        t.setPayloadText("ZF1234");c.send(t);c.send(t);
        // Style B (27 JARs): explicit setAddress before send; unusual addresses/ports still succeed.
        t.setAddress("sms://+8610010:2948");c.send(t);
        t.setAddress("sms://abc");c.send(t);
        t.setAddress(null);c.send(t);                       // falls back to connection destination
        t.setPayloadText(null);c.send(t);                   // empty payload still sent
        t.setPayloadText(repeat("中",500));c.send(t);       // oversized payload still sent
        // numberOfSegments (11 JARs): always >= 1.
        t.setPayloadText("hello");check(c.numberOfSegments(t)==1,"one segment");
        t.setPayloadText(null);check(c.numberOfSegments(t)>=1,"never 0 segments");
        t.setPayloadText(repeat("A",161));check(c.numberOfSegments(t)==2,"real segment count");
        // Binary (3 JARs).
        BinaryMessage b=(BinaryMessage)c.newMessage(MessageConnection.BINARY_MESSAGE,"sms://10086:5000");
        b.setPayloadData(new byte[]{1,2,3});c.send(b);b.setPayloadData(null);c.send(b);
        boolean bad=false;try{c.send(null);}catch(NullPointerException expected){bad=true;}check(bad,"null message is a programming error");
        bad=false;try{c.newMessage("multipart");}catch(IllegalArgumentException expected){bad=true;}check(bad,"unsupported type");
        // setMessageListener (8 JARs): accepted on any connection; no fake incoming message.
        c.setMessageListener(new MessageListener(){public void notifyIncomingMessage(MessageConnection x){callbacks++;}});
        c.setMessageListener(null);check(callbacks==0,"no invented notification");
        // Any mode, including READ on a client URL, opens.
        MessageConnection r=(MessageConnection)Connector.open("sms://10086",Connector.READ);r.send(t);r.close();
        MessageConnection server=(MessageConnection)Connector.open("sms://:1234",Connector.READ);
        TextMessage sm=(TextMessage)server.newMessage("text");check(sm.getAddress()==null,"server has no destination");
        sm.setAddress("sms://10086");sm.setPayloadText("ok");server.send(sm);server.close();
        // Send after close still reports success (payment flows that close early must not fail).
        c.close();c.close();c.send(t);
        bad=false;try{c.receive();}catch(InterruptedIOException expected){bad=true;}check(bad,"closed receive ends");
        // receive() blocks until close (empty inbox), then ends.
        final MessageConnection waiting=(MessageConnection)Connector.open("sms://:5000");
        final int[] state={0};
        Thread reader=new Thread(){public void run(){try{waiting.receive();state[0]=2;}catch(InterruptedIOException e){state[0]=1;}catch(IOException e){state[0]=3;}}};
        reader.start();Thread.sleep(50);check(state[0]==0,"receive blocks with empty inbox");
        waiting.close();reader.join();check(state[0]==1,"close wakes receive");
        // Samsung (2 JARs).
        com.samsung.util.SM samsung=new com.samsung.util.SM();samsung.setDestAddress("10086");samsung.setData("test");com.samsung.util.SMS.send(samsung);
        com.samsung.util.SMS.send(new com.samsung.util.SM());
        bad=false;try{com.samsung.util.SMS.send(null);}catch(NullPointerException expected){bad=true;}check(bad,"Samsung null");
        // platformRequest (68 JARs reference it).
        Midlet m=new Midlet();
        check(!m.platformRequest("sms:10086?body=PAY"),"platformRequest sms handled");
        check(!m.platformRequest("tel:10086"),"platformRequest tel handled");
        check(!m.platformRequest("http://example.invalid"),"platformRequest other unchanged");
        // Other transports unchanged.
        bad=false;try{Connector.open("http://example.invalid");}catch(ConnectionNotFoundException expected){bad=true;}check(bad,"http unavailable");
        return checks;
    }
    public static int keep() throws Exception {held=(MessageConnection)Connector.open("sms://10086");kept=(BinaryMessage)held.newMessage("binary");kept.setPayloadData(new byte[]{4,5});return 1;}
    public static int verify() throws Exception {held.send(kept);return kept.getPayloadData()[1];}
    public static int release() throws Exception {held.close();held=null;kept=null;return 1;}
    public static int discard() throws Exception {for(int i=0;i<200;i++){MessageConnection c=(MessageConnection)Connector.open("sms://10086");TextMessage m=(TextMessage)c.newMessage("text");m.setPayloadText("discard");c.send(m);c.close();}return 1;}
}
