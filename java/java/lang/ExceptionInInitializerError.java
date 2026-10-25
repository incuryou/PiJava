package java.lang;
public class ExceptionInInitializerError extends LinkageError {
    private Throwable exception;
    public ExceptionInInitializerError() { }
    public ExceptionInInitializerError(String message) { super(message); }
    public ExceptionInInitializerError(Throwable exception) { this.exception=exception; }
    public Throwable getException() { return exception; }
    public Throwable getCause() { return exception; }
}
