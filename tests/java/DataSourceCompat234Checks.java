import java.io.IOException;
import javax.microedition.media.Control;
import javax.microedition.media.Manager;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.protocol.ContentDescriptor;
import javax.microedition.media.protocol.DataSource;
import javax.microedition.media.protocol.SourceStream;

public final class DataSourceCompat234Checks {
    private static int assertions;
    private static void check(boolean value,String label){assertions++;if(!value)throw new RuntimeException(label);}
    private static final class Stream implements SourceStream {
        byte[] bytes=NativeMediaChecks.wav(); int at; int mode; int transfer=257; String type="audio/wav";
        public ContentDescriptor getContentDescriptor(){return new ContentDescriptor(type);}
        public long getContentLength(){return bytes.length;}
        public int getTransferSize(){return transfer;}
        public int read(byte[] b,int off,int len)throws IOException{
            if(mode==1)return 0;if(mode==2)return len+1;if(mode==3)throw new IOException("read-primary");if(mode==4)return -2;
            if(mode==6 && len<transfer)throw new IOException("short transfer block");
            if(at==bytes.length)return -1;int n=bytes.length-at;if(n>len)n=len;
            System.arraycopy(bytes,at,b,off,n);at+=n;return n;
        }
        public int getSeekType(){return NOT_SEEKABLE;}
        public long seek(long where)throws IOException{throw new IOException("not seekable");}
        public long tell(){return at;}
        public Control getControl(String type){return null;}
        public Control[] getControls(){return new Control[0];}
    }
    private static final class Source extends DataSource {
        Stream stream=new Stream(); int connects,starts,stops,disconnects;int count=1;boolean connected,started,failConnect,failStop;String type="audio/wav";
        Source(){super("caller:opaque-not-opened");}
        public String getContentType(){if(!connected)throw new IllegalStateException("not connected");return type;}
        public void connect()throws IOException{connects++;if(failConnect)throw new IOException("connect-primary");connected=true;}
        public void disconnect(){disconnects++;connected=false;started=false;}
        public void start()throws IOException{if(!connected)throw new IOException("not connected");starts++;started=true;}
        public void stop()throws IOException{stops++;started=false;if(failStop)throw new IOException("stop-cleanup");}
        public SourceStream[] getStreams(){if(!connected)throw new IllegalStateException("not connected");SourceStream[] streams=new SourceStream[count];for(int i=0;i<count;i++)streams[i]=stream;return streams;}
        public Control getControl(String type){return null;}
        public Control[] getControls(){return new Control[0];}
    }
    private static void expectBadRead(int mode)throws Exception{
        Source s=new Source();s.stream.mode=mode;boolean rejected=false;
        try{Manager.createPlayer(s);}catch(IOException expected){rejected=true;}
        check(rejected&&s.connects==1&&s.starts==1&&s.stops==1&&s.disconnects==1,"invalid SourceStream read rejected and cleaned");
    }
    public static int run()throws Exception{
        assertions=0;check(SourceStream.NOT_SEEKABLE==0&&SourceStream.SEEKABLE_TO_START==1&&SourceStream.RANDOM_ACCESSIBLE==2,"seek ABI constants");
        check(new ContentDescriptor(null).getContentType()==null,"descriptor permits unknown content type");
        Source s=new Source();check(s.getLocator().equals("caller:opaque-not-opened"),"locator retained without transport");
        Player p=Manager.createPlayer(s);check(s.connects==1&&s.starts==1&&s.stops==1&&s.disconnects==1&&!s.connected,"source copied with balanced lifecycle");
        check(s.stream.tell()==16044&&p.getState()==Player.UNREALIZED,"all bytes copied into unrealized player");
        p.realize();check(p.getDuration()==1000000&&p.getContentType().equals("audio/wav"),"caller source reaches real decoder");p.start();p.close();
        check(s.stops==1&&s.disconnects==1,"player owns copy rather than caller source");
        s=new Source();s.type=null;p=Manager.createPlayer(s);p.realize();check(p.getContentType().equals("audio/wav"),"stream descriptor fallback");p.close();
        s=new Source();s.type=null;s.stream.type=null;p=Manager.createPlayer(s);p.realize();check(p.getDuration()==1000000,"unknown descriptor uses actual media magic");p.close();
        s=new Source();s.stream.transfer=8192;s.stream.mode=6;p=Manager.createPlayer(s);p.realize();check(p.getDuration()==1000000,"source transfer blocks respected through EOF");p.close();
        boolean rejected=false;try{Manager.createPlayer((DataSource)null);}catch(IllegalArgumentException expected){rejected=true;}check(rejected,"null source rejected");
        s=new Source();s.count=0;rejected=false;try{Manager.createPlayer(s);}catch(MediaException expected){rejected=true;}check(rejected&&s.starts==0&&s.disconnects==1,"unsupported stream count rejected before start");
        s=new Source();s.count=2;rejected=false;try{Manager.createPlayer(s);}catch(MediaException expected){rejected=true;}check(rejected&&s.starts==0&&s.disconnects==1,"multiple streams not silently truncated");
        expectBadRead(1);expectBadRead(2);expectBadRead(4);
        s=new Source();s.stream.mode=3;s.failStop=true;rejected=false;try{Manager.createPlayer(s);}catch(IOException expected){rejected=expected.getMessage().equals("read-primary");}check(rejected&&s.disconnects==1,"read exception survives cleanup failure");
        s=new Source();s.failStop=true;rejected=false;try{Manager.createPlayer(s);}catch(IOException expected){rejected=expected.getMessage().equals("stop-cleanup");}check(rejected&&s.disconnects==1,"successful read cannot hide stop failure");
        s=new Source();s.failConnect=true;rejected=false;try{Manager.createPlayer(s);}catch(IOException expected){rejected=true;}check(rejected&&s.starts==0&&s.disconnects==1,"failed connect still releases partial connection");
        s=new Source();s.type="application/unsupported";rejected=false;try{Manager.createPlayer(s);}catch(MediaException expected){rejected=true;}check(rejected&&s.disconnects==1,"unsupported MIME remains unsupported");
        return assertions;
    }
}
