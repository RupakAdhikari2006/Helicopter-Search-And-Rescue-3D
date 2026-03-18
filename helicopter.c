#include "helicopter.h"
#include "terrain.h"
#include <GL/glut.h>
#include <math.h>

#define PI 3.14159265358979323846

void initHelicopter(Helicopter* heli) {
    heli->x = 0.0f;
    heli->y = 0.5f;
    heli->z = 0.0f;
    heli->rot_y = 0.0f;
    heli->rot_x = 0.0f;
    heli->rot_z = 0.0f;
    
    heli->main_rotor_angle = 0.0f;
    heli->tail_rotor_angle = 0.0f;
    
    heli->rotor_speed = 0.0f;
    heli->engine_on = 0;
    
    heli->velocity_ydir = 0.0f;
    heli->velocity_fwd = 0.0f;
    heli->velocity_side = 0.0f;
    heli->passengers = 0;
    heli->total_targets = 5;
    
    heli->fuel = 100.0f;
    heli->refuels_left = 2;
    heli->game_time = 0.0f;
    heli->time_remaining = 360.0f; // Default 6 mins
}

void updateHelicopter(Helicopter* heli, float dt) {
    // Engine and rotor logic
    if (heli->engine_on == 1) {
        heli->rotor_speed += 10.0f * dt;
        if (heli->rotor_speed >= 40.0f) heli->engine_on = 2;
    } else if (heli->engine_on == 3) {
        heli->rotor_speed -= 5.0f * dt;
        if (heli->rotor_speed <= 0.0f) {
            heli->rotor_speed = 0.0f;
            heli->engine_on = 0;
        }
    }
    
    heli->main_rotor_angle += heli->rotor_speed * dt * 20.0f;
    if (heli->main_rotor_angle > 360.0f) heli->main_rotor_angle -= 360.0f;
    
    heli->tail_rotor_angle += heli->rotor_speed * dt * 25.0f;
    if (heli->tail_rotor_angle > 360.0f) heli->tail_rotor_angle -= 360.0f;

    // Movement physics
    float heading_rad = heli->rot_y * PI / 180.0f;
    heli->x += (float)sin(heading_rad) * heli->velocity_fwd * dt;
    heli->z += (float)cos(heading_rad) * heli->velocity_fwd * dt;
    heli->y += heli->velocity_ydir * dt;
    
    // Auto-stabilize
    heli->rot_x *= 0.90f;
    heli->rot_z *= 0.90f;
    
    // Friction/drag
    heli->velocity_fwd *= 0.95f;
    heli->velocity_ydir *= 0.92f;

    // Terrain-aware ground collision
    float groundH = getTerrainHeight(heli->x, heli->z);
    float minY = groundH + 0.5f;
    if (heli->y < minY) {
        heli->y = minY;
        heli->velocity_ydir = 0.0f;
        heli->rot_x = 0.0f;
        heli->rot_z = 0.0f;
    }
    
    // Fuel depletion while engine running
    if (heli->engine_on == 2) {
        heli->fuel -= 1.2f * dt; // Drains slowly
        if (heli->fuel < 0.0f) heli->fuel = 0.0f;
        
        // Out of fuel — engine dies
        if (heli->fuel <= 0.0f) {
            heli->engine_on = 3;
        }
    }
    
    // Mission timer (only while engine is on)
    if (heli->engine_on >= 1 && heli->engine_on <= 2) {
        heli->game_time += dt;
    }
}

// Helper: draw cylinder
static void drawCylinder(float radius, float length) {
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, radius, radius, length, 16, 1);
    gluDeleteQuadric(quad);
}

void drawHelicopter(Helicopter* heli) {
    glPushMatrix();
    
    glTranslatef(heli->x, heli->y, heli->z);
    glRotatef(heli->rot_y, 0, 1, 0);
    glRotatef(heli->rot_x, 1, 0, 0);
    glRotatef(heli->rot_z, 0, 0, 1);

    // Main Body — Bronze/Steampunk Gyrocopter
    float mat_amb_body[] = { 0.25f, 0.15f, 0.05f, 1.0f }; // Dark bronze ambient
    float mat_diff_body[] = { 0.55f, 0.35f, 0.15f, 1.0f }; // Shiny bronze diffuse
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_amb_body);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_body);
    
    // Barrel-like rounded body
    glPushMatrix();
    glScalef(1.1f, 0.9f, 1.3f);
    glutSolidSphere(1.0, 24, 24);
    glPopMatrix();
    
    // Engine boiler (Golden backpiece)
    float mat_diff_gold[] = { 0.8f, 0.6f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_gold);
    glPushMatrix();
    glTranslatef(0.0f, 0.2f, -1.0f);
    glScalef(0.6f, 0.6f, 0.8f);
    glutSolidSphere(1.0, 16, 16);
    glPopMatrix();
    
    // Cockpit windshield (Brass piping / thick glass)
    float mat_glass[] = { 0.3f, 0.6f, 0.7f, 0.5f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_glass);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.8f);
    glScalef(0.7f, 0.6f, 0.5f);
    glutSolidSphere(1.0, 16, 16);
    glPopMatrix();
    glDisable(GL_BLEND);
    
    // Tail Boom - Wooden log/shaft
    float mat_amb_wood[] = { 0.2f, 0.1f, 0.05f, 1.0f };
    float mat_diff_wood[] = { 0.4f, 0.2f, 0.1f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_amb_wood);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_wood);

    glPushMatrix();
    glTranslatef(0, 0.2f, -1.2f);
    drawCylinder(0.12f, 1.8f);
    glPopMatrix();
    
    // Tail vertical fin - Wooden fin
    glPushMatrix();
    glTranslatef(0, 0.4f, -2.8f);
    glRotatef(-20.0f, 1, 0, 0);
    glScalef(0.08f, 1.2f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Horizontal stabilizer - Double wooden wings
    glPushMatrix();
    glTranslatef(0, 0.3f, -2.5f);
    glScalef(1.5f, 0.08f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Rotor mast - Thick iron/bronze pole
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_body);
    glPushMatrix();
    glTranslatef(0, 0.8f, 0);
    glRotatef(-90, 1, 0, 0);
    drawCylinder(0.15f, 0.4f);
    glPopMatrix();
    
    // Main Rotor Blades (Steampunk Canvas Wings)
    glPushMatrix();
    glTranslatef(0, 1.2f, 0);
    glRotatef(heli->main_rotor_angle, 0, 1, 0);
    
    float mat_spec[] = { 1, 1, 1, 1 };
    float mat_diff_rotor[] = { 0.8f, 0.7f, 0.5f, 1 }; // Canvas color
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_rotor);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 20.0f);
    
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glRotatef(90.0f * i, 0, 1, 0);
        glTranslatef(0, 0, 1.6f);
        // Canvas stretched over wooden frame
        glScalef(0.4f, 0.05f, 3.2f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
    
    // Tail Rotor (Wooden propeller)
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_amb_wood);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_wood);
    
    glPushMatrix();
    glTranslatef(0.15f, 0.5f, -2.8f);
    glRotatef(heli->tail_rotor_angle, 1, 0, 0);
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glRotatef(90.0f * i, 1, 0, 0);
        glTranslatef(0, 0.5f, 0);
        glScalef(0.08f, 1.0f, 0.15f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
    
    // Landing Skids (Iron sleds)
    float mat_iron[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_iron);
    glPushMatrix(); glTranslatef(0.6f, -0.7f, -0.6f); drawCylinder(0.08f, 2.2f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.6f, -0.7f, -0.6f); drawCylinder(0.08f, 2.2f); glPopMatrix();
    
    // Struts (Iron scaffolding)
    glPushMatrix(); glTranslatef(0.6f, -0.7f, 0);  glRotatef(-45, 0,0,1); drawCylinder(0.06f, 0.7f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.6f, -0.7f, 0);  glRotatef(45, 0,0,1);  drawCylinder(0.06f, 0.7f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.6f, -0.7f, 1.0f); glRotatef(-45, 0,0,1); drawCylinder(0.06f, 0.7f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.6f, -0.7f, 1.0f); glRotatef(45, 0,0,1);  drawCylinder(0.06f, 0.7f); glPopMatrix();

    // Searchlight (underneath, first person visible cone)
    float mat_light[] = { 1.0f, 1.0f, 0.8f, 1.0f };
    float mat_emit[]  = { 0.3f, 0.3f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_light);
    glMaterialfv(GL_FRONT, GL_EMISSION, mat_emit);
    glPushMatrix();
    glTranslatef(0, -0.7f, 0.5f);
    glutSolidSphere(0.08, 8, 8);
    glPopMatrix();
    float no_emit[] = {0,0,0,1};
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emit);

    glPopMatrix();
}
