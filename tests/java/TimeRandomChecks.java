import java.util.Calendar;
import java.util.Date;
import java.util.Random;
import java.util.TimeZone;

/** Self-contained CLDC regression checks. Returns the number of assertions. */
public class TimeRandomChecks {
    private static int assertions;
    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }
    private static Calendar gmt(long millis) {
        Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
        calendar.setTimeInMillis(millis);
        return calendar;
    }
    private static void date(Calendar calendar, int year, int month, int day) {
        check(calendar.get(Calendar.YEAR) == year, "calendar year");
        check(calendar.get(Calendar.MONTH) == month, "calendar month");
        check(calendar.get(Calendar.DATE) == day, "calendar day");
    }
    private static void roundTrip(long millis, TimeZone zone) {
        Calendar calendar = Calendar.getInstance(zone);
        calendar.setTimeInMillis(millis);
        int millisecond = calendar.get(Calendar.MILLISECOND);
        calendar.set(Calendar.MILLISECOND, millisecond);
        check(calendar.getTimeInMillis() == millis, "calendar epoch round trip");
    }
    public static int run() {
        assertions = 0;
        Calendar calendar = gmt(0);
        date(calendar, 1970, Calendar.JANUARY, 1);
        check(calendar.get(Calendar.DAY_OF_WEEK) == Calendar.THURSDAY, "epoch weekday");
        calendar.setTimeInMillis(-1);
        date(calendar, 1969, Calendar.DECEMBER, 31);
        check(calendar.get(Calendar.HOUR_OF_DAY) == 23, "negative epoch hour");
        check(calendar.get(Calendar.MINUTE) == 59, "negative epoch minute");
        check(calendar.get(Calendar.SECOND) == 59, "negative epoch second");
        check(calendar.get(Calendar.MILLISECOND) == 999, "negative epoch millisecond");
        check(calendar.get(Calendar.HOUR) == 11 && calendar.get(Calendar.AM_PM) == Calendar.PM,
                "twelve hour fields");
        calendar.setTimeInMillis(-58060800000L);
        date(calendar, 1968, Calendar.FEBRUARY, 29);
        calendar.setTimeInMillis(951782400000L);
        date(calendar, 2000, Calendar.FEBRUARY, 29);
        calendar.setTimeInMillis(4107542400000L);
        date(calendar, 2100, Calendar.MARCH, 1);
        calendar.setTimeInMillis(13574563200000L);
        date(calendar, 2400, Calendar.FEBRUARY, 29);
        calendar.setTimeInMillis(-2203891200000L);
        date(calendar, 1900, Calendar.MARCH, 1);

        // Consecutive setters must preserve February 29 until the final read.
        calendar.setTimeInMillis(2592000000L); // January 31, 1970
        calendar.set(Calendar.YEAR, 2000);
        calendar.set(Calendar.MONTH, Calendar.FEBRUARY);
        calendar.set(Calendar.DATE, 29);
        check(calendar.getTimeInMillis() == 951782400000L, "lazy consecutive setters");
        calendar.set(Calendar.YEAR, 2100);
        date(calendar, 2100, Calendar.MARCH, 1);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.MONTH, -1);
        date(calendar, 1969, Calendar.DECEMBER, 1);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.MONTH, 12);
        date(calendar, 1971, Calendar.JANUARY, 1);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.DATE, 0);
        date(calendar, 1969, Calendar.DECEMBER, 31);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.MILLISECOND, -1);
        check(calendar.getTimeInMillis() == -1, "negative field normalization");
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.HOUR_OF_DAY, 24);
        date(calendar, 1970, Calendar.JANUARY, 2);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.HOUR, 11);
        calendar.set(Calendar.AM_PM, Calendar.PM);
        check(calendar.get(Calendar.HOUR_OF_DAY) == 23, "AM PM setters");
        calendar.set(Calendar.HOUR_OF_DAY, 6);
        check(calendar.get(Calendar.HOUR) == 6 && calendar.get(Calendar.AM_PM) == Calendar.AM,
                "hour setter precedence");
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.DAY_OF_WEEK, Calendar.SUNDAY);
        date(calendar, 1969, Calendar.DECEMBER, 28);
        calendar.set(Calendar.DATE, 31);
        check(calendar.get(Calendar.DAY_OF_WEEK) == Calendar.WEDNESDAY,
                "date setter precedence");
        boolean rejected = false;
        try { calendar.get(0); } catch (ArrayIndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "invalid calendar get field");
        rejected = false;
        try { calendar.set(15, 1); } catch (ArrayIndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "invalid calendar set field");
        calendar.setTimeInMillis(0);
        Date copy = calendar.getTime();
        copy.setTime(1000);
        check(calendar.getTimeInMillis() == 0, "calendar Date defensive result");
        calendar.setTime(copy);
        copy.setTime(2000);
        check(calendar.getTimeInMillis() == 1000, "calendar Date defensive input");
        check(calendar.after(gmt(0)) && calendar.before(gmt(2000)), "calendar comparisons");
        check(!calendar.after(null) && !calendar.before("bad"), "calendar comparison types");
        check(calendar.equals(gmt(1000)) && calendar.hashCode() == gmt(1000).hashCode(),
                "calendar equality hash");

        TimeZone east = TimeZone.getTimeZone("Asia/Shanghai");
        check(east.getRawOffset() == 28800000 && !east.useDaylightTime(), "Shanghai fixed offset");
        check(east.getOffset(1, 2000, Calendar.FEBRUARY, 29, Calendar.TUESDAY, 0) == 28800000,
                "zone offset method");
        TimeZone half = TimeZone.getTimeZone("GMT-05:30");
        check(half.getRawOffset() == -19800000 && half.getID().equals("GMT-05:30"), "custom negative zone");
        check(TimeZone.getTimeZone("GMT+05:45").getRawOffset() == 20700000, "custom quarter hour zone");
        check(TimeZone.getTimeZone("UTC").getID().equals("UTC"), "UTC identity");
        check(TimeZone.getDefault().getID().equals("GMT"), "default zone fallback");
        check(TimeZone.getTimeZone("unknown").getID().equals("GMT"), "unknown zone fallback");
        check(TimeZone.getTimeZone("GMT+24:00").getID().equals("GMT"), "invalid hour zone fallback");
        check(TimeZone.getTimeZone("GMT+01:60").getID().equals("GMT"), "invalid minute zone fallback");
        check(TimeZone.getTimeZone("GMT+aa:bb").getID().equals("GMT"), "invalid text zone fallback");
        String[] ids = TimeZone.getAvailableIDs();
        check(ids.length == 3 && ids[0].equals("GMT"), "available time zones");
        ids[0] = "changed";
        check(TimeZone.getAvailableIDs()[0].equals("GMT"), "zone ID defensive array");
        rejected = false;
        try { east.getOffset(1, 2100, Calendar.FEBRUARY, 29, Calendar.MONDAY, 0); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "invalid offset date");
        calendar.setTimeInMillis(0);
        calendar.setTimeZone(east);
        check(calendar.get(Calendar.HOUR_OF_DAY) == 8 && calendar.getTimeInMillis() == 0,
                "zone change preserves instant");
        calendar.set(Calendar.HOUR_OF_DAY, 0);
        check(calendar.getTimeInMillis() == -28800000L, "zone field conversion");
        calendar.setTimeInMillis(0);
        calendar.setTimeZone(half);
        date(calendar, 1969, Calendar.DECEMBER, 31);
        check(calendar.get(Calendar.HOUR_OF_DAY) == 18 && calendar.get(Calendar.MINUTE) == 30,
                "zone previous date");
        calendar.setTimeInMillis(0);
        calendar.setTimeZone(new OffsetZone());
        check(calendar.get(Calendar.HOUR_OF_DAY) == 1, "custom offset computeFields");
        calendar.set(Calendar.HOUR_OF_DAY, 2);
        check(calendar.getTimeInMillis() == 3600000L, "custom offset computeTime");
        roundTrip(-9223372036854775807L - 1, TimeZone.getTimeZone("GMT"));
        roundTrip(9223372036854775807L, TimeZone.getTimeZone("GMT"));
        roundTrip(-9223372036854775807L - 1, east);
        roundTrip(9223372036854775807L, half);
        calendar.setTimeInMillis(0);
        calendar.set(Calendar.YEAR, 2147483647);
        rejected = false;
        try { calendar.getTimeInMillis(); }
        catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "date millisecond overflow rejection");
        HookCalendar hooks = new HookCalendar();
        hooks.setTimeInMillis(0);
        check(hooks.get(Calendar.YEAR) == 1234, "subclass computeFields hook");
        hooks.set(Calendar.YEAR, 77);
        check(hooks.getTimeInMillis() == 77, "subclass computeTime hook");
        Date zero = new Date(0);
        check(zero.equals(new Date(0)) && !zero.equals(new Date(1)) && !zero.equals(null),
                "Date equality");
        check(new Date(0x123456789abcdef0L).hashCode() == (int) 0x88888888L, "Date hash halves");
        check(zero.toString().equals("Thu Jan 01 00:00:00 GMT 1970"), "Date formatted string");
        check(new Date(-1).toString().equals("Wed Dec 31 23:59:59 GMT 1969"), "Date negative string");

        Random random = new Random(0);
        int[] vectors = { -1155484576, -723955400, 1033096058, -1690734402, -1557280266 };
        for (int i = 0; i < vectors.length; i++) check(random.nextInt() == vectors[i], "Random nextInt vector");
        random.setSeed(0);
        check(random.nextInt() == vectors[0], "Random reseed");
        check(new Random(1L << 48).nextInt() == vectors[0], "Random 48 bit seed mask");
        check(new Random(1L << 32).nextInt() == -726420384, "Random high seed bits");
        check(new Random(0).nextLong() == -4962768465676381896L, "Random nextLong vector");
        check(new Random(0).nextFloat() == 12263604 / 16777216.0f, "Random nextFloat vector");
        check(new Random(0).nextDouble() == 6583972509698697L / 9007199254740992.0,
                "Random nextDouble vector");
        random.setSeed(0);
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        int[] expectedBytes = { 96, 180, 32, 187, 56, 81, 217, 212 };
        for (int i = 0; i < bytes.length; i++) check((bytes[i] & 255) == expectedBytes[i], "Random bytes vector");
        check(new Random(0).nextInt(8) == 5, "Random power of two bound");
        check(new Random(0).nextInt(10) == 0, "Random general bound");
        check(new Random(0).nextInt(1) == 0, "Random bound one");
        RejectRandom rejection = new RejectRandom();
        check(rejection.nextInt(1073741825) == 1234 && rejection.calls == 2, "Random rejection branch");
        rejected = false;
        try { random.nextInt(0); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "Random zero bound exception");
        rejected = false;
        try { random.nextInt(-7); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "Random negative bound exception");
        return assertions;
    }
    public static class RejectRandom extends Random {
        public int calls;
        protected int next(int bits) { calls++; return calls == 1 ? 2147483647 : 1234; }
    }
    public static class OffsetZone extends TimeZone {
        public int getOffset(int era, int year, int month, int day, int weekDay, int millis) { return 3600000; }
        public int getRawOffset() { return 0; }
        public boolean useDaylightTime() { return true; }
    }
    public static class HookCalendar extends Calendar {
        protected void computeFields() { fields[YEAR] = 1234; isSet[YEAR] = true; }
        protected void computeTime() { time = fields[YEAR]; }
    }
}
