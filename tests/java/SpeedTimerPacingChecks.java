import java.util.Timer;
import java.util.TimerTask;
// 0.2.113: fixture for SpeedTimerPacing (java.util.Timer queue waits in 60 fps mode).
public class SpeedTimerPacingChecks {
    static final class Tick extends TimerTask { public void run() {} }
    public static Timer timer;
    public static Tick task;
    public static Object plain = new Object();
    public static void setup(int periodMs) {
        timer = new Timer();
        task = new Tick();
        timer.schedule(task, 1, periodMs);   // fixed delay, like the game (schedule(task, 1, 41))
    }
}
