#ifndef TERRAIN_H
#define TERRAIN_H

#define GRID_SIZE 81
#define TERRAIN_SCALE 2.5f
#define NUM_PEOPLE 8
#define NUM_TREES 80
#define NUM_ROCKS 30
#define NUM_BUSHES 50
#define NUM_RIVER_PTS 12
#define NUM_FUEL_STATIONS 2

// Heightmap
extern float heightMap[GRID_SIZE][GRID_SIZE];

// Stranded person
typedef struct {
    float x;
    float z;
    int is_rescued;
} StrandedPerson;

// Tree variety
typedef struct {
    float x, z;
    int type;       // 0=pine, 1=oak, 2=birch
    float scale;    // size variation
} TreeInstance;

// Rock
typedef struct {
    float x, z;
    float scale;
    float rot;
} RockInstance;

// Bush
typedef struct {
    float x, z;
    float scale;
} BushInstance;

// River waypoint
typedef struct {
    float x, z;
} RiverPoint;

// Fuel Station
typedef struct {
    float x, z;
} FuelStation;

extern StrandedPerson people[NUM_PEOPLE];
extern TreeInstance trees[NUM_TREES];
extern RockInstance rocks[NUM_ROCKS];
extern BushInstance bushes[NUM_BUSHES];
extern RiverPoint riverPath[NUM_RIVER_PTS];
extern FuelStation fuelStations[NUM_FUEL_STATIONS];

// Get interpolated terrain height at world position
float getTerrainHeight(float wx, float wz);

// Get steepness of terrain at world position
float getTerrainSlope(float wx, float wz);

// Init all terrain data
void initTerrain();

// Draw everything
void drawTerrain();

#endif // TERRAIN_H
