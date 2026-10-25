package javax.wireless.messaging;
import java.io.IOException;
import java.io.InterruptedIOException;
import javax.microedition.io.Connection;
public interface MessageConnection extends Connection {
    String TEXT_MESSAGE = "text";
    String BINARY_MESSAGE = "binary";
    Message newMessage(String type);
    Message newMessage(String type, String address);
    int numberOfSegments(Message msg);
    Message receive() throws IOException, InterruptedIOException;
    void send(Message msg) throws IOException, InterruptedIOException;
    void setMessageListener(MessageListener listener) throws IOException;
}
