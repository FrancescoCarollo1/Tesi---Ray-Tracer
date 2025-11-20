#version 330 core
out vec4 FragColor;

// --- INPUT ---
uniform vec2 u_resolution;
uniform vec3 u_cam_origin;
uniform vec3 u_cam_lookat;
uniform float u_fov;
uniform float u_time;      
uniform int u_frame_index;

uniform int u_num_spheres;
uniform int u_num_triangles; // <--- NUOVO

// --- STRUTTURE DATI ---
struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
    int material; 
    float param;  
};

struct Triangle { // <--- NUOVO
    vec3 v0;
    vec3 v1;
    vec3 v2;
    vec3 color;
    int material;
    float param;
};

#define MAX_OBJ 64
uniform Sphere u_spheres[MAX_OBJ];
uniform Triangle u_triangles[MAX_OBJ]; // <--- Array Triangoli

struct HitRecord {
    float t;
    vec3 p;
    vec3 normal;
    int index;
    int type; // 0 = Nessuno, 1 = Sfera, 2 = Triangolo
};

// --- UTILS ---
float hash(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 random_in_unit_sphere(vec3 seed) {
    float phi = hash(seed.xy) * 6.28318;
    float costheta = hash(seed.yz) * 2.0 - 1.0;
    float u = hash(seed.zx);
    float r = pow(u, 0.3333);
    float theta = acos(costheta);
    return vec3(r * sin(theta) * cos(phi), r * sin(theta) * sin(phi), r * cos(theta));
}

// --- INTERSEZIONI ---

// Sfera
float hit_sphere(vec3 center, float radius, vec3 ro, vec3 rd) {
    vec3 oc = ro - center;
    float a = dot(rd, rd);
    float b = 2.0 * dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return -1.0;
    
    float t = (-b - sqrt(discriminant)) / (2.0 * a);
    if (t < 0.001) t = (-b + sqrt(discriminant)) / (2.0 * a);
    if (t < 0.001) return -1.0;
    return t;
}

// Triangolo (Möller–Trumbore)
float hit_triangle_math(vec3 v0, vec3 v1, vec3 v2, vec3 ro, vec3 rd) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(rd, edge2);
    float a = dot(edge1, h);
    
    if (a > -0.00001 && a < 0.00001) return -1.0; // Parallelo
    
    float f = 1.0 / a;
    vec3 s = ro - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return -1.0;
    
    vec3 q = cross(s, edge1);
    float v = f * dot(rd, q);
    if (v < 0.0 || u + v > 1.0) return -1.0;
    
    float t = f * dot(edge2, q);
    if (t > 0.001) return t;
    return -1.0;
}

// --- SCENE TRAVERSAL ---
HitRecord trace_world(vec3 ro, vec3 rd) {
    HitRecord hit;
    hit.t = 100000.0;
    hit.index = -1;
    hit.type = 0; 

    // 1. Controllo Sfere
    for(int i = 0; i < u_num_spheres; i++) {
        float t = hit_sphere(u_spheres[i].center, u_spheres[i].radius, ro, rd);
        if (t > 0.001 && t < hit.t) {
            hit.t = t;
            hit.index = i;
            hit.type = 1; // Sfera
        }
    }

    // 2. Controllo Triangoli
    for(int i = 0; i < u_num_triangles; i++) {
        float t = hit_triangle_math(u_triangles[i].v0, u_triangles[i].v1, u_triangles[i].v2, ro, rd);
        if (t > 0.001 && t < hit.t) {
            hit.t = t;
            hit.index = i;
            hit.type = 2; // Triangolo
        }
    }
    
    // Calcolo Normali
    if (hit.index != -1) {
        hit.p = ro + hit.t * rd;
        
        if (hit.type == 1) {
            // Normale Sfera
            hit.normal = normalize(hit.p - u_spheres[hit.index].center);
        } else if (hit.type == 2) {
            // Normale Triangolo (Flat Shading)
            vec3 e1 = u_triangles[hit.index].v1 - u_triangles[hit.index].v0;
            vec3 e2 = u_triangles[hit.index].v2 - u_triangles[hit.index].v0;
            hit.normal = normalize(cross(e1, e2));
        }
    }
    return hit;
}

// Schlick
float schlick(float cosine, float ref_idx) {
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;

    // Camera Setup
    vec3 ro = u_cam_origin;
    vec3 f = normalize(u_cam_lookat - ro);
    vec3 r = normalize(cross(vec3(0,1,0), f));
    vec3 u = cross(f, r);
    float zoom = 1.0 / tan(radians(u_fov) * 0.5);
    vec3 rd = normalize(f * zoom + r * uv.x + u * uv.y);

    // --- RENDER LOOP ---
    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 seed = vec3(uv, u_time + float(u_frame_index) * 0.1); 

    for (int bounce = 0; bounce < 8; bounce++) {
        HitRecord hit = trace_world(ro, rd);

        if (hit.index != -1) {
            // Recuperiamo i dati del materiale in base al tipo colpito
            vec3 matColor;
            int matType;
            float matParam;

            if (hit.type == 1) { // Sfera
                matColor = u_spheres[hit.index].color / 255.0;
                matType = u_spheres[hit.index].material;
                matParam = u_spheres[hit.index].param;
            } else { // Triangolo
                matColor = u_triangles[hit.index].color / 255.0;
                matType = u_triangles[hit.index].material;
                matParam = u_triangles[hit.index].param;
            }

            vec3 attenuation = matColor;
            vec3 emitted = vec3(0.0);
            seed += vec3(hit.p * float(bounce+1));

            vec3 scatteredDir;
            bool didScatter = true;
            vec3 offsetPos = hit.p; 

            // 3: EMISSIVE
            if (matType == 3) {
                emitted = attenuation * matParam;
                finalColor += emitted * throughput;
                break;
            }
            // 0: LAMBERTIAN
            else if (matType == 0) {
                scatteredDir = normalize(hit.normal + random_in_unit_sphere(seed));
                offsetPos = hit.p + hit.normal * 0.01;
            }
            // 1: METAL
            else if (matType == 1) {
                vec3 reflected = reflect(rd, hit.normal);
                scatteredDir = normalize(reflected + random_in_unit_sphere(seed) * matParam);
                if (dot(scatteredDir, hit.normal) <= 0.0) didScatter = false;
                offsetPos = hit.p + hit.normal * 0.01;
            }
            // 2: GLASS
            else if (matType == 2) {
                attenuation = vec3(1.0);
                float etai_over_etat = (dot(rd, hit.normal) > 0.0) ? matParam : (1.0 / matParam);
                vec3 n_eff = (dot(rd, hit.normal) > 0.0) ? -hit.normal : hit.normal;
                float cos_theta = min(dot(-rd, n_eff), 1.0);
                float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

                if (etai_over_etat * sin_theta > 1.0 || hash(seed.xy) < schlick(cos_theta, etai_over_etat)) {
                    scatteredDir = reflect(rd, n_eff);
                } else {
                    scatteredDir = refract(rd, n_eff, etai_over_etat);
                }
                offsetPos = hit.p + scatteredDir * 0.01; 
            }

            if (didScatter) {
                throughput *= attenuation;
                ro = offsetPos;
                rd = scatteredDir;
            } else {
                throughput = vec3(0.0);
                break;
            }
        } else {
            // Sky
            if (throughput.x > 0.0) { 
                float t = 0.5 * (rd.y + 1.0);
                vec3 sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
                // sky = vec3(0.0); // Scommenta per cielo nero
                finalColor += sky * throughput;
            }
            break;
        }
    }

    finalColor = pow(finalColor, vec3(1.0/2.2));
    float alpha = 1.0 / float(u_frame_index);
    FragColor = vec4(finalColor, alpha);
}