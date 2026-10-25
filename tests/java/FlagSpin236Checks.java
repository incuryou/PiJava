public final class FlagSpin236Checks implements Runnable {
    private static volatile boolean busy=true;
    private static volatile int observed=0;
    public void run(){observed=7;busy=false;}
    public static int runTest(){
        busy=true;observed=0;
        new Thread(new FlagSpin236Checks()).start();
        while(busy){}
        if(observed!=7)throw new RuntimeException("producer not observed");
        return 1;
    }
}
