package javax.microedition.io;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Local JAR resources and SMS (always reported sent, 0.2.107); other protocols unavailable. */
public class Connector {
    public static final int READ = 1;
    public static final int WRITE = 2;
    public static final int READ_WRITE = 3;
    private Connector() {}
    public static Connection open(String name) throws IOException {
        return open(name, READ_WRITE, false);
    }
    public static Connection open(String name, int mode) throws IOException {
        return open(name, mode, false);
    }
    public static Connection open(String name, int mode, boolean timeouts) throws IOException {
        checkName(name);
        if (mode != READ && mode != WRITE && mode != READ_WRITE)
            throw new IllegalArgumentException("Invalid connection mode");
        if (name.indexOf(':') <= 0)
            throw new IllegalArgumentException("Connection name requires a protocol");
        if (name.startsWith("sms://")) return javax.wireless.messaging.OfflineSmsConnection.open(name, mode);
        // No transport handler exists for other protocols.
        throw new ConnectionNotFoundException(name);
    }
    public static InputStream openInputStream(String name) throws IOException {
        checkName(name);
        // Bare resource paths retain the existing read-only JAR lookup extension.
        // A URI scheme must not be silently interpreted as a local resource path.
        if (name.indexOf(':') >= 0) throw new ConnectionNotFoundException(name);
        InputStream in = openStream(name);
        if (in == null) throw new ConnectionNotFoundException(name);
        return in;
    }
    public static DataInputStream openDataInputStream(String name) throws IOException {
        return new DataInputStream(openInputStream(name));
    }
    public static OutputStream openOutputStream(String name) throws IOException {
        checkName(name);
        throw new ConnectionNotFoundException(name);
    }
    public static DataOutputStream openDataOutputStream(String name) throws IOException {
        return new DataOutputStream(openOutputStream(name));
    }
    private static void checkName(String name) {
        if (name == null || name.length() == 0) throw new IllegalArgumentException("Invalid connection name");
    }
    private static native InputStream openStream(String name);
    /** VM-internal (0.2.107): logs one SMSSKIP line; the SMS/payment is treated as sent. */
    public static native void __vmSmsSkip(String kind, String address, int length, boolean closed);
}
