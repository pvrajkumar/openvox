
-- Transform.GpuParticles

in vec3 Position;
in float Radius;
out vec3 vPosition;
out float vRadius;

void main()
{
    vPosition = Position;
    vRadius = Radius;
}

-- Blit.GpuParticles

out vec4 FragColor;
in vec2 vTexCoord;
uniform sampler2D Sampler;
uniform vec2 StepSize;

vec4 f(vec2 coord, float xOffset, float yOffset)
{
    coord += vec2(StepSize.x * xOffset, StepSize.y * yOffset);
    return texture2D(Sampler, coord);
}

void main()
{
    vec2 c = vTexCoord; c.y = 1.0 - c.y;
    vec4 v = 1 * f(c, -1, -1) + 2 * f(c, 0, -1) + 1 * f(c, 1, -1) +
             2 * f(c, -1, 0)  + 4 * f(c, 0, 0)  + 2 * f(c, 1, 0)  +
             1 * f(c, -1, 1)  + 2 * f(c, 0, 1)  + 1 * f(c, 1, 1);
    FragColor = vec4(0,0,0,v.a / 16);
}

-- Blit.Particles

out vec4 FragColor;
in vec2 vTexCoord;
uniform sampler2D Sampler;
uniform vec2 StepSize;
uniform float Darkness;
uniform bool ClipParticles;
const float Tau = 10.0;

float f(vec2 coord, float xOffset, float yOffset)
{
    coord += vec2(StepSize.x * xOffset, StepSize.y * yOffset);
    vec2 v = texture2D(Sampler, coord).rg;
    if (v.y == 0.0)
        return 0.0;
    return v.x / v.y;
}

void main()
{
    vec2 c = vTexCoord; c.y = 1.0 - c.y;
    float fade = min(texture2D(Sampler, c).b, 5) / 5;
    
    if (!ClipParticles)
    {
        FragColor = vec4(0, 0, 0, fade);
        return;
    }
    
    float thickness = 1 * f(c, -1, -1) + 2 * f(c, 0, -1) + 1 * f(c, 1, -1) +
                      2 * f(c, -1, 0)  + 4 * f(c, 0, 0)  + 2 * f(c, 1, 0)  +
                      1 * f(c, -1, 1)  + 2 * f(c, 0, 1)  + 1 * f(c, 1, 1);
                      
    thickness /= 4 + 8 + 4;

    if (thickness == 0.0)
    {
        discard;
        return;
    }
    
    thickness *= 2;
    float a = 2 * clamp(thickness, 0.0, 1.0);
    a *= fade;
    FragColor = vec4(0, 0, 0, max(0,a - 0.125));
}

-- Raycast.Debug

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D RayStart;
uniform sampler2D RayStop;
uniform sampler3D Volume;

uniform float StepLength = 0.01;

void main()
{
    vec3 rayStart = texture(RayStart, vTexCoord).xyz;
    vec3 rayStop = texture(RayStop, vTexCoord).xyz;
    
    if (rayStart == rayStop)
    {
        FragColor = vec4(0, 0.5, 1, 0.75);
        //discard;
        return;
    }

    vec3 ray = rayStop - rayStart;
    float rayLength = length(ray);
    vec3 stepVector = StepLength * ray / rayLength;

    vec3 pos = rayStart;
    vec4 dst = vec4(0);
    float previous = 0;
    int count = 0;
    while (rayLength > 0) {
        float V = texture(Volume, pos).r > 0.0 ? 1.0 : 0.0;
        if (V > 0.0 && V != previous) {
            count++;
            dst.a = 1;
            dst.r += 0.25;
            if (dst.r > 1.0)
                dst.g += 0.25;
            if (dst.g > 0.25)
                dst.b += 0.25;
        }
        vec4 src = vec4(V);
        src.rgb *= src.a;
        dst = (1.0 - dst.a) * src + dst;
        pos += stepVector;
        rayLength -= StepLength;
        previous = V;
    }

    FragColor = dst;
}


-- Raycast

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D RayStart;
uniform sampler2D RayStop;
uniform sampler3D Volume;

uniform float StepLength = 0.01;

void main()
{
    vec3 rayStart = texture(RayStart, vTexCoord).xyz;
    vec3 rayStop = texture(RayStop, vTexCoord).xyz;
  
    if (rayStart == rayStop) {
        //FragColor = vec4(0, 0.5, 1, 0.75);
        //return;
        discard;
    }

    vec3 ray = rayStop - rayStart;
    float rayLength = length(ray);
    vec3 stepVector = StepLength * ray / rayLength;

    vec3 pos = rayStart;
    vec4 dst = vec4(0);
    while (dst.a < 1 && rayLength > 0) {
        float V = 0.125 * texture(Volume, pos).r;
        vec4 src = vec4(V);
        src.rgb *= src.a;
        dst = (1.0 - dst.a) * src + dst;
        pos += stepVector;
        rayLength -= StepLength;
    }

    FragColor = dst;
}

-- Interval.VS

in vec4 Position;
out vec3 vPosition;
uniform mat4 ModelviewProjection;
void main()
{
    gl_Position = ModelviewProjection * Position;
    vPosition = Position.xyz;
}

-- Interval.FS

in vec3 vPosition;
out vec3 FragColor;

void main()
{
    FragColor = 0.5 * (vPosition + 1.0);
}

-- Voxelize.VS

in vec4 Position;
out float vInstance;
uniform mat4 VoxelProjection;
uniform float SliceScale;

void main()
{
    vInstance = float(gl_InstanceID);
    vec4 position = Position;
    position.z += vInstance * SliceScale;
    gl_Position = VoxelProjection * position;
}

-- Voxelize.GS

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in float vInstance[3];

void main()
{
    gl_Layer = int(vInstance[0]);

    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    EndPrimitive();
}

-- Voxelize.FS

out float FragColor;

void main()
{
    FragColor = gl_FrontFacing ? -1.0 : +1.0;
}

-- Blit.VS

in vec4 Position;
in vec2 TexCoord;
out vec2 vTexCoord;

void main()
{
    vTexCoord = TexCoord;
    gl_Position = Position;
}

-- Blit.FS

out vec4 FragColor;
in vec2 vTexCoord;
uniform sampler2D Sampler;

void main()
{
    FragColor = texture2D(Sampler, vTexCoord);
}

-- Particle.VS

in vec4 Position;
in vec3 Velocity;
in float BirthTime;
uniform mat4 ModelviewProjection;
uniform float Time;
out float vAlpha;
out vec3 vVelocity;
out vec3 vPosition;
uniform float FadeRate;

void main()
{
    gl_Position = ModelviewProjection * Position;
    vAlpha = max(0.0, 1.0 - (Time - BirthTime) * FadeRate);
    vVelocity = normalize(Velocity);
    vPosition = Position.xyz;
}

-- Particle.GS

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat3 Modelview;
uniform mat4 ModelviewProjection;
uniform mat4 CubeProjection;
uniform float PointSize;
in vec3 vPosition[1];
in vec3 vVelocity[1];
in float vAlpha[1];
out float gAlpha;
out vec3 gTransformedCoord;
out vec3 gTransformedCenter;
out vec2 gTexCoord;
out vec3 gVolumeCoord;
out vec3 gParticleCenter;
out float gParticleRadius;

float Epsilon = 0.001;

vec3 to_volume(vec3 v)
{
    vec4 vPrime = CubeProjection * vec4(v,1);
    return 0.5 * (vec3(1) + vPrime.xyz);
}

void emit(vec3 v)
{
    gVolumeCoord = to_volume(v);
    gTransformedCoord = Modelview * v;
    gl_Position = ModelviewProjection * vec4(v,1);
    EmitVertex();
}

vec3 perp(vec3 v)
{
    vec3 b = cross(v, vec3(0, 0, 1));
    if (dot(b, b) < 0.01)
        b = cross(v, vec3(0, 1, 0));
    return b;
}

void main()
{
    gAlpha = vAlpha[0];

    vec3 p = vPosition[0];
    float w = PointSize * 0.5;
    float h = w * 2.0;
    vec3 u = Modelview * vVelocity[0];

    // Determine 't', which represents Z-aligned magnitude.
    // By default, t = 0.0.
    // If velocity aligns with Z, increase t towards 1.0.
    // If total speed is negligible, increase t towards 1.0.
    float t = 0.0;
    float nz = abs(normalize(u).z);
    if (nz > 1.0 - Epsilon)
        t = (nz - (1.0 - Epsilon)) / Epsilon;
    if (dot(u,u) < Epsilon)
        t = (Epsilon - dot(u,u)) / Epsilon;

    // Disable motion blur for now:
    t = 1.0;

    // Compute screen-space velocity:
    u.z = 0.0;
    u = normalize(u);

    // Lerp the orientation vector if screen-space velocity is negligible:
    u = normalize(mix(u, vec3(0,1,0), t));
    h = mix(h, w, t);

    // Compute the change-of-basis matrix for the billboard:
    vec3 v = perp(u);
    vec3 a = u * Modelview;
    vec3 b = v * Modelview;
    vec3 c = cross(a, b);
    mat3 basis = mat3(a, b, c);

    // Compute the four offset vectors:
    vec3 N = basis * vec3(0,w,0);
    vec3 S = basis * vec3(0,-w,0);
    vec3 E = basis * vec3(-h,0,0);
    vec3 W = basis * vec3(h,0,0);

    // Send down the billboard's center position:
    gParticleCenter = to_volume(p);
    gParticleRadius = 0.5 * min(w, h);
    gTransformedCenter = Modelview * p;

    // Emit the quad:
    gTexCoord = vec2(1,1); emit(p+N+E);
    gTexCoord = vec2(0,1); emit(p+N+W);
    gTexCoord = vec2(1,0); emit(p+S+E);
    gTexCoord = vec2(0,0); emit(p+S+W);
    EndPrimitive();
}

-- Particle.Solid.FS

in float gAlpha;
in vec2 gTexCoord;
in vec3 gVolumeCoord;
in vec3 gParticleCenter;
in float gParticleRadius;
out vec4 FragColor;
uniform sampler3D VolumeSampler;
uniform bool ShowBillboards;
uniform float Darkness;
uniform vec3 StepSize;
uniform mat3 Modelview;
in vec3 gTransformedCoord;
in vec3 gTransformedCenter;

const float Tau = 1.0;
const float MaxRadius = 1.0;

// Returns distance to nearest boundary along the given ray
float boundary_distance(vec3 origin, vec3 direction)
{
    vec3 stepSize = StepSize;
    vec3 step = stepSize * direction;
    vec3 coord = origin;
    float stepLength = 0.01;
    float T = 0;
    while (T < MaxRadius)
    {
        float V = texture(VolumeSampler, coord).r;
        if (V < 1.0) return T;
        T += stepLength;
        coord += step;
    }
    return MaxRadius;
}

void main()
{
    if (ShowBillboards)
    {
        FragColor.r = 0.1;
        FragColor.g = 1;
        return;
    }
    
    FragColor = vec4(0);
    
    // March from gParticleCenter to gVolumeCoord; discard the fragment if encountering empty voxel
    float rayLength = distance(gParticleCenter, gVolumeCoord);
    if (rayLength > 0.0)
    {
        // Exploit HW interpolation by double-marching at texel boundaries:
        vec3 stepSize = vec3(min(min(StepSize.x, StepSize.y),StepSize.z)); // <--- we need a smarter way of precisely marching on texel boundaries
        vec3 step = 2.0 * stepSize * (gVolumeCoord - gParticleCenter) / rayLength;
        vec3 coord = gParticleCenter + step * 0.25;
        
        // Perform the march:
        float stepLength = length(step);
        for (; rayLength > -stepLength / 2; coord += step, rayLength -= stepLength)
        {
            float V = texture(VolumeSampler, coord).r;
            if (V < 1.0)
            {
                FragColor.r = -1.0;
            }
        }
    }
        
    // Write thickness to the red channel:
    if (FragColor.r != -1)
    {
        vec3 k = Modelview * vec3(0,0,1);
        FragColor.r = boundary_distance(gVolumeCoord, -k) + boundary_distance(gVolumeCoord, +k);

        // Increment the billboard count in the green channel: (we're using additive blending)
        FragColor.g = 1.0;
    }
    
    float d = distance(gTransformedCoord.xy, gTransformedCenter.xy);
    float r = 2 * gParticleRadius;
    FragColor.b = 1 - exp(-Tau * (1-d/r));

    // Final clamp-to-zero:
    FragColor = vec4(max(vec4(0), FragColor));
}

-- Particle.Surface.FS

in float gAlpha;
in vec2 gTexCoord;
in vec3 gTransformedCoord;
in vec3 gTransformedCenter;
in vec3 gVolumeCoord;
in vec3 gParticleCenter;
in float gParticleRadius;
out vec4 FragColor;
uniform sampler3D VolumeSampler;
uniform bool ShowBillboards;
uniform float Darkness;
uniform vec3 StepSize;
uniform mat3 Modelview;

const float Tau = 1.0;
const float MaxRadius = 1.0;

// Returns distance to nearest boundary along the given ray
float boundary_distance(vec3 origin, vec3 direction)
{
    vec3 stepSize = StepSize;
    vec3 step = stepSize * direction;
    vec3 coord = origin;
    float stepLength = 0.01;
    float T = 0;
    while (T < MaxRadius)
    {
        float V = texture(VolumeSampler, coord).r;
        if (V > 0.0) return T;
        T += stepLength;
        coord += step;
    }
    return MaxRadius;
}

void main()
{
    if (ShowBillboards)
    {
        FragColor.r = 0.1;
        FragColor.g = 1;
        return;
    }
    
    FragColor = vec4(0);
    
    // March from gParticleCenter to gVolumeCoord; discard the fragment if encountering empty voxel
    float rayLength = distance(gParticleCenter, gVolumeCoord);
    if (rayLength > 0.0)
    {
        // Exploit HW interpolation by double-marching at texel boundaries:
        vec3 stepSize = vec3(min(min(StepSize.x, StepSize.y),StepSize.z)); // <--- we need a smarter way of precisely marching on texel boundaries
        vec3 step = stepSize * (gVolumeCoord - gParticleCenter) / rayLength;
        vec3 coord = gParticleCenter + step * 0.25;
        
        // Perform the march:
        float stepLength = length(step);
        for (; rayLength > -stepLength / 2; coord += step, rayLength -= stepLength)
        {
            float V = texture(VolumeSampler, coord).r;
            if (V > 0.0)
            {
                FragColor.r = -1.0;
                break;
            }
        }
    }
    
    // Write thickness to the red channel:
    if (FragColor.r != -1)
    {
        vec3 k = Modelview * vec3(0,0,1);
        FragColor.r = boundary_distance(gVolumeCoord, -k) + boundary_distance(gVolumeCoord, +k);

        // Increment the billboard count in the green channel: (we're using additive blending)
        FragColor.g = 1.0;
    }
    
    float d = distance(gTransformedCoord.xy, gTransformedCenter.xy);
    float r = 2 * gParticleRadius;
    FragColor.b = 1 - exp(-Tau * (1-d/r));

    // Final clamp-to-zero:
    FragColor = vec4(max(vec4(0), FragColor));
}

-- Wireframe.VS

in vec4 Position;
uniform mat4 ModelviewProjection;

void main()
{
    gl_Position = ModelviewProjection * Position;
}

-- Wireframe.FS

out vec4 FragColor;

void main()
{
    FragColor = vec4(0.5, 0.5, 0.6, 0.5);
}

-- Particle.Interval.GS

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;
uniform mat4 ModelviewProjection;
uniform mat4 CubeProjection;
uniform float PointSize;
in vec3 vPosition[1];
out vec3 gVolumeCoord;
vec3 cuboid[8];

vec3 to_volume(vec3 v)
{
    vec4 vPrime = CubeProjection * vec4(v,1);
    return 0.5 * (1 + vPrime.xyz);
}

void emit_vert(vec3 v)
{
    gVolumeCoord = to_volume(v);
    gl_Position = ModelviewProjection * vec4(v,1);
    EmitVertex();
}

void emit_face(int a, int b, int c, int d)
{
    emit_vert(cuboid[a]);
    emit_vert(cuboid[b]);
    emit_vert(cuboid[c]);
    emit_vert(cuboid[d]);
    EndPrimitive();
}

void main()
{
    vec3 p = vPosition[0];
    float r = PointSize * 0.5;
    vec3 i = r * vec3(1,0,0);
    vec3 j = r * vec3(0,1,0);
    vec3 k = r * vec3(0,0,1);
    
    cuboid[0] = p + i + j + k;
    cuboid[1] = p + i + j - k;
    cuboid[2] = p - i + j - k;
    cuboid[3] = p - i + j + k;
    cuboid[4] = p + i - j + k;
    cuboid[5] = p + i - j - k;
    cuboid[6] = p - i - j - k;
    cuboid[7] = p - i - j + k;

    emit_face(0,1,3,2); 
    emit_face(5,4,6,7);
    emit_face(4,5,0,1);
    emit_face(3,2,7,6);
    emit_face(0,3,4,7);
    emit_face(2,1,6,5);
}

-- Particle.Interval.FS

in vec3 gVolumeCoord;
out vec3 FragColor;

void main()
{
    FragColor = gVolumeCoord;
}

-- Vessel.Interval.VS

in vec4 Position;
out vec3 vPosition;
uniform mat4 ModelviewProjection;
uniform mat4 CubeProjection;

vec3 to_volume(vec3 v)
{
    vec4 vPrime = CubeProjection * vec4(v,1);
    return 0.5 * (1 + vPrime.xyz);
}

void main()
{
    gl_Position = ModelviewProjection * Position;
    vPosition = to_volume(Position.xyz);
}

-- Vessel.Interval.FS

in vec3 vPosition;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vPosition, 1);
}

-- Combine.Interval.FS

out vec3 FragColor;
in vec2 vTexCoord;
uniform sampler2D ParticleCoords;
uniform sampler2D ParticleDepths;
uniform sampler2D VesselCoords;
uniform sampler2D VesselDepths;
uniform bool RayStart;

void main()
{
    vec3 pc = texture2D(ParticleCoords, vTexCoord).rgb;
    vec3 vc = texture2D(VesselCoords, vTexCoord).rgb;
    float pd = texture2D(ParticleDepths, vTexCoord).x;
    float vd = texture2D(VesselDepths, vTexCoord).x;
    
    if (RayStart)
    {
        FragColor = vd < pd ? vc : pc;
    }
    else
    {
        FragColor = vd > pd ? vc : pc;
    }
}

-- Particle.Raycast

uniform sampler2D RayStart;
uniform sampler2D RayStop;
uniform sampler3D Boundaries;
uniform sampler2D ParticleBins;
uniform vec3 StepSize;
uniform mat3 CubeProjection;
uniform vec3 VolumeScale;
uniform vec3 VolumeOffset;
uniform bool FlipInterval;
uniform int NumBinColumns;
uniform int NumBinRows;
uniform int MaxParticlesPerBin;

in vec2 vTexCoord;
out vec4 FragColor;

float StepLength = 0.01;

bool IntersectSphere(vec3 rO, vec3 rD, vec3 sC, float sR, out float d, out float q)
{
    vec3 L = sC - rO;
    float s = dot(L, rD);
    float L2 = dot(L, L);
    float sqR = sR * sR;
    float m2 = L2 - s * s;
    q = sqrt(sqR - m2);
    d = (L2 > sqR) ? (s - q) : (s + q);
    return d > 0.0;
}

void main()
{
    vec2 tc = vTexCoord;
    vec2 tcInterval = tc; if (FlipInterval) tcInterval.y = 1 - tcInterval.y;

    vec3 rayStart = texture(RayStart, tcInterval).xyz;
    if (rayStart.z == 0.0)
        discard;
        
    vec3 rayStop = texture(RayStop, tcInterval).xyz;
  
    if (rayStart == rayStop)
        discard;
    
    int i = int(tc.x * NumBinColumns);
    int j = int(tc.y * NumBinRows);
    if (i < 0 || i >= NumBinColumns || j < 0 || j >= NumBinRows)
        discard;
        
    float thickness = 0;
    vec3 rayDir = normalize(rayStop - rayStart);

    FragColor = vec4(0);

    vec3 rayStartPrime = (2.0 * rayStart - 1.0) * CubeProjection;
    vec3 rayStopPrime = (2.0 * rayStop - 1.0) * CubeProjection;
    vec3 rayDirPrime = normalize(rayStopPrime - rayStartPrime);

    ivec2 coord = ivec2(i * (MaxParticlesPerBin + 1), NumBinRows - j - 1);
    for (int particleIndex = 0; particleIndex < MaxParticlesPerBin; particleIndex++, coord.x++)
    {
        vec4 particle = texelFetch(ParticleBins, coord, 0);
        if (particle.w == 0.0)
            break;

        vec3 sphereCenter = particle.xyz;
        float radius = particle.w;

        float t, q;
        if (IntersectSphere(rayStartPrime, rayDirPrime, sphereCenter, radius, t, q))
            thickness += q * 0.2;
    }

    // Draw the vessel:

    vec3 ray = rayStop - rayStart;
    float rayLength = length(ray);
    vec3 stepVector = StepLength * ray / rayLength;
    vec3 pos = rayStart;
    vec4 dst = vec4(0, 0, 0, thickness);

    while (dst.a < 1 && rayLength > 0) {
        float V = 0.125 * texture(Boundaries, pos).r;
        vec4 src = vec4(V);
        src.rgb *= src.a;
        dst = (1.0 - dst.a) * src + dst;
        pos += stepVector;
        rayLength -= StepLength;
    }
    
    FragColor = dst;
}
