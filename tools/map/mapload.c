#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "mapload.h"

#define MAX_PLAYERS	8
#define MAP_MAXAREA	(256 * 256)
#define debug(z, ...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

void mapFree(GAMEMAP *map)
{
	if (map)
	{
		free(map->mGateways);
		free(map->mMapTiles);
	}
	free(map);
}

/* Initialise the map structure */
GAMEMAP *mapLoad(char *filename)
{
	char		path[PATH_MAX];
	GAMEMAP		*map = malloc(sizeof(*map));
	uint32_t	i, j, gameTime, gameType, droidVersion;
	uint32_t	gwVersion, gameVersion, featVersion, terrainVersion;
	char		aFileType[4];
	bool		littleEndian = true;
	PHYSFS_file	*fp = NULL;
	uint16_t	terrainSignature[3];
	bool		counted[MAX_PLAYERS];

	// this cries out for a class based design
	#define readU8(v) ( littleEndian ? PHYSFS_readULE8(fp, v) : PHYSFS_readUBE8(fp, v) )
	#define readU16(v) ( littleEndian ? PHYSFS_readULE16(fp, v) : PHYSFS_readUBE16(fp, v) )
	#define readU32(v) ( littleEndian ? PHYSFS_readULE32(fp, v) : PHYSFS_readUBE32(fp, v) )
	#define readS8(v) ( littleEndian ? PHYSFS_readSLE8(fp, v) : PHYSFS_readSBE8(fp, v) )
	#define readS16(v) ( littleEndian ? PHYSFS_readSLE16(fp, v) : PHYSFS_readSBE16(fp, v) )
	#define readS32(v) ( littleEndian ? PHYSFS_readSLE32(fp, v) : PHYSFS_readSBE32(fp, v) )

	/* === Load map data === */

	strcpy(path, filename);
	strcat(path, "/game.map");
	fp = PHYSFS_openRead(path);
	if (!map)
	{
		return NULL;
	}
	map->mGateways = NULL;
	map->mMapTiles = NULL;

	if (!fp)
	{
		debug(LOG_ERROR, "Map file %s not found", path);
		goto failure;
	}
	else if (PHYSFS_read(fp, aFileType, 4, 1) != 1
	    || !readU32(&map->version)
	    || !readU32(&map->width)
	    || !readU32(&map->height)
	    || aFileType[0] != 'm'
	    || aFileType[1] != 'a'
	    || aFileType[2] != 'p')
	{
		debug(LOG_ERROR, "Bad header in %s", path);
		goto failure;
	}
	else if (map->version <= 9)
	{
		debug(LOG_ERROR, "%s: Unsupported save format version %u", path, map->version);
		goto failure;
	}
	else if (map->version > 36)
	{
		debug(LOG_ERROR, "%s: Undefined save format version %u", path, map->version);
		goto failure;
	}
	else if (map->width * map->height > MAP_MAXAREA)
	{
		debug(LOG_ERROR, "Map %s too large : %d %d", path, map->width, map->height);
		goto failure;
	}

	/* Allocate the memory for the map */
	map->mMapTiles = calloc(map->width * map->height, sizeof(*map->mMapTiles));
	if (!map->mMapTiles)
	{
		debug(LOG_ERROR, "Out of memory");
		goto failure;
	}
	
	/* Load in the map data */
	for (i = 0; i < map->width * map->height; i++)
	{
		uint16_t	texture;
		uint8_t		height;

		if (!readU16(&texture) || !readU8(&height))
		{
			debug(LOG_ERROR, "%s: Error during savegame load", path);
			goto failure;
		}

		map->mMapTiles[i].texture = texture;
		map->mMapTiles[i].height = height;
		for (j = 0; j < MAX_PLAYERS; j++)
		{
			map->mMapTiles[i].tileVisBits = (uint8_t)(map->mMapTiles[i].tileVisBits &~ (uint8_t)(1 << j));
		}
	}

	if (!readU32(&gwVersion) || !readU32(&map->numGateways) || gwVersion != 1)
	{
		debug(LOG_ERROR, "Bad gateway in %s", path);
		goto failure;
	}

	map->mGateways = calloc(map->numGateways, sizeof(*map->mGateways));
	for (i = 0; i < map->numGateways; i++)
	{
		if (!readU8(&map->mGateways[i].x1) || !readU8(&map->mGateways[i].y1)
		    || !readU8(&map->mGateways[i].x2) || !readU8(&map->mGateways[i].y2))
		{
			debug(LOG_ERROR, "%s: Failed to read gateway info", path);
			goto failure;
		}
	}
	PHYSFS_close(fp);


	/* === Load game data === */

	strcpy(path, filename);
	strcat(path, ".gam");
	fp = PHYSFS_openRead(path);
	if (!fp)
	{
		debug(LOG_ERROR, "Game file %s not found", path);
		goto failure;
	}
	else if (PHYSFS_read(fp, aFileType, 4, 1) != 1
	    || aFileType[0] != 'g'
	    || aFileType[1] != 'a'
	    || aFileType[2] != 'm'
	    || aFileType[3] != 'e'
	    || !readU32(&gameVersion))
	{
		debug(LOG_ERROR, "Bad header in %s", path);
		goto failure;
	}
	if (gameVersion > 35)	// big-endian
	{
		littleEndian = false;
	}
	if (!readU32(&gameTime)
	    || !readU32(&gameType)
	    || !readS32(&map->scrollMinX)
	    || !readS32(&map->scrollMinY)
	    || !readU32(&map->scrollMaxX)
	    || !readU32(&map->scrollMaxY)
	    || PHYSFS_read(fp, map->levelName, 20, 1) != 1)
	{
		debug(LOG_ERROR, "Bad data in %s", filename);
		goto failure;
	}
	PHYSFS_close(fp);


	/* === Load feature data === */

	littleEndian = true;
	strcpy(path, filename);
	strcat(path, "/feat.bjo");
	fp = PHYSFS_openRead(path);
	if (!fp)
	{
		debug(LOG_ERROR, "Feature file %s not found", path);
		goto failure;
	}
	else if (PHYSFS_read(fp, aFileType, 4, 1) != 1
	    || aFileType[0] != 'f'
	    || aFileType[1] != 'e'
	    || aFileType[2] != 'a'
	    || aFileType[3] != 't'
	    || !readU32(&featVersion)
	    || !readU32(&map->numFeatures))
	{
		debug(LOG_ERROR, "Bad features header in %s", path);
		goto failure;
	}
	map->mLndObjects[IMD_FEATURE] = malloc(sizeof(*map->mLndObjects[IMD_FEATURE]) * map->numFeatures);
	for(i = 0; i < map->numFeatures; i++)
	{
		LND_OBJECT *psObj = &map->mLndObjects[0][i];
		int nameLength = 60;
		uint32_t dummy;
		uint8_t visibility[8];

		if (featVersion <= 19)
		{
			nameLength = 40;
		}
		if (PHYSFS_read(fp, psObj->name, nameLength, 1) != 1
		    || !readU32(&psObj->id)
		    || !readU32(&psObj->x) || !readU32(&psObj->y) || !readU32(&psObj->z)
		    || !readU32(&psObj->direction)
		    || !readU32(&psObj->player)
		    || !readU32(&dummy) // BOOL inFire
		    || !readU32(&dummy) // burnStart
		    || !readU32(&dummy)) // burnDamage
		{
			debug(LOG_ERROR, "Failed to read feature from %s", path);
			goto failure;
		}
		psObj->player = 0;	// work around invalid feature owner
		if (featVersion >= 14 && PHYSFS_read(fp, &visibility, 1, 8) != 8)
		{
			debug(LOG_ERROR, "Failed to read feature visibility from %s", path);
			goto failure;
		}
		psObj->type = 0;	// IMD LND type for feature
		// Sanity check data
		if (psObj->x >= map->width * TILE_WIDTH || psObj->y >= map->height * TILE_HEIGHT)
		{
			debug(LOG_ERROR, "Bad feature coordinate %u(%u, %u)", psObj->id, psObj->x, psObj->y);
			goto failure;
		}
	}
	PHYSFS_close(fp);


	/* === Load terrain data === */

	littleEndian = true;
	strcpy(path, filename);
	strcat(path, "/ttypes.ttp");
	fp = PHYSFS_openRead(path);
	if (!fp)
	{
		debug(LOG_ERROR, "Terrain type file %s not found", path);
		goto failure;
	}
	else if (PHYSFS_read(fp, aFileType, 4, 1) != 1
	    || aFileType[0] != 't'
	    || aFileType[1] != 't'
	    || aFileType[2] != 'y'
	    || aFileType[3] != 'p'
	    || !readU32(&terrainVersion)
	    || !readU32(&map->numTerrainTypes))
	{
		debug(LOG_ERROR, "Bad features header in %s", path);
		goto failure;
	}
	if (!readU16(&terrainSignature[0]) || !readU16(&terrainSignature[1]) || !readU16(&terrainSignature[2]))
	{
		debug(LOG_ERROR, "Could not read terrain signature from %s", path);
		goto failure;
	}
	if (terrainSignature[0] == 1 && terrainSignature[1] == 0 && terrainSignature[2] == 2)
	{
		map->tileset = TILESET_ARIZONA;
	}
	else if (terrainSignature[0] == 2 && terrainSignature[1] == 2 && terrainSignature[2] == 2)
	{
		map->tileset = TILESET_URBAN;
	}
	else if (terrainSignature[0] == 0 && terrainSignature[1] == 0 && terrainSignature[2] == 2)
	{
		map->tileset = TILESET_ROCKIES;
	}
	else
	{
		debug(LOG_ERROR, "Unknown terrain signature in %s: %lu %lu %lu", path, 
		      terrainSignature[0], terrainSignature[1], terrainSignature[2]);
		goto failure;
	}
	PHYSFS_close(fp);


	/* === Load structure data === */

	map->mLndObjects[IMD_STRUCTURE] = NULL;
	map->numStructures = 0;


	/* === Load droid data === */

	map->mLndObjects[IMD_DROID] = NULL;
	map->numDroids = 0;
	littleEndian = true;
	strcpy(path, filename);
	strcat(path, "/dinit.bjo");
	fp = PHYSFS_openRead(path);
	if (fp)
	{
		if (PHYSFS_read(fp, aFileType, 4, 1) != 1
		    || aFileType[0] != 'd'
		    || aFileType[1] != 'i'
		    || aFileType[2] != 'n'
		    || aFileType[3] != 't'
		    || !readU32(&droidVersion)
		    || !readU32(&map->numDroids))
		{
			debug(LOG_ERROR, "Bad droid header in %s", path);
			goto failure;
		}
		map->mLndObjects[IMD_DROID] = malloc(sizeof(*map->mLndObjects[IMD_DROID]) * map->numDroids);
		for (i = 0; i < map->numDroids; i++)
		{
			LND_OBJECT *psObj = &map->mLndObjects[IMD_DROID][i];
			int nameLength = 60;
			uint32_t dummy;
			uint8_t visibility[8];

			if (droidVersion <= 19)
			{
				nameLength = 40;
			}
			if (PHYSFS_read(fp, psObj->name, nameLength, 1) != 1
			    || !readU32(&psObj->id)
			    || !readU32(&psObj->x) || !readU32(&psObj->y) || !readU32(&psObj->z)
			    || !readU32(&psObj->direction)
			    || !readU32(&psObj->player)
			    || !readU32(&dummy) // BOOL inFire
			    || !readU32(&dummy) // burnStart
			    || !readU32(&dummy)) // burnDamage
			{
				debug(LOG_ERROR, "Failed to read droid from %s", path);
				goto failure;
			}
			psObj->type = IMD_DROID;
			// Sanity check data
			if (psObj->x >= map->width * TILE_WIDTH || psObj->y >= map->height * TILE_HEIGHT)
			{
				debug(LOG_ERROR, "Bad droid coordinate %u(%u, %u)", psObj->id, psObj->x, psObj->y);
				goto failure;
			}
		}
		PHYSFS_close(fp);
	}

	// Count players by looking for the obligatory construction droids
	map->numPlayers = 0;
	memset(counted, 0, sizeof(counted));
	for(i = 0; i < map->numDroids; i++)
	{
		LND_OBJECT *psObj = &map->mLndObjects[IMD_DROID][i];

		if (counted[psObj->player] == false && (strcmp(psObj->name, "ConstructorDroid") == 0 || strcmp(psObj->name, "ConstructionDroid") == 0))
		{
			counted[psObj->player] = true;
			map->numPlayers++;
		}
	}

	return map;

failure:
	mapFree(map);
	if (fp)
	{
		PHYSFS_close(fp);
	}
	return NULL;
}
