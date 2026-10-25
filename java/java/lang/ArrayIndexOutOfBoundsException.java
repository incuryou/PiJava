package java.lang;
public class ArrayIndexOutOfBoundsException extends IndexOutOfBoundsException {
    public ArrayIndexOutOfBoundsException(int index) { super(Integer.toString(index)); }
    public ArrayIndexOutOfBoundsException() {}
    public ArrayIndexOutOfBoundsException(String message) { super(message); }
}
