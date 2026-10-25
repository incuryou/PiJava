package javax.microedition.lcdui;
public class TextField extends Item {
    public static final int ANY = 0, EMAILADDR = 1, NUMERIC = 2, PHONENUMBER = 3, URL = 4, DECIMAL = 5;
    public static final int PASSWORD = 0x10000, UNEDITABLE = 0x20000, SENSITIVE = 0x40000;
    public static final int NON_PREDICTIVE = 0x80000, INITIAL_CAPS_WORD = 0x100000, INITIAL_CAPS_SENTENCE = 0x200000;
    public static final int CONSTRAINT_MASK = 0xFFFF;
    String text;
    int maxSize;
    int constraints;
    private int caret;
    public TextField(String label, String text, int maxSize, int constraints) {
        if (maxSize <= 0) throw new IllegalArgumentException();
        this.label = label; this.maxSize = maxSize; checkConstraints(constraints); this.constraints = constraints; setString(text);
    }
    private static void checkConstraints(int value) { if ((value & CONSTRAINT_MASK) > DECIMAL || (value & ~0x3F0007) != 0) throw new IllegalArgumentException(); }
    private boolean valid(String value) {
        int mode = constraints & CONSTRAINT_MASK;
        if (mode != NUMERIC && mode != DECIMAL && mode != PHONENUMBER) return true;
        boolean point = false;
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c >= '0' && c <= '9') continue;
            if (mode == PHONENUMBER && (c == '+' || c == '*' || c == '#' || c == ' ' || c == '-' || c == '(' || c == ')')) continue;
            if (mode != PHONENUMBER && c == '-' && i == 0 && value.length() > 1) continue;
            if (mode == DECIMAL && c == '.' && !point && value.length() > 1) { point = true; continue; }
            return false;
        }
        return true;
    }
    public String getString() { return text; }
    public void setString(String value) {
        if (value == null) value = "";
        if (value.length() > maxSize || !valid(value)) throw new IllegalArgumentException();
        text = value; caret = text.length(); changed();
    }
    public int getChars(char[] data) {
        if (data == null) throw new NullPointerException();
        if (data.length < text.length()) throw new ArrayIndexOutOfBoundsException();
        text.getChars(0, text.length(), data, 0); return text.length();
    }
    public void setChars(char[] data, int offset, int length) {
        if (data == null) { setString(null); return; }
        range(data, offset, length); setString(new String(data, offset, length));
    }
    private static void range(char[] data, int offset, int length) {
        if (offset < 0 || length < 0 || offset > data.length - length) throw new ArrayIndexOutOfBoundsException();
    }
    public void insert(String value, int position) {
        if (value == null) throw new NullPointerException();
        if (position < 0) position = 0; if (position > text.length()) position = text.length();
        setString(text.substring(0, position) + value + text.substring(position)); caret = position + value.length();
    }
    public void insert(char[] data, int offset, int length, int position) {
        if (data == null) throw new NullPointerException(); range(data, offset, length); insert(new String(data, offset, length), position);
    }
    public void delete(int offset, int length) {
        if (offset < 0 || length < 0 || offset > text.length() - length) throw new StringIndexOutOfBoundsException();
        setString(text.substring(0, offset) + text.substring(offset + length)); caret = offset;
    }
    public int getMaxSize() { return maxSize; }
    public int setMaxSize(int value) {
        if (value <= 0) throw new IllegalArgumentException();
        String next = text.length() > value ? text.substring(0, value) : text;
        if (!valid(next)) throw new IllegalArgumentException();
        maxSize = value; text = next; if (caret > text.length()) caret = text.length(); changed(); return maxSize;
    }
    public int getConstraints() { return constraints; }
    public void setConstraints(int value) { checkConstraints(value); constraints = value; if (!valid(text)) { text = ""; caret = 0; } changed(); }
    public void setInitialInputMode(String characterSubset) {}
    public int size() { return text.length(); }
    public int getCaretPosition() { return caret; }
    boolean editKey(int code) {
        if ((constraints & UNEDITABLE) != 0) return false;
        if (code == -3 || code == 2) { if (caret > 0) caret--; return false; }
        if (code == -4 || code == 5) { if (caret < text.length()) caret++; return false; }
        if (code == -8 || code == 127 || code == 8) {
            if (caret == 0) return false;
            try { delete(caret - 1, 1); return true; } catch (IllegalArgumentException invalid) { return false; }
        }
        if (code >= 48 && code <= 57 || code == 42 || code == 35) {
            if (text.length() >= maxSize) return false;
            String value = code == 42 && (constraints & CONSTRAINT_MASK) == DECIMAL ? "." : String.valueOf((char)code);
            try { insert(value, caret); return true; } catch (IllegalArgumentException invalid) { return false; }
        }
        return false;
    }
    String displayText() {
        if ((constraints & PASSWORD) == 0) return text;
        String result = ""; for (int i = 0; i < text.length(); i++) result += "*"; return result;
    }
    public String labelText() { String a = label == null ? "" : label; return a + (a.length() > 0 ? " " : "") + displayText(); }
}
