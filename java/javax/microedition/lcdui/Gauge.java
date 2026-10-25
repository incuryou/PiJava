package javax.microedition.lcdui;
public class Gauge extends Item {
    public static final int INDEFINITE = -1, CONTINUOUS_IDLE = 0, INCREMENTAL_IDLE = 1, CONTINUOUS_RUNNING = 2, INCREMENTAL_UPDATING = 3;
    int value;
    int max;
    private boolean interactive;
    public Gauge(String label, boolean interactive, int maxValue, int initialValue) {
        this.label = label; this.interactive = interactive; checkMax(maxValue); max = maxValue; setValue(initialValue);
    }
    private void checkMax(int value) { if (value <= 0 && (value != INDEFINITE || interactive)) throw new IllegalArgumentException(); }
    public boolean isInteractive() { return interactive; }
    public int getValue() { return value; }
    public void setValue(int value) {
        if (max == INDEFINITE) { if (value < CONTINUOUS_IDLE || value > INCREMENTAL_UPDATING) throw new IllegalArgumentException(); this.value = value; }
        else this.value = value < 0 ? 0 : value > max ? max : value;
        changed();
    }
    public int getMaxValue() { return max; }
    public void setMaxValue(int value) {
        checkMax(value); if (value == max) return; int old = max; max = value;
        if (value == INDEFINITE) this.value = CONTINUOUS_IDLE;
        else if (old == INDEFINITE) this.value = 0;
        else if (this.value > value) this.value = value;
        changed();
    }
    public String labelText() { return (label == null ? "GAUGE" : label) + " " + value + (max == INDEFINITE ? "" : "/" + max); }
}
