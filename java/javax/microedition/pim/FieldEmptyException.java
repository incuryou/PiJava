package javax.microedition.pim;

/** JSR-75 field failure retaining the affected field identifier. */
public class FieldEmptyException extends RuntimeException {
    private final int field;

    public FieldEmptyException() { field = 0; }
    public FieldEmptyException(String message) { super(message); field = 0; }
    public FieldEmptyException(String message, int field) { super(message); this.field = field; }
    public int getField() { return field; }
}
