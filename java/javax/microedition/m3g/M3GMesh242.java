package javax.microedition.m3g;
/** Bounded geometry helpers; Java owns all arrays and all publication. */
final class M3GMesh242 {
    static native boolean enabled();
    static native boolean eligible(VertexArray p, VertexArray n, VertexArray dst, VertexArray dn, VertexBuffer posed);
    static native boolean composite(Transformable object, Transform target);
    static native int transformTo(Node from, Node target, Transform output);
    static native boolean skinBlock(VertexArray pos, VertexArray norms, float[][] matrices,
        int[] ranges, float[] scaleBias, float[] result, float[] normals, float[] bounds,
        int first, int count);
    static native boolean quantize(float[] source, Object target, int first, int count,
        int components, float invScale, float bx, float by, float bz, boolean positions);
    static native boolean identity(float[] matrix);
    static native boolean invert(float[] matrix, float[] scratch);

    static boolean skin(VertexArray pos, VertexArray norms, VertexArray dst, VertexArray dstNorm,
        VertexBuffer posed, float[][] matrices, int[] ranges, float[] scaleBias) {
        if(!eligible(pos,norms,dst,dstNorm,posed))return false;
        int n=pos.getVertexCount(), c=pos.getComponentCount(), type=pos.getComponentType();
        if(c<2 || c>3 || n<1 || n>65535 || matrices.length>256) return false;
        boolean hasNormals=norms!=null && dstNorm!=null;
        if(hasNormals && c!=3) return false;
        float[] result=new float[n*c];
        float[] normal=hasNormals?new float[n*c]:null;
        float[] bounds={3.4028235e38f,-3.4028235e38f,3.4028235e38f,-3.4028235e38f,
            c>2?3.4028235e38f:scaleBias[3],c>2?-3.4028235e38f:scaleBias[3]};
        for(int first=0;first<n;first+=128)
            if(!skinBlock(pos,hasNormals?norms:null,matrices,ranges,scaleBias,result,normal,bounds,
                first,M3GMath.min(128,n-first)))return false;
        float maxHalf=M3GMath.max((bounds[1]-bounds[0])*0.5f,
            M3GMath.max((bounds[3]-bounds[2])*0.5f,(bounds[5]-bounds[4])*0.5f));
        float scale=maxHalf>0.0f?maxHalf/(type==1?127.0f:32767.0f):scaleBias[0];
        if(scale==0.0f)scale=1.0f;
        float bx=(bounds[0]+bounds[1])*0.5f, by=(bounds[2]+bounds[3])*0.5f, bz=(bounds[4]+bounds[5])*0.5f;
        Object raw=type==1?(Object)new byte[n*c]:(Object)new short[n*c];
        Object rawNormal=!hasNormals?null:dstNorm.getComponentType()==1?(Object)new byte[n*c]:(Object)new short[n*c];
        for(int first=0;first<n*c;first+=1024) {
            int count=M3GMath.min(1024,n*c-first);
            if(!quantize(result,raw,first,count,c,1.0f/scale,bx,by,bz,true))return false;
            if(hasNormals && !quantize(normal,rawNormal,first,count,c,1.0f,0,0,0,false))return false;
        }
        // Only now publish: no partially skinned pose if a helper declines.
        if(type==1)dst.set(0,n,(byte[])raw); else dst.set(0,n,(short[])raw);
        posed.setPositions(dst,scale,new float[]{bx,by,bz});
        if(hasNormals) {
            if(dstNorm.getComponentType()==1)dstNorm.set(0,n,(byte[])rawNormal);
            else dstNorm.set(0,n,(short[])rawNormal);
        }
        return true;
    }
}
