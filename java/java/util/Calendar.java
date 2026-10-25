package java.util;

/**
 * CLDC calendar base. The supplied implementation uses the proleptic Gregorian
 * calendar (Gregorian leap rules at every date), without a historical cutover.
 * Field values are normalized leniently when a read requests the instant.
 * Public millisecond accessors are retained as a compatibility extension.
 */
public abstract class Calendar {
    public static final int YEAR = 1;
    public static final int MONTH = 2;
    public static final int DATE = 5;
    public static final int DAY_OF_MONTH = DATE;
    public static final int DAY_OF_WEEK = 7;
    public static final int AM_PM = 9;
    public static final int HOUR = 10;
    public static final int HOUR_OF_DAY = 11;
    public static final int MINUTE = 12;
    public static final int SECOND = 13;
    public static final int MILLISECOND = 14;
    public static final int SUNDAY = 1, MONDAY = 2, TUESDAY = 3,
            WEDNESDAY = 4, THURSDAY = 5, FRIDAY = 6, SATURDAY = 7;
    public static final int JANUARY = 0, FEBRUARY = 1, MARCH = 2,
            APRIL = 3, MAY = 4, JUNE = 5, JULY = 6, AUGUST = 7,
            SEPTEMBER = 8, OCTOBER = 9, NOVEMBER = 10, DECEMBER = 11;
    public static final int AM = 0, PM = 1;

    protected int[] fields;
    protected boolean[] isSet;
    protected long time;
    private TimeZone zone;
    private boolean timeValid;
    private boolean fieldsValid;

    protected Calendar() {
        fields = new int[15];
        isSet = new boolean[15];
        zone = TimeZone.getDefault();
        time = System.currentTimeMillis();
        timeValid = true;
    }

    public static Calendar getInstance() {
        return getInstance(TimeZone.getDefault());
    }

    public static Calendar getInstance(TimeZone zone) {
        Calendar calendar = new GregorianCalendarImpl();
        calendar.setTimeZone(zone);
        return calendar;
    }

    public final Date getTime() { return new Date(getTimeInMillis()); }
    public final void setTime(Date date) { setTimeInMillis(date.getTime()); }

    public long getTimeInMillis() {
        if (!timeValid) {
            computeTime();
            timeValid = true;
            fieldsValid = false;
        }
        return time;
    }

    public void setTimeInMillis(long millis) {
        time = millis;
        timeValid = true;
        fieldsValid = false;
        for (int i = 0; i < isSet.length; i++) isSet[i] = false;
    }

    public final int get(int field) {
        checkField(field);
        complete();
        return fields[field];
    }

    public final void set(int field, int value) {
        checkField(field);
        // Preserve pending setters until a read requests normalization.
        if (timeValid && !fieldsValid) complete();
        fields[field] = value;
        isSet[field] = true;
        if (field == HOUR_OF_DAY) {
            isSet[HOUR] = false;
            isSet[AM_PM] = false;
        } else if (field == HOUR || field == AM_PM) {
            isSet[HOUR_OF_DAY] = false;
        } else if (field == DAY_OF_MONTH) {
            isSet[DAY_OF_WEEK] = false;
        } else if (field == DAY_OF_WEEK) {
            isSet[DAY_OF_MONTH] = false;
        }
        timeValid = false;
    }

    private void complete() {
        getTimeInMillis();
        if (!fieldsValid) {
            computeFields();
            fieldsValid = true;
        }
    }

    private static void checkField(int field) {
        if (field != YEAR && field != MONTH && field != DAY_OF_MONTH
                && field != DAY_OF_WEEK && field != AM_PM && field != HOUR
                && field != HOUR_OF_DAY && field != MINUTE && field != SECOND
                && field != MILLISECOND) {
            throw new ArrayIndexOutOfBoundsException("calendar field");
        }
    }

    public boolean equals(Object other) {
        if (!(other instanceof Calendar)) return false;
        Calendar that = (Calendar) other;
        return getTimeInMillis() == that.getTimeInMillis()
                && zone.getID().equals(that.zone.getID())
                && zone.getRawOffset() == that.zone.getRawOffset()
                && zone.useDaylightTime() == that.zone.useDaylightTime();
    }

    public int hashCode() {
        long millis = getTimeInMillis();
        return (int) (millis ^ (millis >>> 32)) ^ zone.getID().hashCode()
                ^ zone.getRawOffset() ^ (zone.useDaylightTime() ? 1 : 0);
    }

    public boolean before(Object when) {
        return when instanceof Calendar
                && getTimeInMillis() < ((Calendar) when).getTimeInMillis();
    }

    public boolean after(Object when) {
        return when instanceof Calendar
                && getTimeInMillis() > ((Calendar) when).getTimeInMillis();
    }

    public void setTimeZone(TimeZone value) {
        if (value == null) throw new NullPointerException("time zone");
        // A time-zone change preserves the instant, including pending setters.
        getTimeInMillis();
        zone = value;
        fieldsValid = false;
    }

    public TimeZone getTimeZone() { return zone; }
    protected abstract void computeFields();
    protected abstract void computeTime();
}

/** Package-private Gregorian implementation; all arithmetic is independent of the host. */
final class GregorianCalendarImpl extends Calendar {
    private static final long DAY_MILLIS = 86400000L;

    protected void computeFields() {
        long day = GregorianMath.floorDiv(time, DAY_MILLIS);
        long millis = GregorianMath.floorMod(time, DAY_MILLIS);
        TimeZone zone = getTimeZone();
        // getOffset's arguments describe standard local time, not UTC.
        long standardMillis = millis + zone.getRawOffset();
        long standardDay = day + GregorianMath.floorDiv(standardMillis, DAY_MILLIS);
        int[] date = GregorianMath.dateFromDays(standardDay);
        int year = date[0];
        int offset = zone.getOffset(year <= 0 ? 0 : 1, year <= 0 ? 1 - year : year,
                date[1], date[2], GregorianMath.dayOfWeek(standardDay),
                (int) GregorianMath.floorMod(standardMillis, DAY_MILLIS));
        millis += offset;
        day += GregorianMath.floorDiv(millis, DAY_MILLIS);
        millis = GregorianMath.floorMod(millis, DAY_MILLIS);
        date = GregorianMath.dateFromDays(day);
        fields[YEAR] = date[0];
        fields[MONTH] = date[1];
        fields[DAY_OF_MONTH] = date[2];
        fields[DAY_OF_WEEK] = GregorianMath.dayOfWeek(day);
        int hour = (int) (millis / 3600000L);
        fields[HOUR_OF_DAY] = hour;
        fields[HOUR] = hour % 12;
        fields[AM_PM] = hour / 12;
        fields[MINUTE] = (int) ((millis / 60000L) % 60);
        fields[SECOND] = (int) ((millis / 1000L) % 60);
        fields[MILLISECOND] = (int) (millis % 1000L);
        for (int i = 0; i < isSet.length; i++) isSet[i] = true;
    }

    protected void computeTime() {
        long year = isSet[YEAR] ? fields[YEAR] : 1970;
        long month = isSet[MONTH] ? fields[MONTH] : JANUARY;
        year += GregorianMath.floorDiv(month, 12);
        month = GregorianMath.floorMod(month, 12);
        long day = GregorianMath.daysFromDate(year, (int) month, 1)
                + (isSet[DAY_OF_MONTH] || isSet[DAY_OF_WEEK] ? fields[DAY_OF_MONTH] : 1) - 1;
        if (isSet[DAY_OF_WEEK] && !isSet[DAY_OF_MONTH]) {
            day += (long) fields[DAY_OF_WEEK] - GregorianMath.dayOfWeek(day);
        }
        long hour = fields[HOUR_OF_DAY];
        if (!isSet[HOUR_OF_DAY]) {
            hour = (long) fields[HOUR] + (long) fields[AM_PM] * 12;
        }
        long millis = hour * 3600000L + (long) fields[MINUTE] * 60000L
                + (long) fields[SECOND] * 1000L + fields[MILLISECOND];
        day += GregorianMath.floorDiv(millis, DAY_MILLIS);
        millis = GregorianMath.floorMod(millis, DAY_MILLIS);
        int[] date = GregorianMath.dateFromDays(day);
        int actualYear = date[0];
        int offset = getTimeZone().getOffset(actualYear <= 0 ? 0 : 1,
                actualYear <= 0 ? 1 - actualYear : actualYear,
                date[1], date[2], GregorianMath.dayOfWeek(day), (int) millis);
        millis -= offset;
        day += GregorianMath.floorDiv(millis, DAY_MILLIS);
        millis = GregorianMath.floorMod(millis, DAY_MILLIS);
        // Range checks also avoid overflowing the partial first/last epoch day.
        long minDay = -106751991168L;
        long maxDay = 106751991167L;
        if (day < minDay || day > maxDay
                || (day == minDay && millis < 60424192L)
                || (day == maxDay && millis > 25975807L)) {
            throw new IllegalArgumentException("date outside millisecond range");
        }
        time = day < 0 ? (day + 1) * DAY_MILLIS + (millis - DAY_MILLIS)
                : day * DAY_MILLIS + millis;
    }
}

/** Gregorian dates with astronomical year numbering: year 0 precedes year 1. */
final class GregorianMath {
    static long floorDiv(long value, long divisor) {
        long quotient = value / divisor;
        return value % divisor < 0 ? quotient - 1 : quotient;
    }
    static long floorMod(long value, long divisor) {
        long remainder = value % divisor;
        return remainder < 0 ? remainder + divisor : remainder;
    }
    static int dayOfWeek(long day) { return (int) floorMod(day + 4, 7) + 1; }
    static boolean leap(long year) {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }
    static int monthDays(long year, int month) {
        if (month == 1) return leap(year) ? 29 : 28;
        return month == 3 || month == 5 || month == 8 || month == 10 ? 30 : 31;
    }
    static long daysFromDate(long year, int month, int day) {
        year -= month <= 1 ? 1 : 0;
        long era = floorDiv(year, 400);
        long yoe = year - era * 400;
        int marchMonth = month + (month > 1 ? -2 : 10);
        long doy = (153L * marchMonth + 2) / 5 + day - 1;
        long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }
    static int[] dateFromDays(long day) {
        long z = day + 719468;
        long era = floorDiv(z, 146097);
        long doe = z - era * 146097;
        long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        long year = yoe + era * 400;
        long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long mp = (5 * doy + 2) / 153;
        int date = (int) (doy - (153 * mp + 2) / 5 + 1);
        int month = (int) (mp + (mp < 10 ? 2 : -10));
        year += month <= 1 ? 1 : 0;
        if (year < -2147483648L || year > 2147483647L)
            throw new IllegalArgumentException("calendar year range");
        return new int[] { (int) year, month, date };
    }
}
