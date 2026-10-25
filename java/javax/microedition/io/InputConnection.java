package javax.microedition.io;
import java.io.DataInputStream;
import java.io.InputStream;
import java.io.IOException;
public interface InputConnection extends Connection {
    InputStream openInputStream() throws IOException;
    DataInputStream openDataInputStream() throws IOException;
}
