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

// --- STRUTTURA SFERA ---
struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
    int material; // 0=Diff, 1=Met, 2=Glass, 3=Light
    float param;  
};

// Limite Array 
#define MAX_SPHERES 128
uniform Sphere u_spheres[MAX_SPHERES];

struct HitRecord {
    float t;
    vec3 p;
    vec3 normal;
    int index;
};

// --- RNG ---
// Un hash function più robusta per evitare pattern visivi (Gold Noise)
float hash(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 random_in_unit_sphere(vec3 seed) {
    float phi = hash(seed.xy) * 6.28318;
    float costheta = hash(seed.yz) * 2.0 - 1.0;
    float u = hash(seed.zx);
    
    float theta = acos(costheta);
    float r = pow(u, 0.3333);
    
    return vec3(r * sin(theta) * cos(phi), r * sin(theta) * sin(phi), r * cos(theta));
}

// --- INTERSEZIONE SFERA ---
float hit_sphere(vec3 center, float radius, vec3 ro, vec3 rd) {
    vec3 oc = ro - center;
    float a = dot(rd, rd);
    float b = 2.0 * dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0.0) return -1.0;
    
    // FIX SHADOW ACNE:
    float min_t = 0.01; 

    float t = (-b - sqrt(discriminant)) / (2.0 * a);
    if (t < min_t) t = (-b + sqrt(discriminant)) / (2.0 * a);
    if (t < min_t) return -1.0;
    
    return t;
}

HitRecord trace_world(vec3 ro, vec3 rd) {
    HitRecord hit;
    hit.t = 100000.0; // Infinito
    hit.index = -1;
    
    for(int i = 0; i < u_num_spheres; i++) {
        float t = hit_sphere(u_spheres[i].center, u_spheres[i].radius, ro, rd);
        if (t > 0.0 && t < hit.t) {
            hit.t = t;
            hit.index = i;
        }
    }
    if (hit.index != -1) {
        hit.p = ro + hit.t * rd;
        hit.normal = normalize(hit.p - u_spheres[hit.index].center);
    }
    return hit;
}

// Schlick approximation for Glass
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

    // Path Tracing Loop
    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    
    // SEED INIZIALE:
    vec3 seed = vec3(gl_FragCoord.xy, u_time + float(u_frame_index) * 1.618);

    for (int bounce = 0; bounce < 50; bounce++) {
        HitRecord hit = trace_world(ro, rd);

        if (hit.index != -1) {
            Sphere obj = u_spheres[hit.index];
            vec3 attenuation = obj.color / 255.0;
            vec3 emitted = vec3(0.0);
            
            // Aggiorniamo il seed in modo caotico ad ogni rimbalzo
            seed = vec3(hash(seed.xy), hash(seed.yz), hash(seed.zx)) + float(bounce);

            vec3 scatteredDir;
            bool didScatter = true;
            vec3 offsetPos = hit.p;

            // 3: EMISSIVE
            if (obj.material == 3) {
                emitted = attenuation * obj.param;
                finalColor += emitted * throughput;
                break; 
            }
            // 0: LAMBERTIAN
            else if (obj.material == 0) {
                vec3 randVec = random_in_unit_sphere(seed);
                scatteredDir = normalize(hit.normal + randVec);
                // Spingiamo fuori lungo la normale per evitare acne
                offsetPos = hit.p + hit.normal * 0.01; 
            }
            // 1: METAL
            else if (obj.material == 1) {
                vec3 reflected = reflect(rd, hit.normal);
                scatteredDir = normalize(reflected + random_in_unit_sphere(seed) * obj.param);
                if (dot(scatteredDir, hit.normal) <= 0.0) didScatter = false;
                offsetPos = hit.p + hit.normal * 0.01;
            }
            // 2: GLASS
            else if (obj.material == 2) {
                attenuation = vec3(1.0);
                float etai_over_etat = (dot(rd, hit.normal) > 0.0) ? obj.param : (1.0 / obj.param);
                vec3 n_eff = (dot(rd, hit.normal) > 0.0) ? -hit.normal : hit.normal;
                float cos_theta = min(dot(-rd, n_eff), 1.0);
                float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

                if (etai_over_etat * sin_theta > 1.0 || hash(seed.xy) < schlick(cos_theta, etai_over_etat)) {
                    scatteredDir = reflect(rd, n_eff);
                } else {
                    scatteredDir = refract(rd, n_eff, etai_over_etat);
                }
                
                // Per il vetro, spingiamo lungo il raggio stesso
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
            float t = 0.5 * (rd.y + 1.0);
            vec3 sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            
            // Sfondo nero
            // sky = vec3(0.0); 
            
            finalColor += sky * throughput;
            break;
        }
    }

    // Gamma Correction
    finalColor = pow(finalColor, vec3(1.0/2.2));

    // Accumulo Blending
    float alpha = 1.0 / float(u_frame_index);
    FragColor = vec4(finalColor, alpha);
}