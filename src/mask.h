
/* a triangle wrapper that extends to include orientation and active subtri */

/* $Id$ */

/* Michael A. Park (Mike Park)
 * Computational AeroSciences Branch
 * NASA Langley Research Center
 * Hampton, VA 23681
 * Phone:(757) 864-6604
 * Email: Mike.Park@NASA.Gov
 */

#ifndef MASK_H
#define MASK_H

#include <stdlib.h>
#include <stdio.h>
#include "knife_definitions.h"

BEGIN_C_DECLORATION
typedef struct MaskStruct MaskStruct;
typedef MaskStruct * Mask;
END_C_DECLORATION

#include "triangle.h"

BEGIN_C_DECLORATION
struct MaskStruct {
  Triangle triangle;
  KnifeBool inward_pointing_normal;
  KnifeBool *active;
};

Mask mask_create( Triangle, KnifeBool inward_pointing_normal );
void mask_free( Mask );

#define mask_triangle( mask )((mask)->triangle)

int mask_nsubtri( Mask );

KNIFE_STATUS mask_deactivate_all_subtri( Mask );
KNIFE_STATUS mask_activate_subtri( Mask, Subtri );
KNIFE_STATUS mask_activate_subtri_index( Mask, int subtri_index );

#define mask_subtri_active(mask,subtri_index)	\
  ( NULL == (mask)->active ? TRUE : (mask)->active[(subtri_index)] )

KNIFE_STATUS mask_paint( Mask );
KNIFE_STATUS mask_verify_paint( Mask );

KNIFE_STATUS mask_dump_geom( Mask, FILE * );
KNIFE_STATUS mask_find_subtri_with( Mask, FILE * );

KNIFE_STATUS mask_centroid_volume_contribution( Mask, double *origin,
						double *centroid, 
						double *volume );
KNIFE_STATUS mask_directed_area_contribution( Mask, double *directed_area );

END_C_DECLORATION

#endif /* MASK_H */
