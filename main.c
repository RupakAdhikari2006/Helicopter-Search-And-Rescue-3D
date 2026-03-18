#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

#include "helicopter.h"
#include "terrain.h"
#include "camera.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===== Globals =====
int window_width = 1024;
int window_height = 768;

Helicopter playerHeli;
int currentCamera = 0;
int keyStates[256];
int specialKeyStates[256];
int lastTime = 0;

volatile int soundRunning = 0;
float proximityFlash = 0.0f;
int nearPersonIdx = -1;

// Game state: 0=Menu, 1=Playing, 2=FuelOut, 3=Victory, 4=TimeOut, 5=Intro, 6=Crashed
int gameState = 5;

// Refueling logic
int isRefuelingThisSession = 0;

#ifdef _WIN32
void rotorSoundThread(void* arg) {
    while (soundRunning) {
        Beep(100, 80);
        Sleep(60);
    }
    _endthread();
}
#endif

void resize(int w, int h) {
    if (h == 0) h = 1;
    window_width = w; window_height = h;
    glViewport(0, 0, w, h);
}

static void renderText(float x, float y, const char* str) {
    glRasterPos2f(x, y);
    while (*str) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str); str++; }
}
static void renderTextSmall(float x, float y, const char* str) {
    glRasterPos2f(x, y);
    while (*str) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *str); str++; }
}
static void renderTextBig(float x, float y, const char* str) {
    glRasterPos2f(x, y);
    while (*str) { glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *str); str++; }
}

static void drawRect(float x, float y, float w, float h);

static void drawIntroStory() {
    glColor4f(0, 0, 0, 0.9f); drawRect(0, 0, window_width, window_height);
    
    float x = window_width / 2 - 270;
    float y = window_height - 120;
    
    glColor3f(1.0f, 0.8f, 0.2f); renderTextBig(x + 50, y, "THE QUEST FOR EREBOR");
    y -= 60;
    
    glColor3f(0.8f, 0.8f, 0.8f);
    renderText(x, y, "A great darkness has fallen over the Lonely Mountain.");
    y -= 30;
    renderText(x, y, "Thorin's Company of Dwarves has been scattered across the");
    y -= 30;
    renderText(x, y, "perilous lands of Middle-earth by the dragon Smaug's wrath.");
    y -= 50;
    
    renderText(x, y, "From the lush hills of the Shire to the freezing peaks");
    y -= 30;
    renderText(x, y, "of the Misty Mountains, they wait for rescue.");
    y -= 50;

    glColor3f(1.0f, 0.4f, 0.1f);
    renderText(x, y, "As the master engineer of the Dwarven Gyrocopter, you must");
    y -= 30;
    renderText(x, y, "navigate these hazardous realms. Gather your kin before");
    y -= 30;
    renderText(x, y, "the Orc packs find them in the wilderness.");
    y -= 80;

    float blink = 0.5f + 0.5f * (float)sin(glutGet(GLUT_ELAPSED_TIME)*0.005f);
    glColor4f(1, 0.9f, 0.5f, blink);
    renderText(window_width/2 - 140, y, "Press [ENTER] to Begin the Journey");
}

void processInput() {
    if (gameState != 1) return;
    
    float enginePwr = (playerHeli.engine_on == 2) ? 1.0f : 0.0f;
    
    if (keyStates['e'] || keyStates['E']) {
        if (playerHeli.engine_on == 0 && playerHeli.fuel > 0.0f) {
            playerHeli.engine_on = 1;
            #ifdef _WIN32
            if (!soundRunning) { soundRunning = 1; _beginthread(rotorSoundThread, 0, NULL); }
            #endif
        }
        keyStates['e'] = 0; keyStates['E'] = 0;
    }
    if (keyStates['q'] || keyStates['Q']) {
        if (playerHeli.engine_on == 2) { playerHeli.engine_on = 3; soundRunning = 0; }
        keyStates['q'] = 0; keyStates['Q'] = 0;
    }
    if (keyStates['v'] || keyStates['V']) {
        currentCamera = (currentCamera + 1) % 2;
        keyStates['v'] = 0; keyStates['V'] = 0;
    }
    
    if (enginePwr > 0.0f) {
        if (keyStates['w'] || keyStates['W']) { playerHeli.velocity_ydir += 0.5f; playerHeli.rot_x = 10.0f; }
        if (keyStates['s'] || keyStates['S']) { playerHeli.velocity_ydir -= 0.5f; playerHeli.rot_x = -10.0f; }
        if (specialKeyStates[GLUT_KEY_UP]) { playerHeli.velocity_fwd += 1.0f; playerHeli.rot_x = -20.0f; }
        if (specialKeyStates[GLUT_KEY_DOWN]) { playerHeli.velocity_fwd -= 1.0f; playerHeli.rot_x = 20.0f; }
        if (keyStates['a'] || keyStates['A']) { playerHeli.rot_y += 0.8f; playerHeli.rot_z = 15.0f; }
        if (keyStates['d'] || keyStates['D']) { playerHeli.rot_y -= 0.8f; playerHeli.rot_z = -15.0f; }
    }
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h); glEnd();
}

void drawHUD() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, window_width, 0, window_height);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // --- Startup Menu ---
    if (gameState == 0) {
        glColor4f(0, 0, 0, 0.8f); drawRect(0, 0, window_width, window_height);
        glColor3f(1.0f, 0.8f, 0.2f); renderTextBig(window_width/2 - 200, window_height/2 + 120, "GATHER THE COMPANY");
        glColor3f(1, 1, 1); renderText(window_width/2 - 130, window_height/2 + 40, "Select Difficulty Requirements:");
        
        glColor3f(0.6f, 1.0f, 0.6f); renderText(window_width/2 - 120, window_height/2 - 10, "[3]  Scouting Party (3 Dwarves, 3:00)");
        glColor3f(1.0f, 1.0f, 0.5f); renderText(window_width/2 - 120, window_height/2 - 50, "[5]  Main Expedition (5 Dwarves, 5:00)");
        glColor3f(1.0f, 0.5f, 0.5f); renderText(window_width/2 - 120, window_height/2 - 90, "[8]  Thorin's Full Co. (8 Dwarves, 7:00)");
        goto hud_end;
    }

    // --- Story Sequence ---
    if (gameState == 5) {
        drawIntroStory();
        goto hud_end;
    }
    glColor4f(0, 0, 0, 0.6f); drawRect(0, window_height - 90, window_width, 90);
    glColor3f(1.0f, 0.8f, 0.2f); renderText(15, window_height - 25, "JOURNEY UNDERWAY");
    
    char status[128];
    if (playerHeli.engine_on == 0) sprintf(status, "STEAM ENGINE: OFF");
    else if (playerHeli.engine_on == 1) sprintf(status, "STEAM ENGINE: HEATING...");
    else if (playerHeli.engine_on == 2) sprintf(status, "STEAM ENGINE: RUNNING");
    else sprintf(status, "STEAM ENGINE: COOLING...");
    glColor3f(0.8f, 0.8f, 0.8f); renderTextSmall(15, window_height - 45, status);
    
    float terrH = getTerrainHeight(playerHeli.x, playerHeli.z);
    float AGL = playerHeli.y - terrH;
    char altStr[128]; sprintf(altStr, "ALT: %.0f ft | SPD: %.1f kts | AGL: %.0f ft", playerHeli.y*10, playerHeli.velocity_fwd*5, AGL*10);
    glColor3f(0.7f, 1, 0.7f); renderTextSmall(15, window_height - 62, altStr);
    
    char rescuedStr[64]; sprintf(rescuedStr, "DWARVES GATHERED: %d / %d", playerHeli.passengers, playerHeli.total_targets);
    glColor3f(1, 1, 0); renderText(15, window_height - 85, rescuedStr);
    
    int mins = (int)(playerHeli.game_time / 60); int secs = (int)(playerHeli.game_time) % 60;
    char timerStr[32]; sprintf(timerStr, "TIME: %02d:%02d", mins, secs);
    glColor3f(1,1,1); renderText(window_width - 150, window_height - 25, timerStr);
    
    // Countdown Timer
    int remMins = (int)(playerHeli.time_remaining / 60); int remSecs = (int)(playerHeli.time_remaining) % 60;
    char remStr[32]; sprintf(remStr, "REMAINING: %02d:%02d", remMins, remSecs);
    if (playerHeli.time_remaining < 60.0f) {
        float b = 0.5f + 0.5f * (float)sin(proximityFlash * 10);
        glColor3f(1, b*0.2f, b*0.2f);
    } else {
        glColor3f(1, 0.8f, 0.2f);
    }
    renderText(window_width - 190, window_height - 55, remStr);

    // Compass
    float heading = playerHeli.rot_y; while(heading<0) heading+=360; while(heading>=360) heading-=360;
    char compStr[32]; sprintf(compStr, "HDG: %03.0f", heading);
    glColor3f(1, 0.85f, 0); renderText(window_width/2 - 40, window_height - 25, compStr);

    // Fuel Gauge
    {
        float barX=20, barY=150, barW=20, barH=180;
        float fuelPct = playerHeli.fuel/100.0f;
        glColor4f(0.2f,0.2f,0.2f,0.7f); drawRect(barX, barY, barW, barH);
        float r, g; if (fuelPct > 0.5f) { g=1; r=(1-fuelPct)*2; } else { r=1; g=fuelPct*2; }
        glColor4f(r, g, 0, 0.8f); drawRect(barX+2, barY+2, barW-4, (barH-4)*fuelPct);
        glColor3f(1,1,1); renderTextSmall(barX, barY+barH+5, "COAL");
        char fstr[16]; sprintf(fstr, "%.0f%%", playerHeli.fuel); renderTextSmall(barX, barY-15, fstr);
        
        char refStr[32]; sprintf(refStr, "REFILLS: %d", playerHeli.refuels_left);
        glColor3f(1, 0.5f, 0); renderTextSmall(barX, barY-35, refStr);
        
        if (isRefuelingThisSession) {
            float b = 0.5f + 0.5f*(float)sin(proximityFlash*12);
            glColor4f(0, 1, 0, b); renderText(50, barY + barH/2, "RELOADING COAL...");
        } else if (playerHeli.fuel < 20 && playerHeli.fuel > 0) {
            float b = 0.5f + 0.5f*(float)sin(proximityFlash*6);
            glColor4f(1, 0, 0, b); renderText(50, barY + barH/2, "LOW COAL!");
        }
    }

    // Minimap
    {
        float sz=160, mx=window_width-sz-20, my=20, sc=sz/200.0f;
        glColor4f(0, 0.1f, 0, 0.8f); drawRect(mx, my, sz, sz);
        // Fuel icons
        glColor3f(1,0,0); for(int i=0; i<NUM_FUEL_STATIONS; i++) {
            float fx=mx+sz/2+fuelStations[i].x*sc, fy=my+sz/2+fuelStations[i].z*sc;
            drawRect(fx-3, fy-3, 6, 6);
        }
        // People
        for(int i=0; i<playerHeli.total_targets; i++) {
            float px=mx+sz/2+people[i].x*sc, py=my+sz/2+people[i].z*sc;
            if (people[i].is_rescued) glColor3f(0,1,0); else glColor3f(1,1,0);
            glPointSize(5); glBegin(GL_POINTS); glVertex2f(px, py); glEnd();
        }
        // Heli
        float hx=mx+sz/2+playerHeli.x*sc, hy=my+sz/2+playerHeli.z*sc;
        glColor3f(1,1,1); glPointSize(6); glBegin(GL_POINTS); glVertex2f(hx, hy); glEnd();
    }

    if (nearPersonIdx >= 0) {
        float b = 0.5f + 0.5f*(float)sin(proximityFlash*8);
        glColor4f(1, 0.9f, 0, b); renderText(window_width/2-80, window_height/2-60, "! DWARF NEARBY !");
    }

    // Game Over Screens
    if (gameState == 2) {
        glColor4f(0,0,0,0.85f); drawRect(0,0,window_width,window_height);
        glColor3f(1,0,0); renderTextBig(window_width/2-100, window_height/2+20, "COAL DEPLETED");
        glColor3f(1,1,1); renderText(window_width/2-80, window_height/2-20, "[R] Restart Journey");
    }
    if (gameState == 3) {
        glColor4f(0,0,0,0.85f); drawRect(0,0,window_width,window_height);
        glColor3f(0.8f,0.6f,0); renderTextBig(window_width/2-150, window_height/2+50, "THE COMPANY IS GATHERED!");
        glColor3f(1,1,1); char tstr[64]; sprintf(tstr, "Time: %02d:%02d | Coal: %.0f%%", mins, secs, playerHeli.fuel);
        renderText(window_width/2-130, window_height/2+10, tstr);
        renderText(window_width/2-80, window_height/2-40, "[R] Restart Journey");
    }
    if (gameState == 4) {
        glColor4f(0,0,0,0.85f); drawRect(0,0,window_width,window_height);
        glColor3f(1,0.2f,0); renderTextBig(window_width/2-110, window_height/2+20, "THE ORCS FOUND THEM!");
        glColor3f(1,1,1); renderText(window_width/2-80, window_height/2-20, "[R] Restart Journey");
    }
    if (gameState == 6) {
        glColor4f(0.3f,0,0,0.85f); drawRect(0,0,window_width,window_height);
        float blink = 0.5f + 0.5f*(float)sin(proximityFlash*15.0f);
        glColor3f(1,blink,blink); renderTextBig(window_width/2-170, window_height/2+20, "GYROCOPTER DESTROYED!");
        glColor3f(1,1,1); renderText(window_width/2-80, window_height/2-20, "[R] Restart Journey");
    }

hud_end:
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera(&playerHeli, currentCamera);
    drawTerrain();
    if (gameState != 0) drawHelicopter(&playerHeli);
    drawHUD();
    glutSwapBuffers();
}

void update() {
    int t = glutGet(GLUT_ELAPSED_TIME); float dt = (t - lastTime) / 1000.0f; lastTime = t;
    if (dt > 0.1f) dt = 0.1f;
    proximityFlash += dt;
    processInput();
    if (gameState == 1) {
        updateHelicopter(&playerHeli, dt);
        
        // Timer countdown
        if (playerHeli.engine_on == 2) {
            playerHeli.time_remaining -= dt;
            if (playerHeli.time_remaining <= 0.0f) {
                playerHeli.time_remaining = 0.0f;
                gameState = 4; soundRunning = 0;
            }
        }

        float th = getTerrainHeight(playerHeli.x, playerHeli.z);
        
        // Check Terrain Collisions / Refueling
        float agl = playerHeli.y - th;
        
        // Robust Crash Physics
        if (playerHeli.engine_on >= 1) {
            float impactVel = playerHeli.velocity_ydir;
            float slope = getTerrainSlope(playerHeli.x, playerHeli.z);
            float fwdSpeed = playerHeli.velocity_fwd;
            
            // 1. Vertical / Hard Landing Crash
            if (agl <= 0.55f) {
                if (impactVel < -7.0f || slope > 1.5f || (fwdSpeed > 4.0f && slope > 0.6f)) {
                    gameState = 6; soundRunning = 0;
                }
            }
            
            // 2. Frontal Collision Check (Hitting a hill/mountain face)
            // Check a point ~2.5 units in front of the helicopter
            float rad = playerHeli.rot_y * M_PI / 180.0f;
            float frontX = playerHeli.x + (float)sin(rad) * 2.5f;
            float frontZ = playerHeli.z + (float)cos(rad) * 2.5f;
            float frontHeight = getTerrainHeight(frontX, frontZ);
            
            // If the nose of the heli is below the terrain height at that point
            if (playerHeli.y < frontHeight + 0.3f && fwdSpeed > 2.0f) {
                gameState = 6; soundRunning = 0;
            }
            
            if (gameState == 6) {
                #ifdef _WIN32
                Beep(400, 500); Beep(300, 1000);
                #endif
            }
        }
        
        // --- Object Collision Detection (Trees/Rocks) ---
        if (gameState == 1 && playerHeli.engine_on >= 1) {
            // Check Trees
            for (int i = 0; i < NUM_TREES; i++) {
                if (trees[i].x > 900.0f) continue; // Skip uninitialized
                
                float dx = playerHeli.x - trees[i].x;
                float dz = playerHeli.z - trees[i].z;
                float distSq = dx*dx + dz*dz;
                
                // Collision radius depends on tree scale
                float colRadius = 1.0f + (trees[i].scale * 0.6f); 
                if (distSq < colRadius * colRadius) {
                    float groundH = getTerrainHeight(trees[i].x, trees[i].z);
                    float treeTop = groundH + (trees[i].scale * 3.5f);
                    
                    // If heli is below the top of the tree, CRASH!
                    if (playerHeli.y < treeTop && playerHeli.y > groundH) {
                        gameState = 6; soundRunning = 0;
                        #ifdef _WIN32
                        Beep(350, 400); Beep(200, 800);
                        #endif
                        break;
                    }
                }
            }
            
            // Check Rocks
            if (gameState == 1) { // Only check rocks if not already crashed
                for (int i = 0; i < NUM_ROCKS; i++) {
                    float dx = playerHeli.x - rocks[i].x;
                    float dz = playerHeli.z - rocks[i].z;
                    float distSq = dx*dx + dz*dz;
                    
                    float colRadius = 0.8f + (rocks[i].scale * 0.8f);
                    if (distSq < colRadius * colRadius) {
                        float groundH = getTerrainHeight(rocks[i].x, rocks[i].z);
                        float rockTop = groundH + (rocks[i].scale * 1.2f);
                        
                        if (playerHeli.y < rockTop && playerHeli.y > groundH - 1.0f) {
                            gameState = 6; soundRunning = 0;
                            #ifdef _WIN32
                            Beep(350, 400); Beep(200, 800);
                            #endif
                            break;
                        }
                    }
                }
            }
        }
        
        if (playerHeli.fuel <= 0 && agl <= 0.6f) { gameState=2; soundRunning=0; }
        
        // Refueling logic with limit
        int atFuelStation = -1;
        if (agl < 1.0f) {
            for(int i=0; i<NUM_FUEL_STATIONS; i++) {
                float dx=playerHeli.x-fuelStations[i].x, dz=playerHeli.z-fuelStations[i].z;
                if (sqrt(dx*dx+dz*dz)<4.5f) { atFuelStation=i; break; }
            }
        }
        if (atFuelStation >= 0 && playerHeli.fuel < 99.0f) {
            if (!isRefuelingThisSession && playerHeli.refuels_left > 0) {
                playerHeli.refuels_left--; isRefuelingThisSession = 1;
            }
            if (isRefuelingThisSession) {
                playerHeli.fuel += 20.0f * dt; if(playerHeli.fuel>100) playerHeli.fuel=100;
            }
        } else if (atFuelStation == -1 || playerHeli.fuel >= 100.0f || agl > 1.2f) {
            isRefuelingThisSession = 0;
        }

        // People proximity
        nearPersonIdx = -1; float cd = 25.0f;
        for(int i=0; i<playerHeli.total_targets; i++) {
            if(!people[i].is_rescued) {
                float dx=playerHeli.x-people[i].x, dz=playerHeli.z-people[i].z, d=(float)sqrt(dx*dx+dz*dz);
                if(d<20 && d<cd) { cd=d; nearPersonIdx=i; }
                if(agl<1.5f && d<5.0f) { people[i].is_rescued=1; playerHeli.passengers++; 
                    #ifdef _WIN32
                    Beep(800,150); if(playerHeli.passengers>=playerHeli.total_targets) { Beep(1200,400); gameState=3; }
                    #else
                    if(playerHeli.passengers>=playerHeli.total_targets) gameState=3;
                    #endif
                }
            }
        }
    }
    glutPostRedisplay();
}

void resetGame() {
    soundRunning = 0; initHelicopter(&playerHeli); initTerrain();
    currentCamera = 0; gameState = 5; nearPersonIdx = -1; isRefuelingThisSession = 0;
}

void keyDown(unsigned char key, int x, int y) {
    if (gameState == 5) {
        if (key == 13) gameState = 0; // ENTER to proceed
        return;
    }

    if (gameState == 0) {
        if (key == '3') { playerHeli.total_targets = 3; playerHeli.time_remaining = 180.0f; gameState = 1; }
        else if (key == '5') { playerHeli.total_targets = 5; playerHeli.time_remaining = 300.0f; gameState = 1; }
        else if (key == '8') { playerHeli.total_targets = 8; playerHeli.time_remaining = 420.0f; gameState = 1; }
        return;
    }
    keyStates[key] = 1;
    if (gameState > 1 && (key == 'r' || key == 'R')) resetGame();
    if (key == 27) exit(0);
}

void keyUp(unsigned char key, int x, int y) { keyStates[key] = 0; }
void specialKeyDown(int k, int x, int y) { specialKeyStates[k] = 1; }
void specialKeyUp(int k, int x, int y) { specialKeyStates[k] = 0; }

void init() {
    // Natural sky blue - not aqua/cyan
    glClearColor(0.53f, 0.72f, 0.87f, 1.0f);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    /* GL_COLOR_MATERIAL removed — it caused glColor3f (sky) to override glMaterialfv (terrain) */
    /* Set a global scene material so objects aren't totally black without explicit materials */
    float scene_amb[] = {0.6f, 0.65f, 0.55f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, scene_amb);
    // Bright warm sunlight - high ambient so greens are visible even in shadow
    float l_amb[]={0.55f, 0.60f, 0.50f, 1.0f};   // warm greenish ambient
    float l_dif[]={1.0f, 0.98f, 0.90f, 1.0f};     // near-white warm sunlight diffuse
    float l_pos[]={200.0f, 400.0f, 150.0f, 0.0f}; // high sun angle
    glLightfv(GL_LIGHT0, GL_AMBIENT, l_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l_dif);
    glLightfv(GL_LIGHT0, GL_POSITION, l_pos);
    // Natural outdoor haze fog - warm dusty atmosphere
    glEnable(GL_FOG);
    float fCol[] = {0.68f, 0.78f, 0.65f, 1.0f}; // warm greenish-grey haze
    glFogfv(GL_FOG_COLOR, fCol);
    glFogi(GL_FOG_MODE, GL_EXP2); glFogf(GL_FOG_DENSITY, 0.0025f);
    resetGame(); lastTime = glutGet(GLUT_ELAPSED_TIME);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("The Quest for Erebor - A Gyrocopter Adventure");
    init();
    glutDisplayFunc(display); glutReshapeFunc(resize); glutIdleFunc(update);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialKeyDown); glutSpecialUpFunc(specialKeyUp);
    glutMainLoop();
    return 0;
}
