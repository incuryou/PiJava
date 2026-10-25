import java.util.Hashtable;
import java.util.Enumeration;
public class Runtime235Checks {
    static int checks=0;
    static void check(boolean yes){if(!yes)throw new RuntimeException("235 library check "+checks);checks++;}
    static class SideKey {
        static int calls;int value;
        SideKey(int v){value=v;}
        public int hashCode(){return 1;}
        public boolean equals(Object o){calls++;return o instanceof SideKey && ((SideKey)o).value==value;}
    }
    static class BadKey { public boolean equals(Object o){throw new IllegalArgumentException("intentional");} }
    static Object key(int k){switch(k%3){case 0:return new Character((char)k);case 1:return new Integer(k);default:return new Long(0x100000000L+k);}}
    static int seed=0x235;
    static int next(){seed=seed*1664525+1013904223;return seed&0x7fffffff;}
    public static int run(){
        Hashtable h=new Hashtable(1);Object[] keys=new Object[128];Object[] values=new Object[128];int count=0;
        for(int i=0;i<16000;i++){
            int choice=next()%5;Object k=key(next()%121);int at=-1;
            for(int j=0;j<count;j++)if(keys[j].equals(k)){at=j;break;}
            if(choice==0||choice==1){Object val=new Integer(i);check(h.put(k,val)==(at<0?null:values[at]));if(at<0){keys[count]=k;values[count++]=val;}else values[at]=val;}
            else if(choice==2){check(h.remove(k)==(at<0?null:values[at]));if(at>=0){for(int j=at;j<count-1;j++){keys[j]=keys[j+1];values[j]=values[j+1];}keys[--count]=null;values[count]=null;}}
            else {check(h.get(k)==(at<0?null:values[at]));check(h.containsKey(k)==(at>=0));}
            check(h.size()==count);
            if(i%97==0){Enumeration e=h.keys(),v=h.elements();for(int j=0;j<count;j++){check(e.hasMoreElements()&&v.hasMoreElements());check(e.nextElement()==keys[j]&&v.nextElement()==values[j]);}check(!e.hasMoreElements()&&!v.hasMoreElements());}
            if(i%503==0){h.clear();count=0;check(h.isEmpty());}
        }
        h.clear();SideKey a=new SideKey(1),b=new SideKey(2);h.put(a,"a");h.put(b,"b");SideKey.calls=0;
        check(h.get(new SideKey(2)).equals("b")&&SideKey.calls==2);
        h.put(new Character('a'),"c");SideKey.calls=0;check(h.get(new Character('a')).equals("c")&&SideKey.calls==2);
        h.clear();h.put(new BadKey(),"bad");try{h.get(new Character('x'));check(false);}catch(IllegalArgumentException ok){checks++;}
        for(int i=0;i<4;i++){try{if(i==0)h.get(null);if(i==1)h.containsKey(null);if(i==2)h.put(new Object(),null);if(i==3)h.remove(null);check(false);}catch(NullPointerException ok){checks++;}}
        check(new Long(Long.MIN_VALUE).equals(new Long(Long.MIN_VALUE)));check(!new Integer(1).equals(new Long(1)));
        // Unreviewed legacy wrappers inherit Object.equals. Do not substitute
        // scalar-value semantics for a method the bundled library does not implement.
        h.clear();Byte legacy=new Byte((byte)7);h.put(legacy,"legacy");check(h.get(legacy).equals("legacy")&&h.get(new Byte((byte)7))==null);
        check(new Character('\uffff').charValue()==65535);check(new Byte((byte)-128).byteValue()==-128);check(new Short((short)-32768).shortValue()==-32768);
        check(!"".equals(new Object())&&!"".equals(new char[0])&&!"".equals(null)&&"".equals(new String(new char[0])));
        check("abc".equals(new String(new char[]{'a','b','c'}))&&!"abc".equals(new Integer(1)));
        return checks;
    }
    public static int glyphs(int loops){Hashtable h=new Hashtable();for(int i=0;i<4096;i++)h.put(new Character((char)i),new Integer(i));int sum=0;for(int n=0;n<loops;n++)for(int i=0;i<4096;i++)sum+=((Integer)h.get(new Character((char)i))).intValue();return sum;}
}
