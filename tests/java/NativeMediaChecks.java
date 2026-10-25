import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import javax.microedition.media.Manager;
import javax.microedition.media.MediaException;
import javax.microedition.media.NativePlayer;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import com.nokia.mid.sound.Sound;

/** Decoder/lifecycle integration checks run inside the custom interpreter. */
public final class NativeMediaChecks {
    private static int assertions;
    private static void check(boolean value, String label) {
        assertions++;
        if (!value) throw new RuntimeException(label);
    }
    public static byte[] wav() {
        byte[] data = new byte[16044];
        data[0]='R'; data[1]='I'; data[2]='F'; data[3]='F'; put32(data,4,16036);
        data[8]='W'; data[9]='A'; data[10]='V'; data[11]='E';
        data[12]='f'; data[13]='m'; data[14]='t'; data[15]=' '; put32(data,16,16);
        put16(data,20,1); put16(data,22,1); put32(data,24,8000); put32(data,28,16000);
        put16(data,32,2); put16(data,34,16);
        data[36]='d'; data[37]='a'; data[38]='t'; data[39]='a'; put32(data,40,16000);
        int i=0;
        while (i<8000) { put16(data,44+i*2,i%16<8?10000:-10000); i++; }
        return data;
    }
    private static void put16(byte[] data, int at, int value) {
        data[at]=(byte)value; data[at+1]=(byte)(value>>8);
    }
    private static void put32(byte[] data, int at, int value) {
        put16(data,at,value); put16(data,at+2,value>>16);
    }
    private static byte[] midi() {
        return new byte[] { 'M','T','h','d',0,0,0,6,0,0,0,1,0,96,
            'M','T','r','k',0,0,0,12,0,(byte)0x90,60,100,96,(byte)0x80,60,0,0,(byte)0xff,47,0 };
    }
    private static final class Source extends ByteArrayInputStream {
        boolean closed;
        boolean zero = true;
        Source(byte[] data) { super(data); }
        public int read(byte[] data, int off, int length) {
            if (zero) { zero = false; return 0; }
            return super.read(data,off,length);
        }
        public void close() { closed = true; }
    }
    private static final class OversizeSource extends InputStream {
        int count;
        public int read() { count++; return 0; }
        public int read(byte[] data, int off, int length) { count += length; return length; }
    }
    private static final class Listener implements PlayerListener {
        int ended;
        int closed;
        public void playerUpdate(Player player, String event, Object data) {
            if (event.equals(END_OF_MEDIA)) ended++;
            if (event.equals(CLOSED)) closed++;
        }
    }
    public static int run() throws Exception {
        assertions=0;
        Source source = new Source(wav());
        Player player = Manager.createPlayer(source,null);
        check(source.available()==0 && !source.closed,"consumes stream without taking ownership and zero-read progresses");
        check(player instanceof NativePlayer && player.getState()==Player.UNREALIZED,"real backend initial state");
        check(player.getDuration()==Player.TIME_UNKNOWN,"duration unknown before realization");
        Listener listener = new Listener(); player.addPlayerListener(listener);
        player.realize();
        check(player.getDuration()==1000000L && player.getContentType().equals("audio/wav"),"WAV sample duration and sniffing");
        check(player.setMediaTime(500000L)==500000L && player.getMediaTime()==500000L,"real decoder seek");
        check(player.setMediaTime(999999999999L)==1000000L,"seek clamps to actual duration");
        player.setMediaTime(0); player.setLoopCount(2); player.start();
        check(player.getState()==Player.STARTED,"native player starts");
        NativePlayer.dispatchEnd(-1);
        check(listener.ended==0 && player.getState()==Player.STARTED,"unknown completion ignored");
        player.stop(); player.deallocate();
        check(player.getState()==Player.REALIZED,"native deallocate state");
        player.close(); player.close();
        check(listener.closed==1 && listener.ended==0,"close once without artificial completion");
        check(!source.closed,"player close does not close caller source");
        player=Manager.createPlayer(new ByteArrayInputStream(midi()),"audio/x-midi");
        player.realize();
        check(player.getDuration()==500000L && player.getContentType().equals("audio/midi"),"MIDI timing and alias normalization");
        player.close();
        boolean rejected=false;
        player=Manager.createPlayer(new ByteArrayInputStream(new byte[]{1,2,3}),"audio/midi");
        try { player.realize(); } catch(MediaException expected) { rejected=true; }
        check(rejected && player.getState()==Player.UNREALIZED,"malformed MIDI fails realization"); player.close();
        rejected=false;
        try { Manager.createPlayer(new ByteArrayInputStream(wav()),"application/x-unsupported-audio"); } catch(MediaException expected) { rejected=true; }
        check(rejected,"unsupported format rejected");
        rejected=false;
        try { Manager.createPlayer("http://unopened.invalid/a.mid"); } catch(MediaException expected) { rejected=true; }
        check(rejected,"unsupported locator rejected");
        OversizeSource huge=new OversizeSource(); rejected=false;
        try { Manager.createPlayer(huge,"audio/wav"); } catch(IOException expected) { rejected=true; }
        check(rejected && huge.count==8*1024*1024+1,"media input bounded at 8 MiB plus overflow probe");
        check(Manager.getSupportedContentTypes(null).length>=6 && Manager.getSupportedContentTypes("http").length==0,"supported formats honest");
        check(Manager.getSupportedProtocols("application/x-unsupported-audio").length==0,"unsupported type has no transport");
        rejected=false; try { Manager.playTone(128,100,50); } catch(IllegalArgumentException expected) { rejected=true; }
        check(rejected,"tone note validation");
        byte[] sharedWav = wav();
        Player[] players = new Player[64];
        int i=0;
        while(i<players.length) {
            players[i]=Manager.createPlayer(new ByteArrayInputStream(sharedWav),"audio/wav");
            players[i].realize(); i++;
        }
        Player overflow=Manager.createPlayer(new ByteArrayInputStream(sharedWav),"audio/wav");
        rejected=false;
        try { overflow.realize(); } catch(MediaException expected) { rejected=true; }
        check(rejected,"player registry caps realized handles at 64"); overflow.close();
        i=0; while(i<players.length) { players[i].close(); i++; }
        player=Manager.createPlayer(new ByteArrayInputStream(sharedWav),"audio/wav"); player.realize();
        check(player.getState()==Player.REALIZED,"closed player slots are reusable"); player.close();
        Manager.playTone(60,20,25);
        Sound sound=new Sound(wav(),Sound.FORMAT_WAV);
        sound.setGain(127); check(sound.getGain()==127,"Nokia gain retained");
        sound.play(1); check(sound.getState()==Sound.SOUND_PLAYING,"Nokia WAV uses real player");
        sound.stop(); check(sound.getState()==Sound.SOUND_STOPPED,"Nokia stop");
        sound.resume(); sound.release(); check(sound.getState()==Sound.SOUND_UNINITIALIZED,"Nokia release");
        sound=new Sound(440,20); sound.play(1); check(sound.getState()==Sound.SOUND_PLAYING,"Nokia frequency tone plays"); sound.release();
        check(Sound.getSupportedFormats().length==2 && Sound.getSupportedFormats()[0]==Sound.FORMAT_TONE && Sound.getSupportedFormats()[1]==Sound.FORMAT_WAV,"Nokia OTT subset and WAV available");
        return assertions;
    }
}
