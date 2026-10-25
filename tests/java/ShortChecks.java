import java.util.Hashtable;

public class ShortChecks {
    private static int checks;
    private static void check(boolean ok) {
        checks++;
        if (!ok) throw new RuntimeException("Short check " + checks + " failed");
    }
    public static int run() {
        checks = 0;
        short[] values = {-32768, -1, 0, 1, 7, 32767};
        Hashtable table = new Hashtable();
        for (int i = 0; i < values.length; i++) {
            Short first = new Short(values[i]);
            Short same = new Short(values[i]);
            check(first.equals(same) && same.equals(first));
            check(first.hashCode() == values[i] && same.hashCode() == first.hashCode());
            check(!first.equals(null) && !first.equals(new Integer(values[i])));
            check(!first.equals(new Short((short)(values[i] ^ 1))));
            table.put(first, new Integer(i));
            check(((Integer)table.get(same)).intValue() == i);
            check(((Integer)table.put(same, new Integer(i + 100))).intValue() == i);
            check(table.size() == i + 1);
        }
        for (int i = 0; i < values.length; i++) {
            check(table.containsKey(new Short(values[i])));
            check(((Integer)table.remove(new Short(values[i]))).intValue() == i + 100);
        }
        check(table.size() == 0);
        return checks;
    }
}
