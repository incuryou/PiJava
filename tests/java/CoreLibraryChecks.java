public class CoreLibraryChecks {
    public static String savedLiteral, savedDynamic, savedIntern;
    public static String[] savedGrowth;
    private static int checks;

    private static void check(boolean ok, String label) {
        ++checks;
        if (!ok) throw new RuntimeException("core check " + checks + ": " + label);
    }

    private static String growthValue(int i) {
        return new String(new char[]{'c','1','1','4','g', (char)(i + 256)});
    }

    // Run once per fresh/reset VM: receiver-first assertions require an empty
    // pool for their content. Follow with GC/snapshot checks, not another run().
    public static int runStrings() {
        checks = 0;
        String literal = "core114-literal-first";
        String equal = new String(literal);
        check(equal != literal && equal.equals(literal), "copy is a distinct equal string");
        check(equal.intern() == literal, "literal first canonical identity");
        check(literal.intern() == literal, "canonical literal returns itself");

        // This content has not appeared in an ldc before the intern call.
        String first = new String(new char[]{'c','o','r','e','1','1','4','-','f','i','r','s','t'});
        check(first.intern() == first, "absent content publishes receiver");
        check(new String(first).intern() == first, "equal new receiver reuses first");
        check(first == "core114-first", "intern first agrees with later literal");

        String empty = "";
        check(new String(new char[0]).intern() == empty, "empty canonical identity");
        String unicode = "\u4e2d\u6587\u00e9\ud83d\ude00";
        check(new String(new char[]{'\u4e2d','\u6587','\u00e9','\ud83d','\ude00'}).intern() == unicode,
              "UTF16 supplementary canonical identity");
        String nul = "A\u0000B";
        check(new String(new char[]{'A',0,'B'}).intern() == nul, "embedded NUL preserved");
        check(nul.intern() != "A".intern() && nul.intern() != "A\u0000C".intern(),
              "NUL suffix distinguishes content");
        String fb = "FB", ea = "Ea";
        check(fb.hashCode() == 2236 && ea.hashCode() == 2236, "known hash collision fixture");
        check(new String(fb).intern() == fb && new String(ea).intern() == ea && fb != ea,
              "equal hashes keep distinct canonical content");
        check("Core114".intern() != "core114".intern(), "case remains distinct");

        String[] growth = new String[96];
        for (int i = 0; i < growth.length; ++i) {
            String receiver = growthValue(i);
            growth[i] = receiver.intern();
            check(growth[i] == receiver, "growth publishes each new receiver");
        }
        System.gc();
        for (int i = 0; i < growth.length; ++i)
            check(growthValue(i).intern() == growth[i], "growth identity after GC request");
        check(new String(literal).intern() == literal && new String(first).intern() == first,
              "early entries survive growth and GC request");
        return 114251;
    }

    // The host keeps only a raw handle and runs gc_collect. The canonical value
    // must then survive through StringLiterals.roots, while makeVictim must die.
    public static String makePoolOnly() {
        return new String(new char[]{'p','o','o','l','1','1','4','o','n','l','y'}).intern();
    }

    public static String makeVictim() {
        return new String(new char[]{'v','i','c','t','i','m','1','1','4'});
    }

    public static String internArgument(String value) {
        return value.intern();
    }

    private static void intValue(double value, int want, String label) {
        check(new Double(value).intValue() == want, label);
    }

    public static int runDoubles() {
        checks = 0;
        intValue(Double.longBitsToDouble(0x7ff8000000000000L), 0, "NaN converts to zero");
        intValue(Double.longBitsToDouble(0xfff8000000000001L), 0, "negative NaN converts to zero");
        intValue(Double.longBitsToDouble(0x7ff0000000000000L), 2147483647, "positive infinity saturates");
        intValue(Double.longBitsToDouble(0xfff0000000000000L), -2147483648, "negative infinity saturates");
        intValue(0.0, 0, "positive zero");
        intValue(Double.longBitsToDouble(0x8000000000000000L), 0, "negative zero");
        intValue(1.9, 1, "positive fraction truncates");
        intValue(-1.9, -1, "negative fraction truncates");
        intValue(0.999999999, 0, "positive subunit truncates");
        intValue(-0.999999999, 0, "negative subunit truncates");
        intValue(Double.longBitsToDouble(1L), 0, "smallest positive subnormal");
        intValue(Double.longBitsToDouble(0x8000000000000001L), 0, "smallest negative subnormal");
        intValue(2147483646.75, 2147483646, "below positive integer boundary");
        intValue(2147483647.0, 2147483647, "maximum integer exactly");
        intValue(2147483647.75, 2147483647, "above maximum fraction saturates");
        intValue(2147483648.0, 2147483647, "positive overflow saturates");
        intValue(-2147483647.75, -2147483647, "above minimum truncates toward zero");
        intValue(-2147483648.0, -2147483648, "minimum integer exactly");
        intValue(-2147483648.75, -2147483648, "below minimum fraction saturates");
        intValue(-2147483649.0, -2147483648, "negative overflow saturates");
        intValue(1.0e300, 2147483647, "large positive finite saturates");
        intValue(-1.0e300, -2147483648, "large negative finite saturates");
        return 114251;
    }

    public static int setupSaved() {
        savedLiteral = "core114-saved-literal";
        savedDynamic = new String(savedLiteral);
        savedIntern = new String(new char[]{'c','o','r','e','1','1','4','-','s','a','v','e','d'}).intern();
        savedGrowth = new String[80];
        for (int i = 0; i < savedGrowth.length; ++i)
            savedGrowth[i] = new String(new char[]{'s','1','1','4', (char)(i + 512)}).intern();
        return verifySaved();
    }

    public static int mutateSaved() {
        savedLiteral = "core114-mutated";
        savedDynamic = null;
        savedIntern = null;
        savedGrowth = null;
        System.gc();
        return 114252;
    }

    public static int verifySaved() {
        check(savedLiteral == "core114-saved-literal", "saved literal identity");
        check(savedDynamic != savedLiteral && savedDynamic.equals(savedLiteral), "saved distinct copy");
        check(savedDynamic.intern() == savedLiteral, "saved copy reuses literal");
        check(savedIntern == "core114-saved" && new String(savedIntern).intern() == savedIntern,
              "saved receiver first canonical identity");
        check(savedGrowth != null && savedGrowth.length == 80, "saved growth length");
        for (int i = 0; i < savedGrowth.length; ++i)
            check(new String(new char[]{'s','1','1','4',(char)(i + 512)}).intern() == savedGrowth[i],
                  "saved growth canonical identity");
        return 114251;
    }

    public static int verifyAfterGc() {
        System.gc();
        return verifySaved();
    }

    public static int run() {
        runStrings();
        runDoubles();
        return 114251;
    }
}
