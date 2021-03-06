
/* fortran API for knife package */

/* Copyright 2007 United States Government as represented by the
 * Administrator of the National Aeronautics and Space
 * Administration. No copyright is claimed in the United States under
 * Title 17, U.S. Code.  All Other Rights Reserved.
 *
 * The knife platform is licensed under the Apache License, Version
 * 2.0 (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "knife_definitions.h"

#include "logger.h"

#define FORTRAN_LOGGER_LEVEL (0)

#include "primal.h"
#include "surface.h"
#include "domain.h"

#include "node.h"
#include "triangle.h"
#include "loop.h"
#include "poly.h"

#define TRY(fcn,msg)					      \
  {							      \
    int code;						      \
    code = (fcn);					      \
    if (KNIFE_SUCCESS != code){				      \
      char surface_tecplot_filename[1025];                            \
      printf("%s: %d: code %d, part %d, %s\n",			      \
	     __FILE__,__LINE__,code,partition,(msg));	      \
      fflush(stdout);					      \
      *knife_status = code;				      \
      sprintf(surface_tecplot_filename,"surface%04d.t",partition);    \
      surface_export_tec( surface, surface_tecplot_filename );\
      return;						      \
    }							      \
  }

#define NOT_NULL(pointer,msg)				\
  if (NULL == (pointer)) {				\
    printf("%s: %d: %s\n",__FILE__,__LINE__,(msg));	\
    fflush(stdout);					\
    *knife_status = KNIFE_NULL;				\
    return;						\
  }

#define ASSERT_INT_EQ(int1,int2,msg)			\
  if ( (int1) != (int2) ) {				\
    printf("%s: %d: %d != %d %s\n",                     \
           __FILE__,__LINE__,(int1),(int2),(msg));	\
    fflush(stdout);					\
    *knife_status = KNIFE_INCONSISTENT;			\
    return;						\
  }

static Primal  surface_primal = NULL;
static Surface surface        = NULL;
static Primal  volume_primal  = NULL;
static Domain  domain         = NULL;
static int partition = EMPTY;

void FC_FUNC_(knife_volume,KNIFE_VOLUME)
  ( int *part_id,
    int *nnode0, int *nnode, double *x, double *y, double *z,
    int *nface, int *ncell, int *c2n, 
    int *knife_status )
{

  partition = *part_id;

  /* to enable logging by partition id number */

  /* logger_partition( *part_id ); logging disabled with this comment! */

  logger_message( FORTRAN_LOGGER_LEVEL, "volume");

  /* so tecplot filenames will include processor id */
  triangle_set_frame( 10000*partition);
  loop_set_frame( 10000*partition);
  mask_set_frame( 10000*partition);
  poly_set_frame( 10000*partition);

  volume_primal = primal_create( *nnode, *nface, *ncell );
  NOT_NULL(volume_primal, "volume_primal NULL");

  volume_primal->nnode0 = *nnode0;

  TRY( primal_copy_volume( volume_primal, x, y, z, c2n ), 
       "primal_copy_volume");

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_boundary,KNIFE_BOUNDARY)
  ( int *face_id, int *nodedim, int *inode,
    int *leading_dim, int *nface, int *f2n, 
    int *knife_status )
{

  logger_message( FORTRAN_LOGGER_LEVEL, "boundary");

  if ( *nface > 0 )
    TRY( primal_copy_boundary( volume_primal, *face_id, 
			       *nodedim, inode,
			       *leading_dim, *nface, f2n ), 
	 "primal_copy_boundary");

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_required_local_dual,KNIFE_REQUIRED_LOCAL_DUAL)
  ( char *knife_input_file_name, 
    int *nodedim, int *required,
    int *knife_status )
{
  FILE *f;
  char surface_filename[1025];
  char string[1025];
  KnifeBool inward_pointing_surface_normal;
  KnifeBool read_faces;
  Set bcs;
  int bc, bc_found;
  int end_of_string;

  logger_message( FORTRAN_LOGGER_LEVEL, "req loc dual");

  if ( *nodedim != primal_nnode(volume_primal)  )
    {
      printf("%s: %d: knife_required_local_dual_ wrong nnode %d %d\n",
	     __FILE__,__LINE__,*nodedim,domain_npoly( domain ));
      *knife_status = KNIFE_ARRAY_BOUND;
      return;
    }

  f = NULL;
  f = fopen(knife_input_file_name, "r");
  if ( NULL == f ) printf("filename: %s\n",knife_input_file_name);
  NOT_NULL(f , "could not open knife_input_file_name");

  fscanf( f, "%s\n", surface_filename);
  end_of_string = strlen(surface_filename);

  surface_primal = primal_from_file(surface_filename);
  if ( NULL == surface_primal ) 
    printf("surface filename: %s\n",surface_filename);
  NOT_NULL(surface_primal, "surface_primal NULL");
  
  inward_pointing_surface_normal = FALSE;
  read_faces = FALSE;

  while ( !( feof( f ) || read_faces ) )
    {
      double dx, dy, dz;
      double scale;
      double angle;
      char massoud_filename[1024];

      fscanf( f, "%s\n", string );
      if( strcmp(string,"outward") == 0 ) {
	inward_pointing_surface_normal = FALSE;
      }
      if( strcmp(string,"inward") == 0 ) {
	inward_pointing_surface_normal = TRUE;
      }
      if( strcmp(string,"translate") == 0 ) {
	fscanf( f, "%lf %lf %lf\n", &dx, &dy, &dz );
	TRY( primal_translate( surface_primal, dx, dy, dz ), 
	     "primal_translate error" );
      }
#define KNIFE_CONVERT_DEGREE_TO_RADIAN(degree) ((degree)*0.0174532925199433)
      if( strcmp(string,"rotate") == 0 ) {
	fscanf( f, "%lf %lf %lf %lf\n", &dx, &dy, &dz, &angle );
	angle = KNIFE_CONVERT_DEGREE_TO_RADIAN(angle);
	TRY( primal_rotate( surface_primal, dx, dy, dz, angle ), 
	     "primal_translate error" );
      }
      if( strcmp(string,"scale") == 0 ) {
	fscanf( f, "%lf\n", &scale );
	TRY( primal_scale_about( surface_primal, 0.0, 0.0, 0.0, scale ), 
	     "primal_scale_about error" );
      }
      if( strcmp(string,"flip_yz") == 0 ) {
	TRY( primal_flip_yz( surface_primal ), 
	     "primal_flip_yz error" );
      }
      if( strcmp(string,"flip_zy") == 0 ) {
	TRY( primal_flip_zy( surface_primal ), 
	     "primal_flip_zy error" );
      }
      if( strcmp(string,"reflect_y") == 0 ) {
	TRY( primal_reflect_y( surface_primal ), 
	     "primal_reflect_y error" );
      }
      if( strcmp(string,"massoud") == 0 ) {
	fscanf( f, "%s\n", massoud_filename );
	TRY( primal_apply_massoud( surface_primal, massoud_filename, 
				   (0 == partition) ),
	     "primal_apply_massoud error" );
      }
      if( strcmp(string,"faces") == 0 ) {
	read_faces = TRUE;
      }
    }

  if ( read_faces )
    {
      bcs = set_create(10,10);
      NOT_NULL(bcs, "Set bcs NULL");
      
      while ( !feof( f ) )
	{
	  bc_found = fscanf( f, "%d", &bc );
	  if ( 1 == bc_found )
	    TRY( set_insert( bcs, bc ), "set_insert bc into bcs");
	}

      if ( 0 == set_size(bcs) )
	{
	  TRY( KNIFE_FAILURE, 
	       "error specifying faces for cut surface in knife input file" );
	}
    }
  else
    {
      bcs = NULL;
    }

  surface = surface_from( surface_primal, bcs, 
			  inward_pointing_surface_normal );
  NOT_NULL(surface, "surface NULL");
  if ( 0 == surface_ntriangle(surface) )
    {
      printf("giving up in knife_required_local_dual, surface has no faces\n");
      *knife_status = KNIFE_NOT_FOUND;
      return;
    }

  TRY( primal_establish_all( volume_primal ), "primal_establish_all" );

  domain = domain_create( volume_primal, surface );
  NOT_NULL(domain, "domain NULL");

  TRY( domain_required_local_dual( domain, required ), 
       "domain_required_local_dual" );

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_cut,KNIFE_CUT)
  ( int *nodedim, int *required,
    int *knife_status )
{
  char tecplot_file_name[1025];
  if ( *nodedim != primal_nnode(volume_primal) )
    {
      printf("%s: %d: knife_cut_ wrong nnode %d %d\n",
	     __FILE__,__LINE__,*nodedim,domain_npoly( domain ));
      *knife_status = KNIFE_ARRAY_BOUND;
      return;
    }

  logger_message( FORTRAN_LOGGER_LEVEL, "create_dual");

  TRY( domain_create_dual( domain, required ), "domain_required_local_dual" );

  logger_message( FORTRAN_LOGGER_LEVEL, "subtract");

  TRY( domain_boolean_subtract( domain ), "boolean subtract" );

  if (FALSE) 
    {
      sprintf( tecplot_file_name, "knife_cut%03d.t", partition );
      domain_tecplot( domain, tecplot_file_name );
    }

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_dual_topo,KNIFE_DUAL_TOPO)
  ( int *nodedim, int *topo,
    int *knife_status )
{
  int node;

  logger_message( FORTRAN_LOGGER_LEVEL, "topo");

  if ( *nodedim != domain_npoly( domain ) )
    {
      printf("%s: %d: knife_dual_topo_ wrong nnode %d %d\n",
	     __FILE__,__LINE__,*nodedim,domain_npoly( domain ));
      *knife_status = KNIFE_ARRAY_BOUND;
      return;
    }

  for ( node = 0; node < domain_npoly(domain); node++ )
    {
      topo[node] = domain_topo(domain,node);
    }

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_make_dual_required,KNIFE_MAKE_DUAL_REQUIRED)
  ( int *node, int *knife_status )
{

  *knife_status = KNIFE_SUCCESS;

  if ( NULL != domain_poly(domain,(*node)-1) ) return;
  TRY( domain_add_interior_poly( domain, (*node)-1 ), 
       "domain_add_interior_poly" );
}

void FC_FUNC_(knife_dual_regions,KNIFE_DUAL_REGIONS)
  ( int *node, int *regions, int *knife_status )
{
  Poly poly;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL( poly, "poly NULL in knife_dual_regions_");

  TRY( poly_regions( poly, regions ), "poly_nregions" );

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_poly_centroid_volume,KNIFE_POLY_CENTRIOD_VOLUME)
  ( int *node, int *region,
    double *x, double *y, double *z, 
    double *volume,
    int *knife_status )
{
  double xyz[3], center[3];
  Poly poly;

  TRY( primal_xyz(domain_primal(domain),(*node)-1,xyz), "primal_xyz" );

  center[0] = xyz[0];
  center[1] = xyz[1];
  center[2] = xyz[2];

  poly = domain_poly(domain,(*node)-1);
  NOT_NULL( poly, "poly NULL in knife_poly_centroid_volume_");

  *knife_status = poly_centroid_volume(poly,*region,xyz,center,volume);

  *x = center[0];
  *y = center[1];
  *z = center[2];
}

void FC_FUNC_(knife_ntriangles_between_poly,KNIFE_NTRIANGLES_BETWEEN_POLY)
  ( int *node1, int *region1, 
    int *node2, int *region2,
    int *nsubtri,
    int *knife_status )
{
  int n;
  int edge;
  Poly poly1, poly2;
  Node node;

  TRY( primal_find_edge( volume_primal, (*node1)-1, (*node2)-1, &edge ), 
       "no edge found by primal_edge_between"); 

  node = domain_node_at_edge_center( domain, edge );
  NOT_NULL(node, "edge node NULL in knife_number_of_triangles_between_");

  poly1 = domain_poly( domain, (*node1)-1 );
  if ( NULL == poly1 )
    {
      printf("%s: %d: knife_ntriangles_between_poly_: poly1 warning\n",
	     __FILE__,__LINE__);
      TRY( domain_add_interior_poly( domain, (*node1)-1 ), "add int poly1");
      poly1 = domain_poly( domain, (*node1)-1 );
    }
  NOT_NULL( poly1, "poly1 NULL in knife_ntriangles_between_poly_");

  poly2 = domain_poly( domain, (*node2)-1 );
  if ( NULL == poly2 )
    {
      printf("%s: %d: knife_ntriangles_between_poly_: poly2 warning\n",
	     __FILE__,__LINE__);
      TRY( domain_add_interior_poly( domain, (*node2)-1 ), "add int poly2");
      poly2 = domain_poly( domain, (*node2)-1 );
    }
  NOT_NULL( poly2, "poly2 NULL in knife_ntriangles_between_poly_");

  TRY( poly_nsubtri_between( poly1, *region1, poly2, *region2, node, &n ), 
       "poly_nsubtri_between" );
  
  *nsubtri = n;
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_triangles_between_poly,KNIFE_TRIANGLES_BETWEEN_POLY)
  ( int *node1, int *region1,
    int *node2, int *region2,
    int *nsubtri,
    double *triangle_node0,
    double *triangle_node1,
    double *triangle_node2,
    double *triangle_normal,
    double *triangle_area,
    int *knife_status )
{
  int edge;
  Poly poly1, poly2;
  Node node;

  poly1 = domain_poly( domain, (*node1)-1 );
  NOT_NULL( poly1, "poly1 NULL in knife_ntriangles_between_poly_");

  poly2 = domain_poly( domain, (*node2)-1 );
  NOT_NULL( poly2, "poly2 NULL in knife_ntriangles_between_poly_");

  TRY( primal_find_edge( volume_primal, (*node1)-1,  (*node2)-1, &edge ), 
       "no edge found by primal_edge_between"); 

  node = domain_node_at_edge_center( domain, edge );
  NOT_NULL(node, "edge node NULL in knife_triangles_between_");

  TRY( poly_subtri_between( poly1, *region1, poly2, *region2, node, *nsubtri, 
			    triangle_node0, triangle_node1, triangle_node2,
			    triangle_normal, triangle_area ), 
       "poly_subtri_between" );
  
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_between_poly_sens,KNIFE_BETWEEN_POLY_SENS)
  ( int *node1, int *region1,
    int *node2, int *region2,
    int *nsubtri,
    int *parent_int,
    double *parent_xyz,
    int *knife_status )
{
  int edge;
  Poly poly1, poly2;
  Node node;
  int tri;

  poly1 = domain_poly( domain, (*node1)-1 );
  NOT_NULL( poly1, "poly1 NULL in knife_ntriangles_between_sens_");

  poly2 = domain_poly( domain, (*node2)-1 );
  NOT_NULL( poly2, "poly2 NULL in knife_ntriangles_between_sens_");

  TRY( primal_find_edge( volume_primal, (*node1)-1,  (*node2)-1, &edge ), 
       "no edge found by primal_edge_between"); 

  node = domain_node_at_edge_center( domain, edge );
  NOT_NULL(node, "edge node NULL in knife_triangles_between_");

  TRY( poly_between_sens( poly1, *region1, poly2, *region2, node, *nsubtri, 
			  parent_int, parent_xyz, surface ), 
       "poly_subtri_between" );
  
  for ( tri = 0 ; tri < 9*(*nsubtri) ; tri++ )
    {
      parent_int[tri]++;
    }

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_number_of_surface_triangles,KNIFE_NUMBER_OF_SURFACE_TRIANGLES)
  ( int *node, int *region,
    int *nsubtri,
    int *knife_status )
{
  int n;
  Poly poly;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_number_of_surface_triangles_");

  TRY( poly_surface_nsubtri( poly, *region, &n ), "poly_surface_nsubtri" );
  
  *nsubtri = n;
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_surface_triangles,KNIFE_SURFACE_TRIANGLES)
  ( int *node, int *region,
    int *nsubtri,
    double *triangle_node0,
    double *triangle_node1,
    double *triangle_node2,
    double *triangle_normal,
    double *triangle_area,
    int *triangle_tag,
    int *knife_status )
{
  Poly poly;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_surface_triangles_");

  TRY( poly_surface_subtri( poly, *region, *nsubtri, 
			    triangle_node0, triangle_node1, triangle_node2,
			    triangle_normal, triangle_area, triangle_tag ), 
       "poly_surface_subtri" );
  
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_surface_sens,KNIFE_SURFACE_SENS)
  ( int *node, int *region, int *nsubtri,
    int *constraint_type,
    double *constraint_xyz,
    int *knife_status )
{
  Poly poly;
  int tri;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_surface_triangles_");

  TRY( poly_surface_sens( poly, *region, *nsubtri, 
			  constraint_type,
			  constraint_xyz,
			  surface ), 
       "poly_surface_sens" );
  
  for ( tri = 0 ; tri < (*nsubtri) ; tri++ )
    {
      constraint_type[0+4*tri]++;
      constraint_type[1+4*tri]++;
      constraint_type[2+4*tri]++;
      constraint_type[3+4*tri]++;
    }

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_number_of_boundary_triangles,KNIFE_NUMBER_OF_BOUNDARY_TRIANGLES)
  ( int *node, int *face, int *region,
    int *nsubtri,
    int *knife_status )
{
  int n;
  Poly poly;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_number_of_boundary_triangles_");

  TRY( poly_boundary_nsubtri( poly, (*face)-1, *region, &n ), 
       "poly_boundary_nsubtri" );
  
  *nsubtri = n;
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_boundary_triangles,KNIFE_BOUNDARY_TRIANGLES)
  ( int *node, int *face, int *region,
    int *nsubtri,
    double *triangle_node0,
    double *triangle_node1,
    double *triangle_node2,
    double *triangle_normal,
    double *triangle_area,
    int *knife_status )
{
  Poly poly;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_boundary_triangles_");

  TRY( poly_boundary_subtri( poly, (*face)-1, *region, *nsubtri, 
			    triangle_node0, triangle_node1, triangle_node2,
			    triangle_normal, triangle_area ), 
       "poly_boundary_subtri" );
  
  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_boundary_sens,KNIFE_BOUNDARY_SENS)
  ( int *node, int *face, int *region, 
    int *nsubtri,
    int *parent_int,
    double *parent_xyz,
    int *knife_status )
{
  Poly poly;
  int i;

  poly = domain_poly( domain, (*node)-1 );
  NOT_NULL(poly, "poly NULL in knife_boundary_triangles_");

  TRY( poly_boundary_sens( poly, (*face)-1, *region, 
			   *nsubtri, 
			   parent_int,
			   parent_xyz,
			   surface ), 
       "poly_boundary_sens" );
  
  for ( i = 0 ; i < 9*(*nsubtri) ; i++ )
    {
      parent_int[i]++;
    }

  *knife_status = KNIFE_SUCCESS;
}


void FC_FUNC_(knife_cut_surface_dim,KNIFE_CUT_SURFACE_DIM)
  ( int *nnode, int *ntriangle, int *knife_status )
{

  if ( NULL == surface )
    {
      *nnode = 0;
      *ntriangle  = 0;
      *knife_status = KNIFE_NULL;
      return;
    }

  *nnode     = surface_nnode(surface);
  *ntriangle = surface_ntriangle(surface);

  *knife_status = KNIFE_SUCCESS;
}

void FC_FUNC_(knife_cut_surface,KNIFE_CUT_SURFACE)
  ( int *nnode, double *xyz, int *global,
    int *ntriangle, int *t2n, 
    int *knife_status )
{
  int node, tri;

  if ( NULL == surface )
    {
      *knife_status = KNIFE_NULL;
      return;
    }
  ASSERT_INT_EQ( *nnode, surface_nnode(surface), "number of nodes" );
  ASSERT_INT_EQ( *ntriangle, surface_ntriangle(surface),"number of triangles");

  TRY( surface_export_array( surface, xyz, global, t2n ), 
       "surface_export_array" );

  for ( node = 0 ; node < surface_nnode(surface) ; node++ )
    global[node]++;

  for ( tri = 0 ; tri < surface_ntriangle(surface) ; tri++ )
    {
      t2n[0+4*tri]++;
      t2n[1+4*tri]++;
      t2n[2+4*tri]++;
    }

  *knife_status = KNIFE_SUCCESS;
}


void FC_FUNC_(knife_free,KNIFE_FREE)( int *knife_status )
{
  primal_free( surface_primal );
  surface_primal = NULL;

  surface_free( surface );
  surface = NULL;

  primal_free( volume_primal );
  volume_primal = NULL;

  domain_free( domain );
  domain = NULL;

  partition = EMPTY;

  *knife_status = KNIFE_SUCCESS;
}
