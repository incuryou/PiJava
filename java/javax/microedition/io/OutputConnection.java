package javax.microedition.io;
import java.io.DataOutputStream;
import java.io.OutputStream;
import java.io.IOException;
public interface OutputConnection extends Connection {
    OutputStream openOutputStream() throws IOException;
    DataOutputStream openDataOutputStream() throws IOException;
}
