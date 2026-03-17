#ifndef HELICOPTER_H
#define HELICOPTER_H

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

// Helicopter state
typedef struct {
    float x, y, z;        // Position
    float rot_y;          // Heading/Yaw
    float rot_x;          // Pitch (forward/back tilt)
    float rot_z;          // Roll (left/right tilt)
    
    float main_rotor_angle;
    float tail_rotor_angle;
    
    float rotor_speed;
    int engine_on;          // 0=off, 1=starting, 2=running, 3=stopping
    
    // Physics velocities
    float velocity_ydir;
    float velocity_fwd;
    float velocity_side;
    
    // Rescue state
    int passengers;
    int total_targets;      // Selectable (3, 5, or 8)
    
    // Fuel system
    float fuel;             // 0-100
    int refuels_left;       // Limit to 2
    
    // Mission timer
    float game_time;        // seconds elapsed
    float time_remaining;   // countdown timer
    
} Helicopter;

void initHelicopter(Helicopter* heli);
void updateHelicopter(Helicopter* heli, float dt);
void drawHelicopter(Helicopter* heli);

#endif // HELICOPTER_H
