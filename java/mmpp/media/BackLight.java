package mmpp.media;

/** Backlight compatibility without host color-backlight capability. */
public class BackLight {
    public static int getColor() { return 0; }
    public static int numColors() { return 0; }
    public static void off() {}
    public static void on(int timeout) {}
    public static void setColor(int RGB) {}
}
