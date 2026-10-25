public class IntegerChecks {
    private static int checks;
    private static void check(boolean ok) {
        checks++;
        if (!ok) throw new RuntimeException("Integer check " + checks + " failed");
    }
    private static void invalid(String text, int radix) {
        boolean caught = false;
        try { Integer.parseInt(text, radix); }
        catch (NumberFormatException e) { caught = true; }
        check(caught);
    }
    public static int run() {
        checks = 0;
        check(Integer.parseInt("2147483647") == Integer.MAX_VALUE);
        check(Integer.parseInt("-2147483648") == Integer.MIN_VALUE);
        check(Integer.parseInt("+123") == 123);
        check(Integer.parseInt("-0") == 0);
        check(Integer.parseInt("0000123") == 123);
        check(Integer.parseInt("7FFFFFFF", 16) == Integer.MAX_VALUE);
        check(Integer.parseInt("-80000000", 16) == Integer.MIN_VALUE);
        check(Integer.parseInt("10101", 2) == 21);
        check(Integer.parseInt("Z", 36) == 35);
        invalid(null, 10);
        invalid("", 10);
        invalid("-", 10);
        invalid("+", 10);
        invalid(" 12", 10);
        invalid("12 ", 10);
        invalid("1.2", 10);
        invalid("0x12", 16);
        invalid("2", 2);
        invalid("z", 35);
        invalid("1", 1);
        invalid("1", 37);
        invalid("2147483648", 10);
        invalid("-2147483649", 10);
        invalid("99999999999999999999999999", 10);
        invalid("80000000", 16);
        invalid("-80000001", 16);
        check(Integer.toString(0).equals("0"));
        check(Integer.toString(Integer.MIN_VALUE).equals("-2147483648"));
        check(Integer.toString(Integer.MAX_VALUE).equals("2147483647"));
        check(Integer.toString(-255, 16).equals("-ff"));
        check(Integer.toString(35, 36).equals("z"));
        check(Integer.toString(17, 1).equals("17"));
        check(Integer.toString(17, 37).equals("17"));
        check(Integer.toHexString(0).equals("0"));
        check(Integer.toHexString(-1).equals("ffffffff"));
        check(Integer.toHexString(Integer.MIN_VALUE).equals("80000000"));
        check(Integer.toOctalString(-1).equals("37777777777"));
        check(Integer.toOctalString(64).equals("100"));
        check(Integer.toBinaryString(-1).equals("11111111111111111111111111111111"));
        check(Integer.toBinaryString(10).equals("1010"));
        for (int radix = 2; radix <= 36; radix++) {
            check(Integer.parseInt(Integer.toString(Integer.MAX_VALUE, radix), radix) == Integer.MAX_VALUE);
            check(Integer.parseInt(Integer.toString(Integer.MIN_VALUE, radix), radix) == Integer.MIN_VALUE);
            check(Integer.parseInt(Integer.toString(-123456789, radix), radix) == -123456789);
        }
        Integer boxed = Integer.valueOf(0x12345678);
        check(boxed.intValue() == 0x12345678);
        check(boxed.byteValue() == 120);
        check(boxed.shortValue() == 22136);
        check(boxed.longValue() == 0x12345678L);
        check(new Integer(-123).floatValue() == -123.0f);
        check(new Integer(-123).doubleValue() == -123.0d);
        check(new Integer("42").intValue() == 42);
        check(Integer.valueOf("ff", 16).intValue() == 255);
        check(Integer.valueOf("-5").intValue() == -5);
        check(boxed.equals(new Integer(0x12345678)));
        check(!boxed.equals(new Integer(1)));
        check(!boxed.equals(null));
        check(!boxed.equals("305419896"));
        check(boxed.hashCode() == boxed.intValue());
        check(new Integer(-12).toString().equals("-12"));
        return checks;
    }
}
