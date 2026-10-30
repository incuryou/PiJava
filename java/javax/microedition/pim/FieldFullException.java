package javax.microedition.pim;

/** JSR-75 field failure retaining the affected field identifier. */
public class FieldFullException extends RuntimeException {
    private final int field;

    public FieldFullException() { field = 0; }
    public FieldFullException(String message) { super(message); field = 0; }
    public FieldFullException(String message, int field) { super(message); this.field = field; }
    public int getField() { return field; }
}
