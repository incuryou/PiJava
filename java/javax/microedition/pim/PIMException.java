package javax.microedition.pim;

/** Checked JSR-75 failure with its reason code. */
public class PIMException extends Exception {
    public static final int FEATURE_NOT_SUPPORTED = 0;
    public static final int GENERAL_ERROR = 1;
    public static final int LIST_CLOSED = 2;
    public static final int LIST_NOT_ACCESSIBLE = 3;
    public static final int MAX_CATEGORIES_EXCEEDED = 4;
    public static final int UNSUPPORTED_VERSION = 5;
    public static final int UPDATE_ERROR = 6;
    private final int reason;

    public PIMException() { reason = 0; }
    public PIMException(String message) { super(message); reason = 0; }
    public PIMException(String message, int reason) { super(message); this.reason = reason; }
    public int getReason() { return reason; }
}
