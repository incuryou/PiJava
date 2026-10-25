package javax.microedition.media.protocol;
import java.io.IOException;
import javax.microedition.media.Controllable;
public interface SourceStream extends Controllable {
    int NOT_SEEKABLE=0;
    int SEEKABLE_TO_START=1;
    int RANDOM_ACCESSIBLE=2;
    ContentDescriptor getContentDescriptor();
    long getContentLength();
    int getTransferSize();
    int read(byte[] buffer,int offset,int length) throws IOException;
    int getSeekType();
    long seek(long where) throws IOException;
    long tell();
}
