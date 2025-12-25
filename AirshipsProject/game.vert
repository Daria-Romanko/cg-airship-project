#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normalMatrix;

uniform float u_time;
uniform float u_swayStrength;

out VS_OUT {
    vec2 uv;
    vec3 worldPos;
    vec3 normal;
    mat3 TBN;
} vs_out;

void main()
{
    vec3 pos = aPos;

    if (u_swayStrength > 0.0001)
    {
        float weight = clamp(abs(aPos.y), 0.0, 1.0);
        float s1 = sin(u_time * 1.6 + aPos.y * 2.2);
        float s2 = cos(u_time * 1.2 + aPos.y * 1.7);
        pos.x += s1 * u_swayStrength * weight;
        pos.z += s2 * u_swayStrength * 0.7 * weight;
    }

    vec4 world = u_model * vec4(pos, 1.0);
    vs_out.worldPos = world.xyz;
    vs_out.uv = aUV;

    vec3 N = normalize(u_normalMatrix * aNormal);
    vec3 T = normalize(u_normalMatrix * aTangent);

    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T));

    vs_out.normal = N;
    vs_out.TBN = mat3(T, B, N);

    gl_Position = u_projection * u_view * world;
}
