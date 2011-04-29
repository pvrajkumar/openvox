-- Surfaces

#pragma OPENCL EXTENSION cl_khr_byte_addressable_store: enable

#define X 0
#define Y 1
#define Z 2

#define SUB(dest,v1,v2) \
          dest[X]=v1[X]-v2[X]; \
          dest[Y]=v1[Y]-v2[Y]; \
          dest[Z]=v1[Z]-v2[Z]; 

/*======================== X-tests ========================*/
#define AXISTEST_X01(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			       	   \
	p2 = a*v2[Y] - b*v2[Z];			       	   \
	minn = min(p0,p2); maxx = max(p0, p2);     \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(minn>rad || maxx<-rad) return 0;

#define AXISTEST_X2(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			           \
	p1 = a*v1[Y] - b*v1[Z];			       	   \
	minn = min(p0,p1); maxx = max(p0, p1);     \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(minn>rad || maxx<-rad) return 0;

/*======================== Y-tests ========================*/
#define AXISTEST_Y02(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p2 = -a*v2[X] + b*v2[Z];	       	       	   \
	minn = min(p0,p2); maxx = max(p0, p2);     \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(minn>rad || maxx<-rad) return 0;

#define AXISTEST_Y1(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p1 = -a*v1[X] + b*v1[Z];	     	       	   \
	minn = min(p0,p1); maxx = max(p0, p1);     \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(minn>rad || maxx<-rad) return 0;

/*======================== Z-tests ========================*/

#define AXISTEST_Z12(a, b, fa, fb)			   \
	p1 = a*v1[X] - b*v1[Y];			           \
	p2 = a*v2[X] - b*v2[Y];			       	   \
	minn = min(p1,p2); maxx = max(p1, p2);     \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(minn>rad || maxx<-rad) return 0;

#define AXISTEST_Z0(a, b, fa, fb)			   \
	p0 = a*v0[X] - b*v0[Y];				   \
	p1 = a*v1[X] - b*v1[Y];			           \
	minn = min(p0,p1); maxx = max(p0, p1);     \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(minn>rad || maxx<-rad) return 0;

inline int triBoxOverlap(
    float boxhalfsize[3],
    float v0[3], float v1[3], float v2[3],
    float e0[3], float e1[3], float e2[3],
    float fe0[3], float fe1[3], float fe2[3])
{
   float minn,maxx,p0,p1,p2,rad;
   AXISTEST_X01(e0[Z], e0[Y], fe0[Z], fe0[Y]);
   AXISTEST_Y02(e0[Z], e0[X], fe0[Z], fe0[X]);
   AXISTEST_Z12(e0[Y], e0[X], fe0[Y], fe0[X]);
   AXISTEST_X01(e1[Z], e1[Y], fe1[Z], fe1[Y]);
   AXISTEST_Y02(e1[Z], e1[X], fe1[Z], fe1[X]);
   AXISTEST_Z0(e1[Y], e1[X], fe1[Y], fe1[X]);
   AXISTEST_X2(e2[Z], e2[Y], fe2[Z], fe2[Y]);
   AXISTEST_Y1(e2[Z], e2[X], fe2[Z], fe2[X]);
   AXISTEST_Z12(e2[Y], e2[X], fe2[Y], fe2[X]);
   return 1;
}

kernel void voxelize(
    write_only global uchar* volume,
    float xscale, float yscale, float zscale,
    float xoffset, float yoffset, float zoffset,
    int rowPitch, int slicePitch,
    int width, int height, int depth,
    read_only global const float* verts, read_only global const uint* faces, uint triangleCount)
{
    float normal[3],e0[3],e1[3],e2[3];
    const uint triangleIndex = get_global_id(0);

    // due to quantization into grids, there can be more threads than triangles:
    if (triangleIndex >= triangleCount)
        return;
        
    uint A = faces[3*triangleIndex];
    uint B = faces[3*triangleIndex+1];
    uint C = faces[3*triangleIndex+2];
    
    float Ax = verts[A*3];
    float Ay = verts[A*3+1];
    float Az = verts[A*3+2];
    
    int iAx = (int) ((Ax + xoffset) * xscale);
    int iAy = (int) ((Ay + yoffset) * yscale);
    int iAz = (int) ((Az + zoffset) * zscale);
    
    float Bx = verts[B*3];
    float By = verts[B*3+1];
    float Bz = verts[B*3+2];

    int iBx = (int) ((Bx + xoffset) * xscale);
    int iBy = (int) ((By + yoffset) * yscale);
    int iBz = (int) ((Bz + zoffset) * zscale);

    float Cx = verts[C*3];
    float Cy = verts[C*3+1];
    float Cz = verts[C*3+2];

    int iCx = (int) ((Cx + xoffset) * xscale);
    int iCy = (int) ((Cy + yoffset) * yscale);
    int iCz = (int) ((Cz + zoffset) * zscale);
    
    int minX = min(min(iAx, iBx), iCx);
    int minY = min(min(iAy, iBy), iCy);
    int minZ = min(min(iAz, iBz), iCz);
    int maxX = max(max(iAx, iBx), iCx)+1;
    int maxY = max(max(iAy, iBy), iCy)+1;
    int maxZ = max(max(iAz, iBz), iCz)+1;
    
    minX = min(width-1,max(minX, 0));
    minY = min(height-1,max(minY, 0));
    minZ = min(depth-1,max(minZ, 0));
    maxX = min(width-1,max(maxX, 0));
    maxY = min(height-1,max(maxY, 0));
    maxZ = min(depth-1,max(maxZ, 0));

    float delta[3] = { 1.0/xscale, 1.0/yscale, 1.0/zscale };
    float boxhalfsize[3] = { 0.5*delta[X], 0.5*delta[Y], 0.5*delta[Z] };
    float triverts[3][3] = { { Ax, Ay, Az}, {Bx, By, Bz}, {Cx, Cy, Cz} };
    float boxcenter[3] = { minX*delta[X] - xoffset, minY*delta[Y] - yoffset, minZ*delta[Z] - zoffset };

    float v0[3] = {Ax, Ay, Az};
    float v1[3] = {Bx, By, Bz};
    float v2[3] = {Cx, Cy, Cz};
    SUB(e0,v1,v0);
    SUB(e1,v2,v1);
    SUB(e2,v0,v2);
    
    float fe0[3] = {fabs(e0[X]), fabs(e0[Y]), fabs(e0[Z])};
    float fe1[3] = {fabs(e1[X]), fabs(e1[Y]), fabs(e1[Z])};
    float fe2[3] = {fabs(e2[X]), fabs(e2[Y]), fabs(e2[Z])};

    SUB(v0,triverts[X],boxcenter);
    SUB(v1,triverts[Y],boxcenter);
    SUB(v2,triverts[Z],boxcenter);

    volume += minX + minY*rowPitch + (depth-1-minZ)*slicePitch;
    for (int z = minZ; z <= maxZ; z++) {
        global uchar* slice = volume;
        for (int y = minY; y <= maxY; y++) {
        
            global uchar* row = slice;
            for (int x = minX; x <= maxX; x++, row++) {
                
                // Do triangle ABC and voxel XYZ intersect?
                // http://jgt.akpeters.com/papers/AkenineMoller01/tribox.html
                
                if (triBoxOverlap(boxhalfsize, v0, v1, v2, e0, e1, e2, fe0, fe1, fe2))
                    *row = 255;
                    
                v0[X] -= delta[X]; v1[X] -= delta[X];  v2[X] -= delta[X];
            }
            slice += rowPitch;
            
            v0[X] = Ax-boxcenter[X]; v1[X] = Bx-boxcenter[X];  v2[X] = Cx-boxcenter[X];
            v0[Y] -= delta[Y]; v1[Y] -= delta[Y];  v2[Y] -= delta[Y];
        }
        volume -= slicePitch;
        
        v0[Z] -= delta[Z]; v1[Z] -= delta[Z];  v2[Z] -= delta[Z];
        v0[Y] = Ay-boxcenter[Y]; v1[Y] = By-boxcenter[Y];  v2[Y] = Cy-boxcenter[Y];
    }
}

--------- Scratch Space ---------

kernel void voxelze(write_only image3d_t volume)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);
}
    
kernel void voxelize(
    write_only image3d_t volume,
    read_only global const float3* verts,
    read_only global const ushort3* faces)
{
}

kernel void fast_clear(
    write_only global uchar* volume,
    uint width, uint height, uint depth,
    uint rowPitch, uint slicePitch)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int z = get_global_id(2);

    // due to quantization into grids, there can be more threads than pixels:
    if (x >= width || y >= height || z >= depth)
        return;
    
    volume[x + y*rowPitch + z*slicePitch] = 0;
}

kernel void clear(
    write_only global uchar* volume,
    uint width, uint height, uint depth,
    uint rowPitch, uint slicePitch)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    // due to quantization into grids, there can be more threads than pixels:
    if (x >= width || y >= height)
        return;
    
    // walk across slices:
    for (int z = 0; z < depth; ++z)
        volume[x + y*rowPitch + z*slicePitch] = 0;
}
