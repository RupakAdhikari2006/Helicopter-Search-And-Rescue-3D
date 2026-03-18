#include "terrain.h"
#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global data
float heightMap[GRID_SIZE][GRID_SIZE];
StrandedPerson people[NUM_PEOPLE];
TreeInstance trees[NUM_TREES];
RockInstance rocks[NUM_ROCKS];
BushInstance bushes[NUM_BUSHES];
RiverPoint riverPath[NUM_RIVER_PTS];
FuelStation fuelStations[NUM_FUEL_STATIONS];

static float waterTime = 0.0f;
static float campfireTime = 0.0f;

static unsigned int rng_state = 12345;
static float randf() {
    rng_state = rng_state * 1103515245 + 12345;
    return (float)((rng_state >> 16) & 0x7FFF) / 32767.0f;
}
static float randf_range(float lo, float hi) {
    return lo + randf() * (hi - lo);
}

static float mountainPeak(float x, float z, float cx, float cz, float height, float radius) {
    float dx = x - cx;
    float dz = z - cz;
    float dist = (float)sqrt(dx*dx + dz*dz);
    if (dist > radius) return 0.0f;
    float t = 1.0f - dist / radius;
    // Smoother cubic falloff
    return height * (3.0f * t * t - 2.0f * t * t * t);
}

static float hash(float x, float y) {
    long lx = *(long*)&x;
    long ly = *(long*)&y;
    long h = lx * 374761393 + ly * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return (float)(h & 0x7fffffff) / (float)0x7fffffff;
}

static float noise(float x, float y) {
    float ix = floor(x);
    float iy = floor(y);
    float fx = x - ix;
    float fy = y - iy;

    // smoothstep
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float a = hash(ix, iy);
    float b = hash(ix + 1.0f, iy);
    float c = hash(ix, iy + 1.0f);
    float d = hash(ix + 1.0f, iy + 1.0f);

    return a + (b - a)*ux + (c - a)*uy + (a - b - c + d)*ux*uy;
}

static float fbm(float x, float y, int octaves) {
    float v = 0.0f;
    float a = 0.5f;
    float shiftX = 100.0f;
    float shiftY = 100.0f;
    
    // Rotate to reduce axial artifacts
    float cosR = 0.8f; float sinR = 0.6f;
    
    for (int i = 0; i < octaves; ++i) {
        v += a * noise(x, y);
        float nx = (x * cosR - y * sinR) * 2.0f + shiftX;
        float ny = (x * sinR + y * cosR) * 2.0f + shiftY;
        x = nx; y = ny;
        a *= 0.5f;
    }
    return v;
}

static void generateHeightMap() {
    float half = (GRID_SIZE - 1) / 2.0f;
    
    for (int gz = 0; gz < GRID_SIZE; gz++) {
        for (int gx = 0; gx < GRID_SIZE; gx++) {
            float wx = (gx - half) * TERRAIN_SCALE;
            float wz = (gz - half) * TERRAIN_SCALE;
            
            float h = 0.0f;
            
            // 1. Regional biomes blending (N/S/E/W)
            float distN = -wz - (float)fabs(wx); if (distN < 0) distN = 0;
            float distS = wz - (float)fabs(wx); if (distS < 0) distS = 0;
            float distE = wx - (float)fabs(wz); if (distE < 0) distE = 0;
            float distW = -wx - (float)fabs(wz); if (distW < 0) distW = 0;
            
            float total = distN + distS + distE + distW + 10.0f;
            float wN = distN / total;
            float wS = distS / total;
            float wE = distE / total;
            float wW = distW / total;
            float wC = 10.0f / total;
            
            // 2. Base Noise for natural flow
            float n1 = fbm(wx * 0.02f, wz * 0.02f, 4);  // Large formations
            float n2 = fbm(wx * 0.06f, wz * 0.06f, 3);  // Medium details
            float n3 = fbm(wx * 0.15f, wz * 0.15f, 2);  // Craggy details

            // 3. Setup Biome characteristics - more prominent rugged hills
            
            // North: Higher, more rugged hills
            float hN = 1.0f + (n1 * 15.0f) + (n2 * 5.0f) + (n3 * 2.0f);
            if (hN > 15.0f) hN = 15.0f + (hN - 15.0f) * 0.3f;
            
            // South: Rolling hills with more depth
            float hS = 0.5f + (n1 * 10.0f) + (n2 * 3.0f);
            if (hS > 10.0f) hS = 10.0f + (hS - 10.0f) * 0.2f;
            
            // East: Higher plateaus and ridges
            float hE = 0.8f + (n1 * 12.0f) + (n2 * 4.0f);
            if (hE > 12.0f) hE = 12.0f + (hE - 12.0f) * 0.25f;
            
            // West: Deeper valleys and more varied hills
            float hW = 0.4f + (n1 * 8.0f) + (n2 * 3.0f);
            
            // 4. Blend the regions
            h = hN * wN + hS * wS + hE * wE + hW * wW + 0.5f * wC;

            // 5. Global Mask: Flatten edges into rolling meadows, flatten center for helipad
            float dC = (float)sqrt(wx*wx + wz*wz);
            if (dC > 75.0f) {
                float t = (dC - 75.0f) / 15.0f;
                if (t > 1.0f) t = 1.0f;
                // Blend toward a gentle meadow height (0.5) at the edges
                h = h * (1.0f - t) + 0.5f * t;
            } else if (dC < 12.0f) {
                // Flatten the very center for the helipad base area
                float blend = dC / 12.0f;
                h = h * blend + 0.15f * (1.0f - blend);
                if (h < 0.1f) h = 0.1f;
            }
            
            heightMap[gz][gx] = h;
        }
    }
}

float getTerrainHeight(float wx, float wz) {
    float half = (GRID_SIZE - 1) / 2.0f;
    float gxf = wx / TERRAIN_SCALE + half;
    float gzf = wz / TERRAIN_SCALE + half;
    
    // Exact grid alignment prevents floating objects on exact vertices
    int gx0 = (int)gxf; int gz0 = (int)gzf;
    if (gxf < 0.0f) gx0--; if (gzf < 0.0f) gz0--; // fix negative casting
    
    if (gx0 < 0) gx0 = 0; if (gz0 < 0) gz0 = 0;
    if (gx0 >= GRID_SIZE - 1) gx0 = GRID_SIZE - 2;
    if (gz0 >= GRID_SIZE - 1) gz0 = GRID_SIZE - 2;
    
    float fx = gxf - gx0; float fz = gzf - gz0;
    float h00 = heightMap[gz0][gx0]; float h10 = heightMap[gz0][gx0+1];
    float h01 = heightMap[gz0+1][gx0]; float h11 = heightMap[gz0+1][gx0+1];
    
    // Bilinear interpolation
    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fz;
}

float getTerrainSlope(float wx, float wz) {
    float hC = getTerrainHeight(wx, wz);
    float hX = getTerrainHeight(wx + 1.0f, wz);
    float hZ = getTerrainHeight(wx, wz + 1.0f);
    float dX = (float)fabs(hX - hC);
    float dZ = (float)fabs(hZ - hC);
    return (float)sqrt(dX*dX + dZ*dZ);
}

void initTerrain() {
    rng_state = 555;
    generateHeightMap();
    
    people[0].x =  30.0f; people[0].z = -55.0f; people[0].is_rescued = 0; // North
    people[1].x = -40.0f; people[1].z = -50.0f; people[1].is_rescued = 0; // North
    people[2].x =  45.0f; people[2].z =  45.0f; people[2].is_rescued = 0; // South
    people[3].x = -45.0f; people[3].z =  45.0f; people[3].is_rescued = 0; // South
    people[4].x =  60.0f; people[4].z =  10.0f; people[4].is_rescued = 0; // East
    people[5].x =  55.0f; people[5].z = -15.0f; people[5].is_rescued = 0; // East
    people[6].x = -50.0f; people[6].z =   5.0f; people[6].is_rescued = 0; // West
    people[7].x = -45.0f; people[7].z = -20.0f; people[7].is_rescued = 0; // West
    
    fuelStations[0] = (FuelStation){ 7.0f, 7.0f };
    fuelStations[1] = (FuelStation){ -45.0f, -35.0f };
    
    int ti = 0; rng_state = 999;
    int attempts = 0;
    while (ti < NUM_TREES && attempts < 50000) {
        attempts++;
        float tx = randf_range(-82.0f, 82.0f);
        float tz = randf_range(-82.0f, 82.0f);
        
        if (tx*tx + tz*tz < 14.0f*14.0f) continue; // Keep helipad clear
        
        float th = getTerrainHeight(tx, tz);
        // Spawn trees on low grassland up to high green hills
        if (th < 0.1f || th > 18.0f) continue; 
        
        // SLOPE CHECK: Prevent floating on sheer cliffs
        float slope = getTerrainSlope(tx, tz);
        if (slope > 1.2f) continue; // Skip steep hills!
        
        trees[ti].x = tx; trees[ti].z = tz;
        
        if (tz < -(float)fabs(tx)) {
           // North (Snowy pines)
           trees[ti].type = 0; // pine
           trees[ti].scale = randf_range(0.85f, 1.45f);
        } else if (tz > (float)fabs(tx)) {
           // South (Majestic Oaks - The Shire)
           trees[ti].type = 1; // oak
           trees[ti].scale = randf_range(1.1f, 1.8f);
        } else if (tx > (float)fabs(tz)) {
           // East (Sparse terrain)
           trees[ti].type = 0; 
           trees[ti].scale = randf_range(0.4f, 0.7f); 
        } else {
           // West (Dead/marsh trees)
           trees[ti].type = 3; 
           trees[ti].scale = randf_range(0.85f, 1.45f);
        }
        ti++;
    }
    
    // Fill remaining empty slots to avoid uninitialized memory rendering to 0,0
    while(ti < NUM_TREES) { trees[ti].x = 1000.0f; trees[ti].z = 1000.0f; ti++; }
    
    attempts = 0;
    for (int i=0; i<NUM_ROCKS; i++) {
        while(attempts < 5000) {
            attempts++;
            rocks[i].x = randf_range(-82.0f, 82.0f);
            rocks[i].z = randf_range(-82.0f, 82.0f);
            float slope = getTerrainSlope(rocks[i].x, rocks[i].z);
            if (slope < 1.5f && getTerrainHeight(rocks[i].x, rocks[i].z) > 0.0f) break;
        }
        rocks[i].scale = randf_range(0.6f, 2.5f);
        rocks[i].rot = randf_range(0, 360);
    }
    
    attempts = 0;
    for (int i=0; i<NUM_BUSHES; i++) {
        while(attempts < 5000) {
            attempts++;
            bushes[i].x = randf_range(-82.0f, 82.0f);
            bushes[i].z = randf_range(-82.0f, 82.0f);
            float slope = getTerrainSlope(bushes[i].x, bushes[i].z);
            if (slope < 1.0f && getTerrainHeight(bushes[i].x, bushes[i].z) > 0.2f) break;
        }
        bushes[i].scale = randf_range(0.5f, 1.0f);
    }
}

// Draw static helpers
static void drawPineTree(float x, float z, float scale, int biomeID) {
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z); glScalef(scale, scale, scale);
    float m_t[] = { 0.35f, 0.18f, 0.12f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_t);
    glPushMatrix(); glTranslatef(0, 0.5f, 0); glScalef(0.18f, 1.2f, 0.18f); glutSolidCube(1.0); glPopMatrix();
    
    // Default Pine
    float m_l[] = { 0.10f, 0.45f, 0.15f, 1.0f }; // Standard deep forest green
    if (biomeID == 0) { m_l[0]=0.05f; m_l[1]=0.3f; m_l[2]=0.1f; } // Very dark, cold evergreen
    else if (biomeID == 2) { m_l[0]=0.15f; m_l[1]=0.55f; m_l[2]=0.2f; } // Lighter highland pine
    
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_l);
    for(int i=0; i<3; i++) {
        glPushMatrix(); glTranslatef(0, 0.9f + i*0.65f, 0); glRotatef(-90, 1,0,0);
        glutSolidCone(0.85f - i*0.25f, 1.6f - i*0.35f, 12, 1); glPopMatrix();
    }
    glPopMatrix();
}

static void drawOakTree(float x, float z, float scale, int biomeID) {
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z); glScalef(scale, scale, scale);
    float m_t[] = { 0.4f, 0.25f, 0.18f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_t);
    glPushMatrix(); glTranslatef(0, 0.7f, 0); glScalef(0.28f, 1.4f, 0.28f); glutSolidCube(1.0); glPopMatrix();
    
    // Default Oak
    float m_l[] = { 0.15f, 0.85f, 0.15f, 1.0f }; // Bright Neon Green standard
    if (biomeID == 1) { m_l[0]=0.0f; m_l[1]=0.95f; m_l[2]=0.2f; } // Extremely vibrant tropical
    
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_l);
    glPushMatrix(); glTranslatef(0, 2.0f, 0); glutSolidSphere(1.15f, 14, 14); glPopMatrix();
    glPopMatrix();
}

static void drawBirchTree(float x, float z, float scale, int biomeID) {
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z); glScalef(scale, scale, scale);
    float m_t[] = { 0.92f, 0.92f, 0.88f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_t);
    glPushMatrix(); glTranslatef(0, 0.8f, 0); glScalef(0.12f, 1.8f, 0.12f); glutSolidCube(1.0); glPopMatrix();
    
    float m_l[] = { 0.25f, 0.65f, 0.15f, 1.0f }; // Nice, saturated bright green leaves
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_l);
    glPushMatrix(); glTranslatef(0, 2.2f, 0); glutSolidSphere(0.7f, 12, 12); glPopMatrix();
    glPopMatrix();
}

static void drawDeadTree(float x, float z, float scale, int biomeID) {
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z); glScalef(scale, scale, scale);
    float m_t[] = { 0.25f, 0.35f, 0.1f, 1.0f }; // Mossy dark green/brown wood
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_t);
    glPushMatrix(); glTranslatef(0, 1.0f, 0); glScalef(0.18f, 2.0f, 0.18f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(0.2f, 1.5f, 0.0f); glRotatef(45, 0,0,1); glScalef(0.08f, 0.8f, 0.08f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.2f, 1.2f, 0.2f); glRotatef(-45, 1,0,1); glScalef(0.08f, 0.6f, 0.08f); glutSolidCube(1.0); glPopMatrix();
    glPopMatrix();
}

static void drawRock(RockInstance* r) {
    float h = getTerrainHeight(r->x, r->z);
    glPushMatrix(); glTranslatef(r->x, h + r->scale*0.25f, r->z);
    glRotatef(r->rot, 0,1,0); glScalef(r->scale, r->scale*0.85f, r->scale*1.15f);
    float m[] = { 0.55f, 0.55f, 0.52f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m);
    glutSolidSphere(1.0, 10, 10); glPopMatrix();
}

static void drawBush(BushInstance* b) {
    float h = getTerrainHeight(b->x, b->z);
    glPushMatrix(); glTranslatef(b->x, h + b->scale*0.35f, b->z);
    float m[] = { 0.18f, 0.45f, 0.12f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m);
    glutSolidSphere(b->scale, 10, 10); glPopMatrix();
}

static void drawPerson(float x, float z, int wave) {
    // Draw a Dwarf (Shorter, wider, earthy colors)
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h + 0.45f, z); // Shorter height
    float sk[] = { 0.95f, 0.8f, 0.65f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, sk);
    glPushMatrix(); glTranslatef(0, 0.6f, 0); glutSolidSphere(0.2, 12, 12); glPopMatrix(); // Larger head
    
    // Tunic (Browns/Greys)
    float sh[] = { 0.4f, 0.3f, 0.2f, 1.0f }; 
    if (wave) { sh[0]=0.2f; sh[1]=0.4f; sh[2]=0.2f; } // Some have green cloaks
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, sh);
    glPushMatrix(); glTranslatef(0, 0.25f, 0); glScalef(0.45f, 0.5f, 0.35f); glutSolidCube(1.0); glPopMatrix(); // Wider body
    
    // Legs
    float pa[] = { 0.2f, 0.2f, 0.2f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, pa);
    glPushMatrix(); glTranslatef(-0.15f, -0.15f, 0); glScalef(0.18f, 0.4f, 0.2f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(0.15f, -0.15f, 0); glScalef(0.18f, 0.4f, 0.2f); glutSolidCube(1.0); glPopMatrix();
    
    // Arms (waving)
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, sh);
    glPushMatrix(); glTranslatef(-0.35f, 0.3f, 0); 
    if(wave) glRotatef(30+(float)sin(waterTime*5)*30, 0,0,1); else glRotatef(140, 0,0,1);
    glScalef(0.15f, 0.45f, 0.15f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(0.35f, 0.3f, 0); 
    if(wave) glRotatef(-30-(float)sin(waterTime*5)*30, 0,0,1); else glRotatef(-140, 0,0,1);
    glScalef(0.15f, 0.45f, 0.15f); glutSolidCube(1.0); glPopMatrix();
    glPopMatrix();
}

static void drawCampfire(float x, float z) {
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z);
    float lo[] = { 0.28f, 0.14f, 0.08f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lo);
    for(int i=0; i<6; i++) {
        glPushMatrix(); glRotatef(i*60, 0,1,0); glTranslatef(0.35f, 0.08f, 0); glScalef(0.18f, 0.12f, 0.12f); glutSolidCube(1.0); glPopMatrix();
    }
    float b = 0.75f + 0.25f * (float)sin(campfireTime*12);
    float fi[] = { 1.0f*b, 0.4f*b, 0, 1 }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, fi);
    float em[] = { 0.6f*b, 0.15f*b, 0, 1 }; glMaterialfv(GL_FRONT, GL_EMISSION, em);
    glPushMatrix(); glTranslatef(0, 0.25f, 0); glutSolidSphere(0.18, 10, 10); glPopMatrix();
    float ne[] = {0,0,0,1}; glMaterialfv(GL_FRONT, GL_EMISSION, ne);
    glPopMatrix();
}

static void drawCabin(float x, float z) {
    // Draw a Hobbit Hole
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z);
    
    // Green grassy hill base
    float gr[] = { 0.2f, 0.6f, 0.15f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gr);
    glPushMatrix(); glTranslatef(0, -0.5f, 0); glScalef(4.0f, 2.5f, 3.5f); glutSolidSphere(1.0, 16, 16); glPopMatrix();
    
    // Round wooden door
    float wd[] = { 0.35f, 0.2f, 0.1f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, wd);
    glPushMatrix(); glTranslatef(0, 0.8f, 3.4f); glScalef(1.2f, 1.2f, 0.1f); glutSolidSphere(1.0, 12, 12); glPopMatrix();
    
    // Brass Doorknob
    float br[] = { 0.8f, 0.7f, 0.2f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, br);
    glPushMatrix(); glTranslatef(0.0f, 0.8f, 3.55f); glutSolidSphere(0.15, 8, 8); glPopMatrix();
    
    glPopMatrix();
}

static void drawFuelStation(float x, float z) {
    // Draw a realistic Fuel/Gas Station
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h, z);
    
    // --- Concrete base pad ---
    float pad[] = { 0.72f, 0.72f, 0.70f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, pad);
    glPushMatrix(); glTranslatef(0, 0.05f, 0); glScalef(5.0f, 0.1f, 4.0f); glutSolidCube(1.0); glPopMatrix();
    
    // --- Canopy support pillars (2 pillars, red/white) ---
    float pillar[] = { 0.85f, 0.12f, 0.12f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, pillar);
    glPushMatrix(); glTranslatef(-1.6f, 1.2f, 0); glScalef(0.18f, 2.4f, 0.18f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef( 1.6f, 1.2f, 0); glScalef(0.18f, 2.4f, 0.18f); glutSolidCube(1.0); glPopMatrix();
    
    // --- Canopy roof ---
    float roof[] = { 0.90f, 0.90f, 0.90f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, roof);
    glPushMatrix(); glTranslatef(0, 2.45f, 0); glScalef(4.2f, 0.18f, 3.0f); glutSolidCube(1.0); glPopMatrix();
    // Canopy red trim
    float trim[] = { 0.85f, 0.12f, 0.12f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, trim);
    glPushMatrix(); glTranslatef(0, 2.38f, 0); glScalef(4.3f, 0.08f, 3.1f); glutSolidCube(1.0); glPopMatrix();
    
    // --- Fuel Pump body ---
    float pump[] = { 0.15f, 0.50f, 0.20f, 1.0f }; // Green pump body
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, pump);
    glPushMatrix(); glTranslatef(0, 0.75f, 0); glScalef(0.6f, 1.5f, 0.4f); glutSolidCube(1.0); glPopMatrix();
    
    // Pump top display screen (dark)
    float screen[] = { 0.05f, 0.05f, 0.15f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, screen);
    glPushMatrix(); glTranslatef(0, 1.15f, 0.21f); glScalef(0.38f, 0.35f, 0.05f); glutSolidCube(1.0); glPopMatrix();
    
    // Pump nozzle hose (dark grey pipe)
    float hose[] = { 0.25f, 0.25f, 0.25f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, hose);
    glPushMatrix(); glTranslatef(0.38f, 0.8f, 0.1f); glScalef(0.08f, 0.6f, 0.08f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(0.55f, 0.52f, 0.1f); glRotatef(45,0,0,1); glScalef(0.08f, 0.35f, 0.08f); glutSolidCube(1.0); glPopMatrix();
    
    // --- Price sign pole ---
    float signpole[] = { 0.6f, 0.6f, 0.6f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, signpole);
    glPushMatrix(); glTranslatef(2.2f, 1.0f, 0); glScalef(0.1f, 2.0f, 0.1f); glutSolidCube(1.0); glPopMatrix();
    // Sign board (yellow)
    float sign[] = { 0.95f, 0.80f, 0.05f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, sign);
    glPushMatrix(); glTranslatef(2.2f, 2.2f, 0); glScalef(0.8f, 0.5f, 0.1f); glutSolidCube(1.0); glPopMatrix();
    
    glPopMatrix();
}

static void drawHelipad(float x, float z) {
    // Dwarven Landing Rune (Stone with glowing inset)
    float h = getTerrainHeight(x, z);
    glPushMatrix(); glTranslatef(x, h + 0.04f, z);
    
    float st[] = { 0.4f, 0.4f, 0.4f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, st);
    // Octagon base
    glBegin(GL_POLYGON);
    glNormal3f(0,1,0); 
    for(int i=0; i<8; i++) {
        float a = i * M_PI / 4.0f;
        glVertex3f(4.0f * cos(a), 0, 4.0f * sin(a));
    }
    glEnd();
    
    // Glowing dwarven gold rune (Diamond shape)
    float go[] = { 1.0f, 0.8f, 0.2f, 1.0f }; glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, go);
    glBegin(GL_QUADS); glNormal3f(0,1,0); 
    glVertex3f(0, 0.02f, -2.5f); glVertex3f(2.5f, 0.02f, 0); 
    glVertex3f(0, 0.02f, 2.5f); glVertex3f(-2.5f, 0.02f, 0);
    glEnd(); 
    
    glPopMatrix();
}

/* Ocean removed - replaced with green landscape */

static void drawSky() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(-1,1,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glBegin(GL_QUADS);
    // Deep blue sky at top, warm cream-green horizon (English countryside summer)
    glColor3f(0.38f, 0.68f, 0.98f); glVertex2f(-1, 1); glVertex2f(1, 1);
    glColor3f(0.80f, 0.95f, 0.78f); glVertex2f(1,-1); glVertex2f(-1,-1);
    glEnd();
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

static void computeTriangleNormal(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, float* nx, float* ny, float* nz) {
    float ux = x1-x0, uy = y1-y0, uz = z1-z0;
    float vx = x2-x0, vy = y2-y0, vz = z2-z0;
    *nx = uy*vz - uz*vy; *ny = uz*vx - ux*vz; *nz = ux*vy - uy*vx;
    float len = (float)sqrt((*nx)*(*nx) + (*ny)*(*ny) + (*nz)*(*nz));
    if (len > 0.0001f) { *nx /= len; *ny /= len; *nz /= len; }
    else { *nx = 0; *ny = 1; *nz = 0; }
}

void drawTerrain() {
    waterTime += 0.016f; campfireTime += 0.016f;
    drawSky(); 
    /* No ocean - green landscape replaces it */
    
    float hf = (GRID_SIZE - 1) / 2.0f;
    
    // Disable blending for solid opaque terrain
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // Terrain faces have downward normals due to winding - disable back-face culling
    glDisable(GL_CULL_FACE);
    float no_spec[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, no_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    
    for (int gz = 0; gz < GRID_SIZE - 1; gz++) {
        for (int gx = 0; gx < GRID_SIZE - 1; gx++) {
            float x0 = (gx - hf)*TERRAIN_SCALE, z0 = (gz - hf)*TERRAIN_SCALE;
            float x1 = (gx+1-hf)*TERRAIN_SCALE, z1 = (gz+1-hf)*TERRAIN_SCALE;
            float h00 = heightMap[gz][gx], h10 = heightMap[gz][gx+1], h01 = heightMap[gz+1][gx], h11 = heightMap[gz+1][gx+1];
            
            float avgH = (h00+h10+h01+h11)*0.25f;
            if (avgH < -2.0f) continue; // skip only deeply underground patches

            float nx, ny, nz;
            
            // Triangle 1
            computeTriangleNormal(x0, h00, z0, x1, h10, z0, x0, h01, z1, &nx, &ny, &nz);
            
            // Natural earth tones - realistic countryside colors
            float mat[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            float t;
            if (avgH < 0.3f) {
                // Flat lowland - rich dark soil green
                mat[0]=0.28f; mat[1]=0.42f; mat[2]=0.14f;
            } else if (avgH < 1.5f) {
                // Natural meadow grass
                t = (avgH - 0.3f) / 1.2f;
                mat[0]=0.30f-t*0.03f; mat[1]=0.48f+t*0.04f; mat[2]=0.16f-t*0.02f;
            } else if (avgH < 3.0f) {
                // Lush hillside green
                t = (avgH - 1.5f) / 1.5f;
                mat[0]=0.27f-t*0.04f; mat[1]=0.52f-t*0.06f; mat[2]=0.14f-t*0.02f;
            } else if (avgH < 5.0f) {
                // Olive-green upper slopes
                t = (avgH - 3.0f) / 2.0f;
                mat[0]=0.23f+t*0.08f; mat[1]=0.46f-t*0.08f; mat[2]=0.12f-t*0.02f;
            } else {
                // High hilltop - warm earthy grass
                t = (avgH - 5.0f) / 2.5f; if (t > 1.0f) t = 1.0f;
                mat[0]=0.31f+t*0.12f; mat[1]=0.38f+t*0.08f; mat[2]=0.10f+t*0.04f;
            }
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat);
            
            glBegin(GL_TRIANGLES);
            // Negate normals so they point UP toward the sun (correct lighting)
            glNormal3f(-nx, -ny, -nz);
            glVertex3f(x0, h00, z0); glVertex3f(x1, h10, z0); glVertex3f(x0, h01, z1);
            
            // Triangle 2
            computeTriangleNormal(x1, h10, z0, x1, h11, z1, x0, h01, z1, &nx, &ny, &nz);
            glNormal3f(-nx, -ny, -nz);
            glVertex3f(x1, h10, z0); glVertex3f(x1, h11, z1); glVertex3f(x0, h01, z1);
            glEnd();
        }
    }
    
    // Restore face culling for other objects (helicopter etc.)
    glEnable(GL_CULL_FACE);
    
    drawHelipad(0,0); drawCabin(8.5f, -6.5f);
    for(int i=0; i<NUM_FUEL_STATIONS; i++) drawFuelStation(fuelStations[i].x, fuelStations[i].z);
    for(int i=0; i<NUM_TREES; i++) {
        int biomeID = 0; // default North
        // Reverse-engineer biome from pos just for drawing styles
        if (trees[i].z > (float)fabs(trees[i].x)) biomeID = 1; // South
        else if (trees[i].x > (float)fabs(trees[i].z)) biomeID = 2; // East
        else if (trees[i].x < -(float)fabs(trees[i].z)) biomeID = 3; // West
        
        if(trees[i].type==0) drawPineTree(trees[i].x, trees[i].z, trees[i].scale, biomeID);
        else if(trees[i].type==1) drawOakTree(trees[i].x, trees[i].z, trees[i].scale, biomeID);
        else if(trees[i].type==2) drawBirchTree(trees[i].x, trees[i].z, trees[i].scale, biomeID);
        else drawDeadTree(trees[i].x, trees[i].z, trees[i].scale, biomeID);
    }
    for(int i=0; i<NUM_ROCKS; i++) drawRock(&rocks[i]);
    for(int i=0; i<NUM_BUSHES; i++) drawBush(&bushes[i]);
    for(int i=0; i<NUM_PEOPLE; i++) {
        if(!people[i].is_rescued) {
            drawPerson(people[i].x, people[i].z, i%2);
            if(i%2==0) drawCampfire(people[i].x+3.0f, people[i].z+1.5f);
        }
    }
}
