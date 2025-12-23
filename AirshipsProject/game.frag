#version 330 core

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float intensity;
};

uniform DirLight u_dirLight;

uniform vec3 u_viewPos;

uniform sampler2D u_diffuse;
uniform sampler2D u_normalMap;
uniform bool u_useNormalMap;

uniform float u_emissionStrength;
uniform vec3 u_tint;

in VS_OUT {
    vec2 uv;
    vec3 worldPos;
    vec3 normal;
    mat3 TBN;
} fs_in;

out vec4 FragColor;

void main()
{
    vec3 albedo = texture(u_diffuse, fs_in.uv).rgb * u_tint;

    vec3 N = normalize(fs_in.normal);
    if (u_useNormalMap) {
        vec3 nTex = texture(u_normalMap, fs_in.uv).rgb;
        nTex = nTex * 2.0 - 1.0;
        N = normalize(fs_in.TBN * nTex);
    }

    vec3 V = normalize(u_viewPos - fs_in.worldPos);
    vec3 L = normalize(-u_dirLight.direction);

    float diff = max(dot(N, L), 0.0);

    vec3 H = normalize(L + V);
    float specPow = 32.0;
    float spec = pow(max(dot(N, H), 0.0), specPow) * diff * 0.25;

    vec3 ambient = u_dirLight.ambient * albedo;
    vec3 diffuse = u_dirLight.diffuse * diff * albedo;
    vec3 specular = u_dirLight.specular * spec;

    vec3 color = (ambient + diffuse + specular) * u_dirLight.intensity;

    if (u_emissionStrength > 0.0) {
        vec3 lightning = vec3(0.75, 0.85, 1.0);
        color += lightning * u_emissionStrength;
    }

    FragColor = vec4(color, 1.0);
}
