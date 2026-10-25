import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.util.Date;
import javax.microedition.io.Connection;
import javax.microedition.io.ConnectionNotFoundException;
import javax.microedition.io.Connector;
import javax.microedition.io.ContentConnection;
import javax.microedition.io.HttpConnection;
import javax.microedition.io.InputConnection;
import javax.microedition.io.OutputConnection;
import javax.microedition.io.StreamConnection;
import javax.microedition.io.StreamConnectionNotifier;
import javax.microedition.midlet.MIDletStateChangeException;
import javax.wireless.messaging.BinaryMessage;
import javax.wireless.messaging.Message;
import javax.wireless.messaging.MessageConnection;
import javax.wireless.messaging.MessageListener;
import javax.wireless.messaging.TextMessage;

/** The VM harness supplies connection-abi.bin containing 01 23 45 67. */
public final class ConnectionAbiChecks {
    private static int checks;
    private static void check(boolean ok, String label) {
        if (!ok) throw new RuntimeException(label);
        checks++;
    }
    public static int run() throws Exception {
        checks = 0;
        HttpConnection http = new ConnectionAbiHttp();
        ContentConnection content = http;
        StreamConnection stream = content;
        InputConnection input = stream;
        OutputConnection output = stream;
        Connection connection = input;
        check(connection instanceof HttpConnection && connection instanceof ContentConnection
            && connection instanceof StreamConnection && connection instanceof OutputConnection,
            "transitive interface instanceof");
        check(content.getLength() == 4L && content.getType().equals("application/test")
            && content.getEncoding() == null, "content ABI");
        check(input.openDataInputStream().readInt() == 0x01234567, "input interface dispatch");
        output.openDataOutputStream().writeInt(0x12345678);
        check(((ConnectionAbiHttp)http).out.size() == 4, "output interface dispatch");
        check(http.getHeaderFieldDate("date", 1234567890123L) == 1234567890123L
            && http.getHeaderFieldInt("unknown", 17) == 17 && http.getHeaderField(0) == null,
            "HTTP header descriptors");
        check(HttpConnection.HEAD.equals("HEAD") && HttpConnection.HTTP_OK == 200
            && HttpConnection.HTTP_TEMP_REDIRECT == 307 && HttpConnection.HTTP_VERSION == 505,
            "HTTP standard constants");
        connection.close();
        boolean rejected = false;
        try { input.openInputStream(); } catch (IOException expected) { rejected = true; }
        check(rejected, "checked exception via InputConnection");
        StreamConnectionNotifier notifier = new ConnectionAbiNotifier();
        Connection listenerConnection = notifier;
        check(listenerConnection instanceof StreamConnectionNotifier
            && notifier.acceptAndOpen() instanceof StreamConnection, "notifier ABI");
        listenerConnection.close();

        MessageConnection messaging = new ConnectionAbiMessaging();
        connection = messaging;
        check(connection instanceof MessageConnection, "messaging Connection inheritance");
        Message message = messaging.newMessage(MessageConnection.BINARY_MESSAGE, "sms://test");
        check(message instanceof BinaryMessage && message.getAddress().equals("sms://test"), "binary message ABI");
        BinaryMessage binary = (BinaryMessage)message;
        byte[] payload = new byte[] { 5, 6 };
        binary.setPayloadData(payload);
        check(binary.getPayloadData() == payload && message.getTimestamp().getTime() == 1234567890123L,
            "binary payload and timestamp ABI");
        TextMessage text = (TextMessage)messaging.newMessage(MessageConnection.TEXT_MESSAGE);
        text.setPayloadText("test");
        text.setAddress("sms://other");
        check(text.getPayloadText().equals("test") && text.getAddress().equals("sms://other"), "text message ABI");
        ConnectionAbiListener listener = new ConnectionAbiListener();
        messaging.setMessageListener(listener);
        listener.notifyIncomingMessage(messaging);
        check(listener.seen == messaging, "MessageListener descriptor");
        rejected = false;
        try { messaging.send(message); } catch (InterruptedIOException expected) {
            rejected = expected.bytesTransferred == 2 && expected.getMessage().equals("test interruption");
        }
        check(rejected, "messaging interrupted IOException ABI");
        rejected = false;
        try { messaging.receive(); } catch (IOException expected) { rejected = true; }
        check(rejected, "messaging receive checked exception");
        connection.close();
        check(new InterruptedIOException() instanceof IOException
            && new InterruptedIOException().bytesTransferred == 0, "interrupted exception defaults");
        check(new MIDletStateChangeException("retry").getMessage().equals("retry")
            && new MIDletStateChangeException() instanceof Exception, "MIDlet exception constructors");
        check(new ConnectionNotFoundException("missing").getMessage().equals("missing"), "connection exception message");

        rejected = false;
        try { Connector.open("http://example.invalid"); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "HTTP cannot report fabricated success");
        // 0.2.107 policy (kept in 0.2.111): SMS is always reported sent, so any sms:// recipient opens.
        boolean opened = false;
        try { opened = Connector.open("sms://test", Connector.READ_WRITE, true) != null; }
        catch (Exception unexpected) { opened = false; }
        check(opened, "SMS adapter accepts any recipient (always sent)");
        rejected = false;
        try { Connector.open("socket://test", Connector.WRITE); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "unknown protocol unsupported");
        rejected = false;
        try { Connector.open("http://test", 0); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "invalid mode");
        rejected = false;
        try { Connector.open("connection-abi.bin", Connector.READ); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "open requires protocol");
        rejected = false;
        try { Connector.openInputStream(null); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "null input name");
        rejected = false;
        try { Connector.openOutputStream(""); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "empty output name");
        rejected = false;
        try { Connector.openInputStream("http://test"); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "input protocol unsupported");
        rejected = false;
        try { Connector.openDataInputStream("connection-abi-missing.bin"); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "missing resource");
        rejected = false;
        try { Connector.openOutputStream("connection-abi.bin"); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "resource output cannot silently discard bytes");
        rejected = false;
        try { Connector.openDataOutputStream("http://test"); }
        catch (ConnectionNotFoundException expected) { rejected = true; }
        check(rejected, "data output cannot silently discard bytes");
        InputStream resource = Connector.openInputStream("connection-abi.bin");
        check(resource.read() == 1 && resource.read() == 35, "resource input native entry");
        DataInputStream resourceData = Connector.openDataInputStream("connection-abi.bin");
        resource.close();
        check(resourceData.readInt() == 0x01234567 && resourceData.read() == -1, "independent resource streams");
        resourceData.close();
        resource = Connector.openInputStream("/connection-abi.bin");
        check(resource.read() == 1, "resource leading slash retained");
        resource.close();
        return checks;
    }
}

final class ConnectionAbiHttp implements HttpConnection {
    boolean closed;
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    public void close() throws IOException { closed = true; }
    public InputStream openInputStream() throws IOException {
        if (closed) throw new IOException("closed");
        return new ByteArrayInputStream(new byte[] { 1, 35, 69, 103 });
    }
    public DataInputStream openDataInputStream() throws IOException { return new DataInputStream(openInputStream()); }
    public OutputStream openOutputStream() throws IOException { return out; }
    public DataOutputStream openDataOutputStream() throws IOException { return new DataOutputStream(openOutputStream()); }
    public String getType() { return "application/test"; }
    public String getEncoding() { return null; }
    public long getLength() { return 4L; }
    public String getURL() { return "http://test"; }
    public String getProtocol() { return "http"; }
    public String getHost() { return "test"; }
    public String getFile() { return "/"; }
    public String getRef() { return null; }
    public String getQuery() { return null; }
    public int getPort() { return 80; }
    public String getRequestMethod() { return GET; }
    public void setRequestMethod(String method) throws IOException { throw new IOException("test only"); }
    public String getRequestProperty(String key) { return null; }
    public void setRequestProperty(String key, String value) throws IOException { throw new IOException("test only"); }
    public int getResponseCode() throws IOException { throw new IOException("test only"); }
    public String getResponseMessage() throws IOException { throw new IOException("test only"); }
    public long getExpiration() throws IOException { return 0; }
    public long getDate() throws IOException { return 0; }
    public long getLastModified() throws IOException { return 0; }
    public String getHeaderField(String name) throws IOException { return null; }
    public int getHeaderFieldInt(String name, int def) throws IOException { return def; }
    public long getHeaderFieldDate(String name, long def) throws IOException { return def; }
    public String getHeaderField(int n) throws IOException { return null; }
    public String getHeaderFieldKey(int n) throws IOException { return null; }
}
final class ConnectionAbiNotifier implements StreamConnectionNotifier {
    public StreamConnection acceptAndOpen() throws IOException { return new ConnectionAbiHttp(); }
    public void close() throws IOException {}
}
final class ConnectionAbiMessage implements BinaryMessage, TextMessage {
    String address;
    String text;
    byte[] bytes;
    public String getAddress() { return address; }
    public void setAddress(String value) { address = value; }
    public Date getTimestamp() { return new Date(1234567890123L); }
    public byte[] getPayloadData() { return bytes; }
    public void setPayloadData(byte[] value) { bytes = value; }
    public String getPayloadText() { return text; }
    public void setPayloadText(String value) { text = value; }
}
final class ConnectionAbiMessaging implements MessageConnection {
    public Message newMessage(String type) { return newMessage(type, null); }
    public Message newMessage(String type, String address) {
        Message result = new ConnectionAbiMessage();
        result.setAddress(address);
        return result;
    }
    public int numberOfSegments(Message msg) { return 1; }
    public Message receive() throws IOException { throw new IOException("test only"); }
    public void send(Message msg) throws IOException, InterruptedIOException {
        InterruptedIOException failure = new InterruptedIOException("test interruption");
        failure.bytesTransferred = 2;
        throw failure;
    }
    public void setMessageListener(MessageListener listener) throws IOException {}
    public void close() throws IOException {}
}
final class ConnectionAbiListener implements MessageListener {
    MessageConnection seen;
    public void notifyIncomingMessage(MessageConnection conn) { seen = conn; }
}
