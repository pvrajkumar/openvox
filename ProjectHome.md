# Overview #

OpenVOX is a real-time voxelization and transformation API designed to live on top of OpenCL.  It can generate boundary voxelizations, solid voxelizations, and distance fields.  Currently it is just an API specification, not an implementation.

  * Designed for operations that take place on a per-frame basis
  * Not suitable for huge data sets, offline processing, or serialization to disk
  * Exposes a simple C API, following conventions borrowed from OpenGL and OpenCL
  * Directly reads from OpenGL VBOs and writes to OpenGL textures (no CPU round-tripping)
  * Does not support visualization or rendering (a simple raycast demo is provided)
  * Supports manipulations such as volumetric blitting, blending, splatting, and path-sweeping

OpenVOX isn't what you need?  Check out other projects such as [binvox](http://www.cs.princeton.edu/~min/binvox/) and [Field3D](http://sites.google.com/site/field3d/).

# Status #

The API header file (`openvox.h`) is checked in; it's fairly well fleshed-out, but implementation efforts have mostly been abandoned.  The proof-of-concept demo that uses OpenCL is included in the `SurfaceVoxels` folder.

# Usage #

Create a 64x64x64 OpenCL buffer representing a boundary voxelization of a bunny mesh:

```
VOXhandle context = voxCreateContext(0, 0);
VOXhandle bunny = voxRegisterMesh(context, bunny_vbo, 3*sizeof(float), tri_count);
VOXhandle voxels = voxCreateVolume(context, 64, 64, 64, VOX_TYPE_UBYTE, 0, 0);
voxVoxelize(bunny, voxels, VOX_VOXELIZE_SURFACE_CONSERVATIVE);
```

Fill an existing 64x64x64 OpenGL texture with a volumetric voxelization of a bunny mesh:

```
voxels = voxCreateVolume(context, 64, 64, 64, VOX_TYPE_UBYTE, VOX_SOURCE_GL_TEXTURE|VOX_SOURCE_USE_PTR, &texHandle);
voxVoxelize(bunny, voxels, VOX_VOXELIZE_VOLUMETRIC);
```

Create a distance field from an existing boundary voxelization:

```
VOXhandle distance = voxCreateVolume(context, 64, 64, 64, VOX_TYPE_FLOAT, 0, 0);
voxTransform(distance, voxels, VOX_TRANSFORM_DISTANCE_EUCLIDEAN);
```
VOXhandle distance = voxCreateVolume(context, 64, 64, 64, VOX_TYPE_FLOAT, 0, 0);
voxTransform(distance, voxels, VOX_TRANSFORM_DISTANCE_EUCLIDEAN);
}}}
```