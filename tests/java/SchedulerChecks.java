import javax.microedition.lcdui.*;
public class SchedulerChecks extends Thread {
    static int steps;
    static class TestCanvas extends Canvas { protected void paint(Graphics g) { } }
    public void run() {
        new TestCanvas().repaint();
        steps = 1;
        try { Thread.sleep(60); } catch(InterruptedException e) { }
        steps = 2;
    }
    public static int runChecks() {
        try {Thread.sleep(-1); return 0;}catch(IllegalArgumentException ok) { }
        catch(InterruptedException wrong) {return 0;}
        return 1;
    }
}
