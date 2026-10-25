package java.lang;
public final class Boolean {
    public static final Boolean TRUE = new Boolean(true);
    public static final Boolean FALSE = new Boolean(false);
    boolean value;
    public Boolean(boolean value) { this.value = value; }
    public boolean booleanValue() { return value; }
    public static Boolean valueOf(boolean value) { return value ? TRUE : FALSE; }
    public String toString() { return value ? "true" : "false"; }
}
