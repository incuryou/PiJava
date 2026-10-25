package javax.microedition.media;
import java.io.InputStream;
import java.io.IOException;
import java.io.ByteArrayOutputStream;

/** Resource audio decoded into the VM mixer; optional compressed audio backend. */
public final class Manager {
    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    private static final int MAX_BYTES = 8 * 1024 * 1024;
    private Manager() {}
    /** Snapshot one caller-supplied local stream; no locator is opened here. */
    public static Player createPlayer(javax.microedition.media.protocol.DataSource source) throws IOException, MediaException {
        if(source==null) throw new IllegalArgumentException("null data source");
        boolean started=false,success=false;
        try {
            source.connect();
            String type=source.getContentType();
            javax.microedition.media.protocol.SourceStream[] streams=source.getStreams();
            if(streams==null || streams.length!=1 || streams[0]==null) throw new MediaException("one media stream required");
            javax.microedition.media.protocol.SourceStream stream=streams[0];
            if(type==null || type.trim().length()==0) {
                javax.microedition.media.protocol.ContentDescriptor descriptor=stream.getContentDescriptor();
                type=descriptor==null?null:descriptor.getContentType();
                if(type!=null && type.trim().length()==0) type=null;
            }
            if(type!=null && normalize(type)==null) throw new MediaException("unsupported media type: "+type);
            int transfer=stream.getTransferSize();
            if(transfer>MAX_BYTES) throw new IOException("media transfer exceeds 8 MiB limit");
            byte[] buffer=new byte[transfer>4096?transfer:4096];
            ByteArrayOutputStream output=new ByteArrayOutputStream(4096);
            source.start(); started=true;
            while(true) {
                int remaining=MAX_BYTES-output.size();
                int requested=buffer.length;
                int count=stream.read(buffer,0,requested);
                if(count==-1) break;
                if(count<=0 || count>requested) throw new IOException("invalid media stream read count");
                if(count>remaining) throw new IOException("media exceeds 8 MiB limit");
                output.write(buffer,0,count);
            }
            Player result=createPlayer(new java.io.ByteArrayInputStream(output.toByteArray()),type);
            success=true; return result;
        } finally {
            // An application read/connect error stays primary if cleanup also fails.
            Throwable cleanup=null;
            if(started) try { source.stop(); } catch(Throwable error) { cleanup=error; }
            try { source.disconnect(); } catch(Throwable error) { if(cleanup==null) cleanup=error; }
            if(success && cleanup!=null) {
                if(cleanup instanceof IOException) throw (IOException)cleanup;
                if(cleanup instanceof RuntimeException) throw (RuntimeException)cleanup;
                if(cleanup instanceof Error) throw (Error)cleanup;
                throw new IOException("data source cleanup failed");
            }
        }
    }
    public static Player createPlayer(String locator) throws IOException, MediaException {
        if (locator == null) throw new IllegalArgumentException("null locator");
        String path;
        if (locator.startsWith("resource://")) path = locator.substring(11);
        else if (locator.startsWith("resource:")) path = locator.substring(9);
        else if (locator.startsWith("/")) path = locator;
        else throw new MediaException("unsupported media locator: " + locator);
        if (!path.startsWith("/")) path = "/" + path;
        InputStream stream = Manager.class.getResourceAsStream(path);
        if (stream == null) throw new IOException("media resource not found: " + path);
        try { return createPlayer(stream, null); }
        finally { stream.close(); }
    }
    public static Player createPlayer(InputStream stream, String type) throws IOException, MediaException {
        if (stream == null) throw new IllegalArgumentException("null stream");
        String normalized = normalize(type);
        if (type != null && normalized == null) throw new MediaException("unsupported media type: " + type);
        ByteArrayOutputStream output = new ByteArrayOutputStream(4096);
        byte[] buffer = new byte[4096];
        while (true) {
            int remaining = MAX_BYTES - output.size();
            int count = stream.read(buffer, 0, remaining < buffer.length ? remaining + 1 : buffer.length);
            if (count < 0) break;
            if (count == 0) {
                int one = stream.read();
                if (one < 0) break;
                if (remaining == 0) throw new IOException("media exceeds 8 MiB limit");
                output.write(one);
            } else {
                if (count > remaining) throw new IOException("media exceeds 8 MiB limit");
                output.write(buffer, 0, count);
            }
        }
        byte[] data = output.toByteArray();
        if (normalized == null) {
            if (data.length >= 4 && data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd') normalized = "audio/midi";
            else if (data.length >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F'
                    && data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E') normalized = "audio/wav";
            else if(NativePlayer.nExtendedAudio()) normalized="audio/auto";
            else throw new MediaException("unrecognized media format");
        }
        return new NativePlayer(data, normalized);
    }
    private static String normalize(String type) {
        if (type == null) return null;
        type = type.toLowerCase().trim();int parameter=type.indexOf(';');if(parameter>=0)type=type.substring(0,parameter).trim();
        if(type.equals("audio/x-nokia-ota"))return type;
        if (type.equals("audio/midi") || type.equals("audio/x-midi") || type.equals("audio/mid") || type.equals("audio/x-mid") || type.equals("audio/sp-midi")) return "audio/midi";
        if (type.equals("audio/wav") || type.equals("audio/x-wav") || type.equals("audio/wave")) return "audio/wav";
        if(type.equals("audio/mp3")||type.equals("audio/x-mp3"))type="audio/mpeg";
        if(NativePlayer.nCodecSupported(type))return type;
        return null;
    }
    public static String[] getSupportedContentTypes(String protocol) {
        if (protocol != null && !protocol.equals("resource")) return new String[0];
        String[] base={"audio/midi","audio/x-midi","audio/sp-midi","audio/wav","audio/x-wav","audio/x-nokia-ota"};
        String[] extra={"audio/mpeg","audio/mp3","audio/amr","audio/amr-wb","audio/aac","audio/ogg"};
        int count=base.length;for(int i=0;i<extra.length;i++)if(normalize(extra[i])!=null)count++;
        String[] result=new String[count];System.arraycopy(base,0,result,0,base.length);count=base.length;
        for(int i=0;i<extra.length;i++)if(normalize(extra[i])!=null)result[count++]=extra[i];
        return result;
    }
    public static String[] getSupportedProtocols(String type) {
        if (type != null && normalize(type) == null) return new String[0];
        return new String[] { "resource" };
    }
    public static void playTone(int note, int duration, int volume) throws MediaException {
        if (note < 0 || note > 127 || duration <= 0) throw new IllegalArgumentException("tone range");
        int level = volume < 0 ? 0 : volume > 100 ? 100 : volume;
        if (!NativePlayer.nTone(note, duration, level)) throw new MediaException("cannot play tone");
    }
}
