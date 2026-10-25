public class ArrayGetterChecks {
    static class Values { short[] data={-32768,-17,0,123,32767}; }
    static class Bounds {
        Values root=new Values();int index;boolean flip;
        int plain(){return root.data[index];}
        int signed(){if(flip)return -root.data[index];return root.data[index];}
    }
    static class Other extends Bounds {int plain(){return 73;}int signed(){return -73;}}
    static int consume(Bounds b){return b.plain()+b.signed();}
    static void check(boolean ok){if(!ok)throw new RuntimeException("array getter mismatch");}
    public static int run(){
        Bounds b=new Bounds();int checks=0;
        for(int i=0;i<12000;++i){b.index=i%5;b.flip=i%4==0;int v=b.root.data[b.index];check(consume(b)==(b.flip?0:2*v));++checks;}
        check(consume(new Other())==0);++checks;
        b.index=-1;boolean caught=false;try{consume(b);}catch(ArrayIndexOutOfBoundsException e){caught=true;}check(caught);++checks;
        b.index=5;caught=false;try{consume(b);}catch(ArrayIndexOutOfBoundsException e){caught=true;}check(caught);++checks;
        b.index=0;b.root.data=null;caught=false;try{consume(b);}catch(NullPointerException e){caught=true;}check(caught);++checks;
        b.root=null;caught=false;try{consume(b);}catch(NullPointerException e){caught=true;}check(caught);++checks;
        return checks;
    }
}
