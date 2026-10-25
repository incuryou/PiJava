public class LiteralIdentityChecks {
    public static String savedLiteral, savedDynamic;
    private static int checks;
    private static void check(boolean value) {
        ++checks;
        if (!value) throw new RuntimeException("literal identity check " + checks);
    }
    public static int run() {
        check("r1-literal" == LiteralIdentityOther.value());
        check("" == LiteralIdentityOther.empty());
        check("中文\u0000é" == LiteralIdentityOther.unicode());
        check("Aa" != "BB");
        String dynamic = new String("r1-literal");
        check(dynamic != "r1-literal" && dynamic.equals("r1-literal"));
        int attempts = 0;
        String marker;
        do { marker = LiteralIdentityOther.empty(); ++attempts; }
        while (marker != "" && attempts < 32);
        check(attempts == 1);
        String[] growth = {"s00", "s01", "s02", "s03", "s04", "s05", "s06", "s07",
            "s08", "s09", "s10", "s11", "s12", "s13", "s14", "s15", "s16", "s17",
            "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27",
            "s28", "s29", "s30", "s31"};
        check(growth.length == 32 && growth[31] == "s31");
        return checks;
    }
    public static int setupSaved() {
        savedLiteral = "r1-saved";
        savedDynamic = new String("r1-saved");
        return verifySaved();
    }
    public static int verifySaved() {
        return savedLiteral == "r1-saved" && savedDynamic != savedLiteral && savedDynamic.equals(savedLiteral) ? 1 : 0;
    }
    public static int verifyLegacyValue() {
        return savedLiteral.equals("r1-saved") && savedDynamic != savedLiteral ? 1 : 0;
    }
    public static String unheld() { return "r1-unheld"; }
    public static String constant() { return "r1-constant"; }
    public static String literal() { return "r1-literal"; }
}
class LiteralIdentityOther {
    static String value() { return "r1-literal"; }
    static String empty() { return ""; }
    static String unicode() { return "中文\u0000é"; }
}
class LiteralIdentityConstant {
    static final String VALUE = "r1-constant";
}
