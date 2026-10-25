package javax.wireless.messaging;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.Date;
import javax.microedition.io.Connector;

/**
 * 0.2.107 (user decision 2026-10-22): every SMS and every SMS-based payment is
 * treated as sent successfully. Nothing is transmitted and no dialog is shown.
 * send() returns normally for any address, port, size or mode and writes one
 * SMSSKIP log line (address + payload length). No incoming SMS is invented:
 * receive() blocks like a phone with an empty inbox until close(), and a
 * registered MessageListener is kept but never told about a fake message.
 */
public final class OfflineSmsConnection implements MessageConnection {
    private final String destination;
    private MessageListener listener;
    private boolean closed;
    private OfflineSmsConnection(String address) { this.destination=address; }
    public static MessageConnection open(String url,int mode) {
        if(mode!=Connector.READ&&mode!=Connector.WRITE&&mode!=Connector.READ_WRITE)throw new IllegalArgumentException("Invalid connection mode");
        // Server form "sms://:port" has no default destination.
        boolean server=url.length()>6&&url.charAt(6)==':';
        return new OfflineSmsConnection(server||url.length()<=6?null:url);
    }
    public Message newMessage(String type) { return newMessage(type,destination); }
    public Message newMessage(String type,String address) {
        if(TEXT_MESSAGE.equals(type))return new Text(address);
        if(BINARY_MESSAGE.equals(type))return new Binary(address);
        throw new IllegalArgumentException("Unsupported SMS message type");
    }
    private static abstract class Container implements Message {
        String address;
        Container(String address){this.address=address;}
        public final String getAddress(){return address;}
        public final void setAddress(String value){address=value;}
        public final Date getTimestamp(){return null;}
        abstract int length();
    }
    private static final class Text extends Container implements TextMessage {
        String text;
        Text(String address){super(address);}
        public String getPayloadText(){return text;}
        public void setPayloadText(String value){text=value;}
        int length(){return text==null?0:text.length();}
    }
    private static final class Binary extends Container implements BinaryMessage {
        byte[] data;
        Binary(String address){super(address);}
        public byte[] getPayloadData(){return data;}
        public void setPayloadData(byte[] value){data=value;}
        int length(){return data==null?0:data.length;}
    }
    private static boolean hasPort(String address) {
        return address!=null&&address.length()>6&&address.indexOf(':',6)>=0;
    }
    private static int septets(char c) {
        if(c>=32&&c<=122&&"[\\]^`".indexOf(c)<0)return 1;
        if("\n\r£¥èéùìòÇØøÅåΔΦΓΛΩΠΨΣΘΞÆæßÉ¤¡ÄÖÑÜ§¿äöñüà".indexOf(c)>=0)return 1;
        return "\f^{}\\[~]|€".indexOf(c)>=0?2:0;
    }
    // Real segment count for well-formed payloads; at least 1 so "segments > 0"
    // checks that games run before paying always pass.
    private static int payloadSegments(Container msg,boolean port) {
        if(msg instanceof Binary){
            byte[] data=((Binary)msg).data;if(data==null)return 1;
            int single=port?133:140,multi=port?127:133;
            return data.length<=single?1:1+(data.length-1)/multi;
        }
        String data=((Text)msg).text;if(data==null)return 1;
        long units=0;boolean gsm=true;
        for(int i=0;i<data.length();i++){int n=septets(data.charAt(i));if(n==0)gsm=false;units+=n;}
        if(!gsm){int single=port?66:70,multi=port?63:66;return data.length()<=single?1:1+(data.length()-1)/multi;}
        if(units<=(port?152:160))return 1;
        int capacity=port?145:152,used=0,count=1;
        for(int i=0;i<data.length();i++){int n=septets(data.charAt(i));if(used+n>capacity){count++;used=0;}used+=n;}
        return count;
    }
    public int numberOfSegments(Message msg) {
        if(!(msg instanceof Container))return 1;
        Container value=(Container)msg;
        return payloadSegments(value,hasPort(value.address));
    }
    public synchronized void send(Message msg) throws IOException {
        if(msg==null)throw new NullPointerException();
        String address=msg.getAddress();
        if(address==null)address=destination;
        int length=msg instanceof Container?((Container)msg).length():0;
        Connector.__vmSmsSkip(msg instanceof Binary?"wma-binary":"wma-text",address,length,closed);
    }
    /** Empty inbox: block until close(), as a phone with no incoming SMS would. */
    public synchronized Message receive() throws IOException {
        while(!closed){
            try{wait();}catch(InterruptedException interrupted){throw new InterruptedIOException("SMS receive interrupted");}
        }
        throw new InterruptedIOException("SMS connection closed");
    }
    public synchronized void setMessageListener(MessageListener value) throws IOException {
        if(closed)throw new IOException("SMS connection closed");
        listener=value; // accepted; no incoming message is ever fabricated
    }
    public synchronized void close(){closed=true;listener=null;notifyAll();}
}
