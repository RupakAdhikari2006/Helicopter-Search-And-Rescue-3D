#include "camera.h"
#include "terrain.h"
#include <GL/glut.h>
#include <math.h>

#define PI 3.14159265358979323846

void setupCamera(Helicopter* heli, int camType) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    float aspect = (float)w / (float)h;
    
    gluPerspective(60.0f, aspect, 0.1f, 3000.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    float h_rad = heli->rot_y * PI / 180.0f;
    float p_rad = heli->rot_x * PI / 180.0f;
    
    if (camType == 0) {
        // 3rd Person
        float cam_dist = 8.0f;
        float cam_height = 3.0f;
        
        float cx = heli->x - (float)sin(h_rad) * cam_dist;
        float cz = heli->z - (float)cos(h_rad) * cam_dist;
        float cy = heli->y + cam_height;
        
        // Don't clip through terrain
        float terrH = getTerrainHeight(cx, cz) + 1.0f;
        if (cy < terrH) cy = terrH;
        
        float look_x = heli->x;
        float look_y = heli->y + 0.5f;
        float look_z = heli->z;
        
        gluLookAt(cx, cy, cz, look_x, look_y, look_z, 0, 1, 0);
    } else {
        // 1st Person (Cockpit)
        float cx = heli->x + (float)sin(h_rad) * 1.5f;
        float cy = heli->y + 0.2f;
        float cz = heli->z + (float)cos(h_rad) * 1.5f;
        
        float look_x = cx + (float)sin(h_rad);
        float look_y = cy - (float)sin(p_rad);
        float look_z = cz + (float)cos(h_rad);
        
        float r_rad = heli->rot_z * PI / 180.0f;
        float up_x = (float)sin(r_rad);
        float up_y = (float)cos(r_rad);
        float up_z = 0.0f;
        
        gluLookAt(cx, cy, cz, look_x, look_y, look_z, up_x, up_y, up_z);
    }
}
