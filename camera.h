#ifndef CAMERA_H
#define CAMERA_H

#include "helicopter.h"

// Set up the projection and modelview matrix for the camera
// camType: 0 = 3rd Person, 1 = 1st Person (Cockpit)
void setupCamera(Helicopter* heli, int camType);

#endif // CAMERA_H
