#version 330 core
out vec4 FragColor;

// --- INPUT ---
uniform vec2 u_resolution;
uniform vec3 u_cam_origin;
uniform vec3 u_cam_lookat;
uniform float u_fov;
uniform float u_aperture; 
uniform float u_focus_dist;
uniform float u_time;      
uniform int u_frame_index;
uniform int u_num_spheres;
uniform int u_num_triangles;

// --- COSTANTI ---
#define EPSILON 0.001
#define MAX_DIST 100000.0
#define MAX_BOUNCES 50

// --- STRUTTURE ---
struct Sphere {
    vec3 center; float radius; vec3 color; int material; float param;  
};
struct Triangle {
    vec3 v0, v1, v2; vec3 color; int material; float param;
};

#define MAX_OBJ 128
uniform Sphere u_spheres[MAX_OBJ];
uniform Triangle u_triangles[MAX_OBJ];

struct HitRecord {
    float t;
    vec3 p;
    vec3 normal;
    int index;
    int type; 
};

// --- RNG MIGLIORATO ---
uint hash_state;

uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

void init_rng(vec2 coord, int frame) {
    hash_state = wang_hash(uint(coord.x) + wang_hash(uint(coord.y) + wang_hash(uint(frame))));
}

float rand() {
    hash_state = wang_hash(hash_state);
    return float(hash_state) / 4294967296.0;
}

vec3 random_in_unit_sphere() {
    float phi = rand() * 6.28318530718;
    float costheta = rand() * 2.0 - 1.0;
    float u = rand();
    float r = pow(u, 0.333333);
    float sintheta = sqrt(1.0 - costheta * costheta);
    return vec3(r * sintheta * cos(phi), r * sintheta * sin(phi), r * costheta);
}

vec3 random_on_hemisphere(vec3 normal) {
    vec3 in_unit_sphere = random_in_unit_sphere();
    if (dot(in_unit_sphere, normal) > 0.0)
        return in_unit_sphere;
    else
        return -in_unit_sphere;
}

vec2 random_in_unit_disk() {
    float a = rand() * 6.28318530718;
    float r = sqrt(rand());
    return vec2(r * cos(a), r * sin(a));
}

// --- INTERSEZIONI ---
float hit_sphere(vec3 center, float radius, vec3 ro, vec3 rd) {
    vec3 oc = ro - center;
    float a = dot(rd, rd);
    float half_b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = half_b * half_b - a * c;
    if (discriminant < 0.0) return -1.0;
    float sqrt_d = sqrt(discriminant);
    float t = (-half_b - sqrt_d) / a;
    if (t < EPSILON) t = (-half_b + sqrt_d) / a;
    if (t < EPSILON) return -1.0;
    return t;
}

float hit_triangle_math(vec3 v0, vec3 v1, vec3 v2, vec3 ro, vec3 rd) {
    vec3 edge1 = v1 - v0; 
    vec3 edge2 = v2 - v0; 
    vec3 h = cross(rd, edge2);
    float a = dot(edge1, h);
    if (abs(a) < 0.00001) return -1.0;
    float f = 1.0 / a; 
    vec3 s = ro - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return -1.0;
    vec3 q = cross(s, edge1);
    float v = f * dot(rd, q);
    if (v < 0.0 || u + v > 1.0) return -1.0;
    float t = f * dot(edge2, q);
    if (t > EPSILON) return t;
    return -1.0;
}

HitRecord trace_world(vec3 ro, vec3 rd) {
    HitRecord hit; 
    hit.t = MAX_DIST; 
    hit.index = -1; 
    hit.type = 0; 
    
    for(int i = 0; i < u_num_spheres; i++) {
        float t = hit_sphere(u_spheres[i].center, u_spheres[i].radius, ro, rd);
        if (t > 0.0 && t < hit.t) { 
            hit.t = t; 
            hit.index = i; 
            hit.type = 1; 
        }
    }
    for(int i = 0; i < u_num_triangles; i++) {
        float t = hit_triangle_math(u_triangles[i].v0, u_triangles[i].v1, u_triangles[i].v2, ro, rd);
        if (t > 0.0 && t < hit.t) { 
            hit.t = t; 
            hit.index = i; 
            hit.type = 2; 
        }
    }
    
    if (hit.index != -1) {
        hit.p = ro + hit.t * rd;
        if (hit.type == 1) {
            hit.normal = normalize(hit.p - u_spheres[hit.index].center);
        } else {
            vec3 e1 = u_triangles[hit.index].v1 - u_triangles[hit.index].v0;
            vec3 e2 = u_triangles[hit.index].v2 - u_triangles[hit.index].v0;
            hit.normal = normalize(cross(e1, e2));
        }
    }
    return hit;
}

float schlick(float cosine, float ref_idx) {
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx); 
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    
    // Inizializza RNG migliorato
    init_rng(gl_FragCoord.xy, u_frame_index);

    // --- SETUP CAMERA ---
    vec3 origin = u_cam_origin;
    vec3 w = normalize(u_cam_origin - u_cam_lookat);
    vec3 u = normalize(cross(vec3(0, 1, 0), w)); 
    vec3 v = cross(w, u);

    float theta = radians(u_fov);
    float h = tan(theta / 2.0);
    float viewport_height = 2.0 * h * u_focus_dist;
    
    vec3 focus_point = origin - w * u_focus_dist + u * (uv.x * viewport_height) + v * (uv.y * viewport_height);

    vec2 rd_lens = random_in_unit_disk() * (u_aperture / 2.0);
    vec3 offset = u * rd_lens.x + v * rd_lens.y;

    vec3 ro = origin + offset;
    vec3 rd = normalize(focus_point - ro);

    // --- RENDER LOOP ---
    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    
    float rr_threshold = 0.1;
    
    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        if (bounce > 3) {
            float max_throughput = max(throughput.x, max(throughput.y, throughput.z));
            if (max_throughput < rr_threshold) {
                if (rand() > max_throughput / rr_threshold) {
                    break;
                }
                throughput /= (max_throughput / rr_threshold);
            }
        }
        
        HitRecord hit = trace_world(ro, rd);

        if (hit.index != -1) {
            vec3 matColor; 
            int matType; 
            float matParam;
            
            if (hit.type == 1) {
                matColor = u_spheres[hit.index].color / 255.0;
                matType = u_spheres[hit.index].material;
                matParam = u_spheres[hit.index].param;
            } else {
                matColor = u_triangles[hit.index].color / 255.0;
                matType = u_triangles[hit.index].material;
                matParam = u_triangles[hit.index].param;
            }

            bool front_face = dot(rd, hit.normal) < 0.0;
            vec3 face_normal = front_face ? hit.normal : -hit.normal;

            vec3 attenuation = matColor;
            vec3 scatteredDir;
            bool didScatter = true;
            
            vec3 offsetPos = hit.p + face_normal * EPSILON;

            if (matType == 3) { // EMISSIVE
                vec3 emitted = attenuation * matParam;
                finalColor += emitted * throughput;
                break;
            }
            else if (matType == 0) { // LAMBERTIAN 
                scatteredDir = normalize(face_normal + random_in_unit_sphere());
                // Prevenzione direzioni degenerate
                if (dot(scatteredDir, face_normal) < 0.01) {
                    scatteredDir = face_normal;
                }
            }
            else if (matType == 1) { // METAL
                vec3 reflected = reflect(rd, face_normal);
                scatteredDir = normalize(reflected + random_in_unit_sphere() * matParam);
                if (dot(scatteredDir, face_normal) <= 0.0) didScatter = false;
            }
            else if (matType == 2) { // GLASS
                attenuation = vec3(1.0);
                float refraction_ratio = front_face ? (1.0 / matParam) : matParam;

                vec3 unit_dir = normalize(rd);
                float cos_theta = min(dot(-unit_dir, face_normal), 1.0);
                float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

                bool cannot_refract = refraction_ratio * sin_theta > 1.0;
                
                if (cannot_refract || rand() < schlick(cos_theta, refraction_ratio)) {
                    scatteredDir = reflect(unit_dir, face_normal);
                    offsetPos = hit.p + face_normal * EPSILON;
                } else {
                    scatteredDir = refract(unit_dir, face_normal, refraction_ratio);
                    offsetPos = hit.p - face_normal * EPSILON;
                }
            }

            if (didScatter) {
                throughput *= attenuation;
                ro = offsetPos;
                rd = scatteredDir;
            } else {
                break;
            }
        } else {
            // Sky
            float t = 0.5 * (rd.y + 1.0);
            vec3 sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            finalColor += sky * throughput;
            break;
        }
    }

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    // Accumulation
    float alpha = 1.0 / float(u_frame_index);
    FragColor = vec4(finalColor, alpha);
}