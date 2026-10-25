import java.util.*;
import javax.microedition.lcdui.*;
import javax.microedition.rms.*;

public class Compat217Checks extends javax.microedition.midlet.MIDlet {
    protected void startApp() {}
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
    static int checks;
    static Vector retained;
    static final String UNICODE = "档案/名\\★";
    static final String SURROGATE = new String(new char[] {'S', '\ud83d', '\ude00', '\u0000'});
    static class Slots extends Vector {
        Slots(int size, int increment) { super(size, increment); }
        int references() {
            int n = 0;
            for (int i = 0; i < elementData.length; i++) if (elementData[i] != null) n++;
            return n;
        }
    }
    static class Keys extends Canvas { protected void paint(Graphics g) {} }
    static void check(boolean value, String label) {
        if (!value) throw new IllegalStateException(label);
        checks++;
    }
    public static int collections() {
        checks = 0;
        Slots v = new Slots(0, 3);
        check(v.capacity() == 0 && v.size() == 0, "zero-capacity constructor");
        try { new Vector(-1, 2); check(false, "negative capacity"); }
        catch (IllegalArgumentException expected) { checks++; }
        check(new Vector().capacity() == 10, "default capacity");
        v.addElement("first");
        check(v.capacity() == 3, "positive increment");
        v.addElement("second"); v.addElement("third"); v.addElement(null);
        check(v.capacity() == 6 && v.indexOf(null) == 3, "increment growth and null lookup");
        v.ensureCapacity(19);
        check(v.capacity() == 19 && v.elementAt(0).equals("first"), "minimum capacity preserved");
        v.setSize(1);
        check(v.size() == 1 && v.references() == 1, "shrink releases references");
        for (int operation = 0; operation < 7; operation++) {
            try {
                if (operation == 0) v.elementAt(1);
                if (operation == 1) v.setElementAt("bad", 1);
                if (operation == 2) v.removeElementAt(4);
                if (operation == 3) v.insertElementAt("bad", 2);
                if (operation == 4) v.insertElementAt("bad", -1);
                if (operation == 5) v.removeElementAt(-1);
                if (operation == 6) v.setSize(-1);
                check(false, "logical size bounds");
            } catch (ArrayIndexOutOfBoundsException expected) { checks++; }
            check(v.size() == 1 && v.elementAt(0).equals("first") && v.references() == 1, "invalid operation preserves contents");
        }
        try { v.copyInto(null); check(false, "copy null"); }
        catch (NullPointerException expected) { checks++; }
        try { v.copyInto(new Object[0]); check(false, "copy short"); }
        catch (ArrayIndexOutOfBoundsException expected) { checks++; }
        v.insertElementAt("last", v.size()); v.insertElementAt("start", 0);
        check(v.firstElement().equals("start") && v.lastElement().equals("last"), "insert boundaries");
        Enumeration e = v.elements();
        check(e.nextElement().equals("start"), "enumeration begins");
        v.removeAllElements();
        check(v.references() == 0 && !e.hasMoreElements(), "clear releases every slot");
        try { e.nextElement(); check(false, "exhausted vector enum"); }
        catch (NoSuchElementException expected) { checks++; }
        try { v.firstElement(); check(false, "empty first"); }
        catch (NoSuchElementException expected) { checks++; }
        try { v.lastElement(); check(false, "empty last"); }
        catch (NoSuchElementException expected) { checks++; }
        v.trimToSize(); check(v.capacity() == 0, "trim empty");
        v.addElement(null); v.addElement("same"); v.addElement("same");
        check(v.lastIndexOf("same") == 2 && v.lastIndexOf(null) == 0 && v.indexOf("same", 2) == 2, "indexed search");
        v.setSize(5); check(v.elementAt(4) == null, "size growth slots initialized");
        Slots doubled = new Slots(1, -2);
        doubled.addElement("a"); doubled.addElement("b"); doubled.addElement("c");
        check(doubled.capacity() == 4, "nonpositive increment doubles");

        Hashtable h = new Hashtable(0);
        check(h.put("key", "old") == null && h.put("key", "new").equals("old"), "hashtable zero and replacement");
        h.put("another", "value");
        check(h.size() == 2 && h.get("key").equals("new") && h.contains("value"), "hashtable growth");
        try { new Hashtable(-1); check(false, "negative hashtable capacity"); }
        catch (IllegalArgumentException expected) { checks++; }
        for (int operation = 0; operation < 6; operation++) {
            try {
                if (operation == 0) h.put(null, "value");
                if (operation == 1) h.put("key", null);
                if (operation == 2) h.get(null);
                if (operation == 3) h.remove(null);
                if (operation == 4) h.containsKey(null);
                if (operation == 5) h.contains(null);
                check(false, "hashtable rejects null");
            } catch (NullPointerException expected) { checks++; }
            check(h.size() == 2 && h.get("key").equals("new"), "null operations preserve hashtable");
        }
        Enumeration he = h.keys(); he.nextElement(); he.nextElement();
        try { he.nextElement(); check(false, "exhausted hashtable enum"); }
        catch (NoSuchElementException expected) { checks++; }
        h.clear(); check(h.isEmpty(), "clear hashtable");
        Character c = new Character('中');
        check(c.charValue() == '中' && c.hashCode() == '中' && c.toString().equals("中"), "character object value");
        check(c.equals(new Character('中')) && !c.equals(new Character('A')) && !c.equals(null) && !c.equals("中"), "character equality");
        h.put(c, "character"); check(h.get(new Character('中')).equals("character"), "character key");
        check(new Character('\ud83d').toString().charAt(0) == '\ud83d', "surrogate character preserved");
        check(Character.digit('1', 1) == -1 && Character.digit('A', 37) == -1, "invalid radix");
        Keys keys = new Keys();
        check(keys.getKeyName('5').equals("5") && keys.getKeyName('#').equals("#"), "phone key names");
        for (int action = Canvas.UP; action <= Canvas.GAME_D; action++) {
            int key = keys.getKeyCode(action);
            if (key != 0) check(keys.getKeyName(key).length() > 0, "action key name");
        }
        check(keys.getKeyName(-6).length() > 0 && keys.getKeyName(-7).length() > 0, "soft key names");
        try { keys.getKeyName(999); check(false, "unknown key"); }
        catch (IllegalArgumentException expected) { checks++; }
        return checks;
    }
    static boolean listed(String name) {
        String[] names = RecordStore.listRecordStores();
        if (names == null) return false;
        for (int i = 0; i < names.length; i++) if (name.equals(names[i])) return true;
        return false;
    }
    public static int rms() throws Exception {
        checks = 0;
        check(RecordStore.listRecordStores() == null, "empty suite returns null");
        RecordStore s = RecordStore.openRecordStore(UNICODE, true);
        RecordStore shared = RecordStore.openRecordStore(UNICODE, false);
        RecordStore second = RecordStore.openRecordStore(SURROGATE, true);
        check(listed(UNICODE) && listed(SURROGATE) && RecordStore.listRecordStores().length == 2, "Unicode and exact surrogate names listed");
        check(s.getSizeAvailable() >= 0, "available space is nonnegative");
        try { RecordStore.deleteRecordStore(UNICODE); check(false, "delete open"); }
        catch (RecordStoreException expected) { checks++; }
        s.closeRecordStore(); check(shared.getSizeAvailable() >= 0, "reference-counted close");
        shared.closeRecordStore();
        check(listed(UNICODE), "closed store stays listed");
        try { s.getSizeAvailable(); check(false, "closed size available"); }
        catch (RecordStoreNotOpenException expected) { checks++; }
        try { s.getName(); check(false, "closed name"); }
        catch (RecordStoreNotOpenException expected) { checks++; }
        try { s.closeRecordStore(); check(false, "extra close"); }
        catch (RecordStoreNotOpenException expected) { checks++; }
        RecordStore.deleteRecordStore(UNICODE);
        check(!listed(UNICODE) && listed(SURROGATE), "delete reflected in list");
        second.closeRecordStore(); RecordStore.deleteRecordStore(SURROGATE);
        check(RecordStore.listRecordStores() == null, "all stores deleted returns null");
        return checks;
    }
    public static int retainArray() {
        retained = new Vector(4, 2);
        retained.addElement(new byte[262144]);
        return 1;
    }
    public static int clearArray() { retained.removeAllElements(); return retained.size(); }
    public static int create(String name) throws Exception {
        RecordStore s = RecordStore.openRecordStore(name, true);
        s.addRecord(new byte[] {42}, 0, 1);
        s.closeRecordStore(); return 1;
    }
    public static int countStores() {
        String[] names = RecordStore.listRecordStores();
        return names == null ? 0 : names.length;
    }
    public static int hasStore(String name) { return listed(name) ? 1 : 0; }
    public static int available(String name) throws Exception {
        RecordStore s = RecordStore.openRecordStore(name, false);
        int available = s.getSizeAvailable(); s.closeRecordStore(); return available;
    }
    public static int erase(String name) throws Exception { RecordStore.deleteRecordStore(name); return 1; }
}
