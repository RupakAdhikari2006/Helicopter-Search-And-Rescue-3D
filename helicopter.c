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

    // Main Body — Army Olive Drab
    float mat_amb_body[] = { 0.18f, 0.20f, 0.10f, 1.0f };
    float mat_diff_body[] = { 0.32f, 0.37f, 0.15f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_amb_body);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_body);
    
    glPushMatrix();
    glScalef(1.0f, 0.8f, 1.4f);
    glutSolidSphere(1.0, 32, 32);
    glPopMatrix();
    
    // Cockpit windshield (semi-transparent blue-green)
    float mat_glass[] = { 0.2f, 0.4f, 0.5f, 0.6f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_glass);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glTranslatef(0.0f, 0.15f, 1.0f);
    glScalef(0.6f, 0.5f, 0.4f);
    glutSolidSphere(1.0, 16, 16);
    glPopMatrix();
    glDisable(GL_BLEND);
    
    // Tail Boom
    float mat_amb_dark[] = { 0.15f, 0.15f, 0.08f, 1.0f };
    float mat_diff_dark[] = { 0.28f, 0.30f, 0.15f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_amb_dark);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_dark);

    glPushMatrix();
    glTranslatef(0, 0.2f, -1.0f);
    drawCylinder(0.15f, 2.0f);
    glPopMatrix();
    
    // Tail vertical fin
    glPushMatrix();
    glTranslatef(0, 0.2f, -3.0f);
    glRotatef(-45.0f, 1, 0, 0);
    glScalef(0.1f, 1.0f, 0.5f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Horizontal stabilizer
    glPushMatrix();
    glTranslatef(0, 0.3f, -2.7f);
    glScalef(1.2f, 0.08f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Rotor mast
    glPushMatrix();
    glTranslatef(0, 0.8f, 0);
    glRotatef(-90, 1, 0, 0);
    drawCylinder(0.1f, 0.3f);
    glPopMatrix();
    
    // Main Rotor Blades
    glPushMatrix();
    glTranslatef(0, 1.1f, 0);
    glRotatef(heli->main_rotor_angle, 0, 1, 0);
    
    float mat_spec[] = { 1, 1, 1, 1 };
    float mat_diff_rotor[] = { 0.7f, 0.7f, 0.7f, 1 };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_rotor);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 50.0f);
    
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glRotatef(90.0f * i, 0, 1, 0);
        glTranslatef(0, 0, 1.5f);
        glScalef(0.2f, 0.05f, 3.0f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
    
    // Tail Rotor
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_amb_dark);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff_dark);
    
    glPushMatrix();
    glTranslatef(0.15f, 0.4f, -2.8f);
    glRotatef(heli->tail_rotor_angle, 1, 0, 0);
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glRotatef(90.0f * i, 1, 0, 0);
        glTranslatef(0, 0.4f, 0);
        glScalef(0.05f, 0.8f, 0.1f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
    
    // Landing Skids
    glPushMatrix(); glTranslatef(0.5f, -0.6f, -0.5f); drawCylinder(0.05f, 2.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.5f, -0.6f, -0.5f); drawCylinder(0.05f, 2.0f); glPopMatrix();
    
    // Struts
    glPushMatrix(); glTranslatef(0.5f, -0.6f, 0);  glRotatef(-45, 0,0,1); drawCylinder(0.05f, 0.5f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.5f, -0.6f, 0);  glRotatef(45, 0,0,1);  drawCylinder(0.05f, 0.5f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.5f, -0.6f, 1.0f); glRotatef(-45, 0,0,1); drawCylinder(0.05f, 0.5f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.5f, -0.6f, 1.0f); glRotatef(45, 0,0,1);  drawCylinder(0.05f, 0.5f); glPopMatrix();

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
