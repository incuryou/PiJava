// 0.2.112 F1: java.lang.Class.isInstance / isAssignableFrom / isArray / isInterface (CLDC 1.1).
public class ClassReflectChecks {
    private static int checks;
    private static void check(boolean ok) {
        checks++;
        if (!ok) throw new RuntimeException("Class reflect check " + checks + " failed");
    }
    interface Shape {}
    interface Round extends Shape {}
    static class Base implements Runnable { public void run() {} }
    static class Derived extends Base implements Round {}
    public static int run() {
        checks = 0;
        Object derived = new Derived(), base = new Base(), text = "abc";
        // isInstance: null is never an instance
        check(!Object.class.isInstance(null));
        check(!String.class.isInstance(null));
        check(Object.class.isInstance(text));
        check(String.class.isInstance(text));
        check(!String.class.isInstance(base));
        check(Base.class.isInstance(derived));
        check(!Derived.class.isInstance(base));
        check(Runnable.class.isInstance(derived));
        check(Shape.class.isInstance(derived));
        check(!Shape.class.isInstance(base));
        // arrays
        int[] ints = new int[2]; short[] shorts = new short[1]; String[] strings = new String[1]; int[][] grid = new int[2][2];
        check(int[].class.isInstance(ints));
        check(!int[].class.isInstance(shorts));
        check(Object.class.isInstance(ints));
        check(Object[].class.isInstance(strings));
        check(!Object[].class.isInstance(ints));
        check(Object[].class.isInstance(grid));
        check(!String[].class.isInstance(new Object[1]));
        check(Base[].class.isInstance(new Derived[1]));
        // isAssignableFrom
        check(Object.class.isAssignableFrom(String.class));
        check(!String.class.isAssignableFrom(Object.class));
        check(Base.class.isAssignableFrom(Derived.class));
        check(Base.class.isAssignableFrom(Base.class));
        check(!Derived.class.isAssignableFrom(Base.class));
        check(Shape.class.isAssignableFrom(Round.class));
        check(Shape.class.isAssignableFrom(Derived.class));
        check(Object.class.isAssignableFrom(Shape.class));
        check(Object.class.isAssignableFrom(int[].class));
        check(Object[].class.isAssignableFrom(String[].class));
        check(!String[].class.isAssignableFrom(Object[].class));
        check(!Object[].class.isAssignableFrom(int[].class));
        check(!int[].class.isAssignableFrom(short[].class));
        check(int[].class.isAssignableFrom(ints.getClass()));
        boolean npe = false;
        try { Object.class.isAssignableFrom(null); } catch (NullPointerException e) { npe = true; }
        check(npe);
        // isArray / isInterface
        check(int[].class.isArray());
        check(strings.getClass().isArray());
        check(!String.class.isArray());
        check(Runnable.class.isInterface());
        check(Round.class.isInterface());
        check(!Base.class.isInterface());
        check(!Runnable[].class.isInterface());
        check(!int[].class.isInterface());
        check(!Object.class.isInterface());
        // the getClass() of an instance matches its literal class
        check(derived.getClass().isInstance(new Derived()));
        check(!derived.getClass().isInterface());
        return checks;
    }
}
