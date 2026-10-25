package java.io;
public class InterruptedIOException extends IOException {
    public int bytesTransferred;
    public InterruptedIOException() {}
    public InterruptedIOException(String message) { super(message); }
}
