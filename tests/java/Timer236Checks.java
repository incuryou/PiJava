import java.util.Timer;
import java.util.TimerTask;
import java.util.Date;
public class Timer236Checks {
    static int checks,order;
    static void check(boolean b,String why){checks++;if(!b)throw new RuntimeException(why);}
    static class Task extends TimerTask {
        volatile int calls;
        Thread worker;
        int expectedOrder;
        Timer owner;
        long stamp;
        public void run(){
            calls++;worker=Thread.currentThread();stamp=scheduledExecutionTime();
            if(expectedOrder!=0)check(++order==expectedOrder,"time queue order");
            if(owner!=null){owner.cancel();owner=null;}
        }
    }
    static void await(Task t,int n)throws Exception {
        long end=System.currentTimeMillis()+3000;
        while(t.calls<n&&System.currentTimeMillis()<end)Thread.sleep(1);
        check(t.calls==n,"scheduled call completed");
    }
    public static int run()throws Exception {
        checks=0;order=0;Timer timer=new Timer();
        try {
            Task never=new Task();check(!never.cancel()&&!never.cancel(),"unscheduled cancel false");
            boolean bad=false;try{timer.schedule(never,0);}catch(IllegalStateException e){bad=true;}check(bad,"cancelled task cannot schedule");
            bad=false;try{timer.schedule(new Task(),-1);}catch(IllegalArgumentException e){bad=true;}check(bad,"negative delay");
            bad=false;try{timer.schedule(new Task(),0,0);}catch(IllegalArgumentException e){bad=true;}check(bad,"nonpositive period");
            bad=false;try{timer.schedule(new Task(),new Date(-1));}catch(IllegalArgumentException e){bad=true;}check(bad,"negative date");
            bad=false;try{timer.schedule(null,1);}catch(NullPointerException e){bad=true;}check(bad,"null task");
            Task delayed=new Task();timer.schedule(delayed,2000);check(delayed.cancel()&&!delayed.cancel(),"scheduled cancel true once");check(timer.purge()==1&&timer.purge()==0,"purge exact count");
            Task a=new Task(),b=new Task(),c=new Task();a.expectedOrder=3;b.expectedOrder=1;c.expectedOrder=2;
            long at=System.currentTimeMillis()+100;
            timer.schedule(a,new Date(at+40));timer.schedule(b,new Date(at));timer.schedule(c,new Date(at+20));
            await(a,1);check(a.worker==b.worker&&b.worker==c.worker,"one worker per Timer");check(b.stamp==at,"Date scheduledExecutionTime");check(!a.cancel(),"executed one shot cancel false");
            bad=false;try{timer.schedule(c,0);}catch(IllegalStateException e){bad=true;}check(bad,"task not reusable");
        } finally {timer.cancel();}
        boolean bad=false;try{timer.schedule(new Task(),1);}catch(IllegalStateException e){bad=true;}check(bad,"cancelled timer not reusable");timer.cancel();
        Timer self=new Timer();Task closing=new Task();closing.owner=self;self.schedule(closing,0,10);await(closing,1);Thread.sleep(50);check(closing.calls==1,"Timer.cancel in task prevents repeat and null-owner dereference");
        final Timer periodic=new Timer();final long[] stamps=new long[3];
        final Task repeating=new Task(){public void run(){stamps[calls]=scheduledExecutionTime();calls++;if(calls==3)cancel();}};
        try{periodic.scheduleAtFixedRate(repeating,20,20);await(repeating,3);Thread.sleep(30);check(repeating.calls==3,"task cancel in run");check(stamps[1]-stamps[0]==20&&stamps[2]-stamps[1]==20,"fixed rate deadlines");}finally{periodic.cancel();}
        // Previously each task had its own worker and could overlap while one slept.
        final Timer serialized=new Timer();final int[] active=new int[1];
        Task slow=new Task(){public void run(){active[0]++;try{Thread.sleep(25);}catch(Exception e){}active[0]--;calls++;}};
        Task later=new Task(){public void run(){check(active[0]==0,"timer tasks execute sequentially");calls++;}};
        try{serialized.schedule(slow,5);serialized.schedule(later,10);await(later,1);}finally{serialized.cancel();}
        return checks;
    }
}
