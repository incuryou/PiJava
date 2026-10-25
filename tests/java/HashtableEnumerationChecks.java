import java.util.Hashtable;
import java.util.Enumeration;
import java.util.NoSuchElementException;

public class HashtableEnumerationChecks {
    static Hashtable table;
    static Enumeration savedKeys, savedValues;
    static int checks;
    static void check(boolean ok) {
        ++checks;
        if (!ok) throw new RuntimeException("hashtable enumeration " + checks);
    }
    // Same consumer sequence as the game's z.f(): parse every key and remove
    // transient IDs from two tables during enumeration. No commercial payload.
    public static int cleanupProbe() {
        Hashtable a = new Hashtable(1), b = new Hashtable(1);
        String[] ids = {"200", "201", "1", "202"};
        for (int i = 0; i < ids.length; ++i) { a.put(ids[i], ids[i]); b.put(ids[i], ids[i]); }
        Enumeration keys = a.keys();
        try {
            while (keys.hasMoreElements()) {
                String key = (String) keys.nextElement();
                if (Integer.parseInt(key) >= 200) { a.remove(key); b.remove(key); }
            }
        } catch (NumberFormatException error) { return 17; }
        return a.size() == 1 && b.size() == 1 && a.containsKey("1") && b.containsKey("1") ? 1 : 18;
    }
    public static int run() {
        check(cleanupProbe() == 1);
        Hashtable h = new Hashtable(1);
        Object v0 = new Object(), v1 = new Object();
        h.put("0", v0); h.put("1", v1); h.put("2", v0);
        Enumeration keys = h.keys(), values = h.elements();
        h.remove("1"); h.remove("0"); h.clear();
        for (int i = 0; i < 40; ++i) h.put(Integer.toString(i + 10), new Object());
        int mask = 0, count = 0, zeros = 0, ones = 0;
        while (keys.hasMoreElements()) {
            Object key = keys.nextElement(); check(key != null);
            int bit = 1 << Integer.parseInt((String) key);
            check((mask & bit) == 0); mask |= bit; ++count;
        }
        check(mask == 7 && count == 3);
        while (values.hasMoreElements()) {
            Object value = values.nextElement();
            check(value == v0 || value == v1);
            if (value == v0) ++zeros; else ++ones;
        }
        check(zeros == 2 && ones == 1);
        try { keys.nextElement(); check(false); }
        catch (NoSuchElementException expected) { check(true); }
        try { values.nextElement(); check(false); }
        catch (NoSuchElementException expected) { check(true); }
        h.clear(); Enumeration empty = h.keys(); h.put("new", v0);
        check(!empty.hasMoreElements());
        try { empty.nextElement(); check(false); }
        catch (NoSuchElementException expected) { check(true); }
        Enumeration beforeReplace = h.elements(); h.put("new", v1);
        check(beforeReplace.nextElement() == v0 && !beforeReplace.hasMoreElements());
        check(h.get("new") == v1);
        try { h.put(null, v0); check(false); }
        catch (NullPointerException expected) { check(true); }
        try { Integer.parseInt((String) null); check(false); }
        catch (NumberFormatException expected) { check(true); }
        try { Integer.parseInt(""); check(false); }
        catch (NumberFormatException expected) { check(true); }
        return checks;
    }
    public static String setupHeld() {
        table = new Hashtable(1);
        String key = new String("enumeration-only-root");
        table.put(key, new Object()); savedKeys = table.keys();
        table.clear(); table = null;
        return key;
    }
    public static String held() { return (String) savedKeys.nextElement(); }
    public static void dropHeld() { savedKeys = null; }
    public static int setupSaved() {
        table = new Hashtable(1);
        table.put("200", new String("saved-a")); table.put("1", new String("saved-b"));
        savedKeys = table.keys(); savedValues = table.elements();
        check(savedKeys.nextElement().equals("200"));
        check(savedValues.nextElement().equals("saved-a"));
        table.clear();
        return 1;
    }
    public static int verifySaved() {
        check(table.size() == 0);
        check(savedKeys.hasMoreElements() && savedKeys.nextElement().equals("1"));
        check(savedValues.hasMoreElements() && savedValues.nextElement().equals("saved-b"));
        check(!savedKeys.hasMoreElements() && !savedValues.hasMoreElements());
        return 1;
    }
    public static int setupLegacy() {
        table = new Hashtable(1); table.put("200", "a"); table.put("201", "b"); table.put("1", "c");
        savedKeys = null; savedValues = null; return 1;
    }
    public static int verifyLegacy() {
        Enumeration keys = table.keys();
        while (keys.hasMoreElements()) {
            String key = (String) keys.nextElement();
            if (Integer.parseInt(key) >= 200) table.remove(key);
        }
        return table.size() == 1 && table.containsKey("1") ? 1 : 0;
    }
}
