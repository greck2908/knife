
/* storage for a primal tetrahedral grid */

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
#include <math.h>
#include <string.h>
#include "primal.h"
#include "set.h"

#define SWAP_INT(x) { \
    int y; \
    char *xp = (char *)&(x); \
    char *yp = (char *)&(y); \
    *(yp+3) = *(xp+0); \
    *(yp+2) = *(xp+1); \
    *(yp+1) = *(xp+2); \
    *(yp+0) = *(xp+3); \
    (x) = y; \
  }
#define SWAP_FLOAT(x) { \
    float y; \
    char *xp = (char *)&(x); \
    char *yp = (char *)&(y); \
    *(yp+3) = *(xp+0); \
    *(yp+2) = *(xp+1); \
    *(yp+1) = *(xp+2); \
    *(yp+0) = *(xp+3); \
    (x) = y; \
  }
#define SWAP_DOUBLE(x) { \
    double y; \
    char *xp = (char *)&(x); \
    char *yp = (char *)&(y); \
    *(yp+7) = *(xp+0); \
    *(yp+6) = *(xp+1); \
    *(yp+5) = *(xp+2); \
    *(yp+4) = *(xp+3); \
    *(yp+3) = *(xp+4); \
    *(yp+2) = *(xp+5); \
    *(yp+1) = *(xp+6); \
    *(yp+0) = *(xp+7); \
    (x) = y; \
  }

#define primal_test_malloc(ptr,fcn)		       \
  if (NULL == (ptr)) {				       \
    printf("%s: %d: malloc failed in %s\n",	       \
	   __FILE__,__LINE__,(fcn));		       \
    return NULL;				       \
  }

#define primal_test_status(ptr,fcn)		       \
  if (NULL == (ptr)) {				       \
    printf("%s: %d: malloc failed in %s\n",	       \
	   __FILE__,__LINE__,(fcn));		       \
    return KNIFE_MEMORY;			       \
  }

#define TRY(fcn,msg)					      \
  {							      \
    int code;						      \
    code = (fcn);					      \
    if (KNIFE_SUCCESS != code){				      \
      printf("%s: %d: %d %s\n",__FILE__,__LINE__,code,(msg)); \
      return code;					      \
    }							      \
  }

#define TRYN(fcn,msg)					      \
  {							      \
    int code;						      \
    code = (fcn);					      \
    if (KNIFE_SUCCESS != code){				      \
      printf("%s: %d: %d %s\n",__FILE__,__LINE__,code,(msg)); \
      return NULL;					      \
    }							      \
  }

#define NOT_NULL(pointer,msg)				      \
  if (NULL == (pointer)){				      \
    printf("%s: %d: %s\n",__FILE__,__LINE__,(msg));	      \
    return KNIFE_NULL;					      \
  }

#define AEN(thing1,thing2,msg)						\
  {									\
    if ((thing1) != (thing2)){						\
      printf("%s: %d: %s: %s\n",__FILE__,__LINE__,__func__,(msg));	\
      return NULL;						        \
    }									\
  }

#define AEF(thing1,thing2,msg)						\
  {									\
    if ((thing1) != (thing2)){						\
      printf("%s: %d: %s: %s\n",__FILE__,__LINE__,__func__,(msg));	\
      return KNIFE_FAILURE;						\
    }									\
  }

Primal primal_create(int nnode, int nface, int ncell)
{
  Primal primal;
  int i;

  primal = (Primal) malloc( sizeof(PrimalStruct) );
  primal_test_malloc(primal,"primal_create primal");

  primal->nnode = nnode;
  primal->nnode0 = primal->nnode;
  primal->xyz = (double *)malloc(3 * MAX(primal->nnode,1) * sizeof(double));
  primal_test_malloc(primal->xyz,"primal_create xyz");

  primal->nface = nface;
  primal->f2n = (int *)malloc(4 * MAX(primal->nface,1) * sizeof(int));
  for(i=0;i<4 * MAX(primal->nface,1);i++) primal->f2n[i] = EMPTY;
  primal_test_malloc(primal->f2n,"primal_create f2n");

  primal->ncell = ncell;
  primal->c2n = (int *)malloc(4 * MAX(primal->ncell,1) * sizeof(int));
  for(i=0;i<4 * MAX(primal->ncell,1);i++) primal->c2n[i] = EMPTY;
  primal_test_malloc(primal->c2n,"primal_create c2n");

  primal->face_adj = adj_create( primal->nnode, 3*primal->nface, 1000 );
  primal->cell_adj = adj_create( primal->nnode, 4*primal->ncell, 1000 );

  primal->nedge = EMPTY;
  primal->c2e = NULL;
  primal->e2n = NULL;

  primal->ntri = EMPTY;
  primal->c2t = NULL;
  primal->t2n = NULL;

  primal->surface_nnode = EMPTY;
  primal->surface_node = NULL;
  primal->surface_volume_node = NULL;

  return primal;
}

Primal primal_from_file( char *filename )
{
  Primal primal;
  int end_of_string;

  end_of_string = strlen(filename);

  if( strcmp(&filename[end_of_string-3],"tri") == 0 ) {
    primal = primal_from_tri( filename );
  } else if( strcmp(&filename[end_of_string-3],"rid") == 0 ) {
    primal = primal_from_fast( filename );
  } else {
    printf("%s: %d: %s %s\n",__FILE__,__LINE__,
	   "input file name extension unknown", filename);
    return NULL;
  }
  TNN( primal, "unable to create primal_from_file" );

  return primal;
}

Primal primal_from_fast( char *filename )
{
  Primal primal;
  int nnode, nface, ncell;
  int i;
  FILE *file;

  file = fopen(filename,"r");
  TNN( file, "unable to open fast file");

  fscanf(file,"%d %d %d",&nnode,&nface,&ncell);
  primal = primal_create( nnode, nface, ncell );
  TNN( primal, "unable to create primal");

  for( i=0; i<nnode ; i++ ) fscanf(file,"%lf",&(primal->xyz[0+3*i]));
  for( i=0; i<nnode ; i++ ) fscanf(file,"%lf",&(primal->xyz[1+3*i]));
  for( i=0; i<nnode ; i++ ) fscanf(file,"%lf",&(primal->xyz[2+3*i]));

  for( i=0; i<nface ; i++ ) {
    fscanf(file,"%d",&(primal->f2n[0+4*i]));
    fscanf(file,"%d",&(primal->f2n[1+4*i]));
    fscanf(file,"%d",&(primal->f2n[2+4*i]));
    primal->f2n[0+4*i]--;
    primal->f2n[1+4*i]--;
    primal->f2n[2+4*i]--;
    adj_add( primal->face_adj, primal->f2n[0+4*i], i);
    adj_add( primal->face_adj, primal->f2n[1+4*i], i);
    adj_add( primal->face_adj, primal->f2n[2+4*i], i);
  }

  for( i=0; i<nface ; i++ ) {
    fscanf(file,"%d",&(primal->f2n[3+4*i]));
  }

  for( i=0; i<ncell ; i++ ) {
    fscanf(file,"%d",&(primal->c2n[0+4*i]));
    fscanf(file,"%d",&(primal->c2n[1+4*i]));
    fscanf(file,"%d",&(primal->c2n[2+4*i]));
    fscanf(file,"%d",&(primal->c2n[3+4*i]));
    primal->c2n[0+4*i]--;
    primal->c2n[1+4*i]--;
    primal->c2n[2+4*i]--;
    primal->c2n[3+4*i]--;
    adj_add( primal->cell_adj, primal->c2n[0+4*i], i);
    adj_add( primal->cell_adj, primal->c2n[1+4*i], i);
    adj_add( primal->cell_adj, primal->c2n[2+4*i], i);
    adj_add( primal->cell_adj, primal->c2n[3+4*i], i);
  }

  fclose(file);

  TSN( primal_establish_all( primal ), "primal_establish_all" );

  return primal;
}

KNIFE_STATUS primal_interrogate_tri( char *filename )
{
  FILE *file;

  int record_header, record_footer;
  int nnode, nface;
  KnifeBool big_endian;
  int real_byte_size;

  file = fopen(filename,"r");
  if ( NULL == file )
    {
      printf("%s: %d: NULL file pointer to %s\n",
	     __FILE__,__LINE__,filename);
      return KNIFE_FILE_ERROR;
    }

  printf( "%s :\n",filename);

  AEF( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  AEF( 1, fread( &nnode, sizeof(int), 1, file), "nnode" );
  AEF( 1, fread( &nface, sizeof(int), 1, file), "nface" );
  AEF( 1, fread( &record_footer, sizeof(int), 1, file), "record footer" );

  if ( ( 134217728 != record_header ) && ( 8 != record_header ) )
    {
      printf(" is ascii, %d\n",record_header);
      return KNIFE_SUCCESS;
    }

  big_endian = (KnifeBool)( 134217728 == record_header );
  if ( big_endian )
    {
      SWAP_INT(record_header);
      SWAP_INT(nnode);
      SWAP_INT(nface);
      SWAP_INT(record_footer);

    }

  printf( "first recoard %d %d %d %d\n", 
	  record_header, nnode, nface, record_footer );

  AEF( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  if ( big_endian ) SWAP_INT(record_header);

  real_byte_size = record_header / 3 / nnode;

  printf( "xyzs are %d bytes each\n", real_byte_size );

  fclose(file);

  return KNIFE_SUCCESS;
}

Primal primal_from_tri( char *filename )
{
  FILE *file;
  int record_header;
  
  file = fopen(filename,"r");
  if ( NULL == file )
    {
      printf("%s: %d: NULL file pointer to %s\n",
	     __FILE__,__LINE__,filename);
      return NULL;
    }

  AEN( 1, fread( &record_header, sizeof(int), 1, file), "record header" );

  fclose( file );

  switch ( record_header )
    {
    case 8 : /* two 4 byte integers */
    case 134217728 :  /* two 4 byte integers - big endian */
      return primal_from_unformatted_tri( filename );
      break;
    }

  return primal_from_ascii_tri( filename );
}

Primal primal_from_ascii_tri( char *filename )
{
  Primal primal;
  int nnode, nface, ncell;
  int i;
  FILE *file;

  int greatest_read_face_id;
  KnifeBool read_error;

  file = fopen(filename,"r");
  if ( NULL == file )
    {
      printf("%s: %d: NULL file pointer to %s\n",
	     __FILE__,__LINE__,filename);
      return NULL;
    }

  fscanf(file,"%d %d",&nnode,&nface);
  ncell = 0;
  primal = primal_create( nnode, nface, ncell );
  if ( NULL == primal )
    {
      printf("%s: %d: primal_from_tri: primal creation \n",
	     __FILE__,__LINE__);
      return NULL;
    }

  for( i=0; i<nnode ; i++ ) {
    AEN( 1, fscanf(file,"%lf",&(primal->xyz[0+3*i])), "x read error" );
    AEN( 1, fscanf(file,"%lf",&(primal->xyz[1+3*i])), "y read error");
    AEN( 1, fscanf(file,"%lf",&(primal->xyz[2+3*i])), "z read error");
  }

  for( i=0; i<nface ; i++ ) {
    AEN( 1, fscanf(file,"%d",&(primal->f2n[0+4*i])), "face index0 read error");
    AEN( 1, fscanf(file,"%d",&(primal->f2n[1+4*i])), "face index1 read error");
    AEN( 1, fscanf(file,"%d",&(primal->f2n[2+4*i])), "face index2 read error");
    primal->f2n[0+4*i]--;
    primal->f2n[1+4*i]--;
    primal->f2n[2+4*i]--;
    adj_add( primal->face_adj, primal->f2n[0+4*i], i);
    adj_add( primal->face_adj, primal->f2n[1+4*i], i);
    adj_add( primal->face_adj, primal->f2n[2+4*i], i);
  }

  greatest_read_face_id = 0;
  read_error = FALSE;
  for( i=0; i<nface ; i++ ) {
    if ( !read_error && (1 == fscanf(file,"%d",&(primal->f2n[3+4*i]))) )
      {
	greatest_read_face_id = MAX(primal->f2n[3+4*i], greatest_read_face_id);
      }
    else
      {
	read_error = TRUE;
	primal->f2n[3+4*i] = greatest_read_face_id + 1;
      }
  }

  fclose(file);

  TRYN( primal_establish_all( primal ), "primal_establish_all" );

  return primal;
}

Primal primal_from_unformatted_tri( char *filename )
{
  Primal primal;
  FILE *file;
  int record_header, record_footer;
  int nnode, nface, ncell;
  int real_byte_size;
  float real4;
  double real8;
  int i,j;

  KnifeBool big_endian;

  file = fopen(filename,"r");
  if ( NULL == file )
    {
      printf("%s: %d: NULL file pointer to %s\n",
	     __FILE__,__LINE__,filename);
      return NULL;
    }

  AEN( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  AEN( 1, fread( &nnode, sizeof(int), 1, file), "nnode" );
  AEN( 1, fread( &nface, sizeof(int), 1, file), "nface" );
  AEN( 1, fread( &record_footer, sizeof(int), 1, file), "record footer" );
  big_endian = (KnifeBool)( 134217728 == record_header );
  if ( big_endian ) SWAP_INT(record_header);
  if ( big_endian ) SWAP_INT(nnode);
  if ( big_endian ) SWAP_INT(nface);
  if ( big_endian ) SWAP_INT(record_footer);

  AEN( record_header, record_footer, "sizing record mismatch");

  ncell = 0;
  primal = primal_create( nnode, nface, ncell );
  if ( NULL == primal )
    {
      printf("%s: %d: primal_from_tri: primal creation \n",
	     __FILE__,__LINE__);
      return NULL;
    }

  AEN( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  if ( big_endian ) SWAP_INT(record_header);

  real_byte_size = record_header / 3 / nnode;

  for( i=0; i<3*nnode ; i++ ) 
    {
      switch (real_byte_size )
	{
	case 4:
	  AEN( 1, fread( &real4, sizeof(float), 1, file), "4 byte xyz" );
	  if ( big_endian ) SWAP_FLOAT(real4);
	  primal->xyz[i] = (double)real4;
	  break;
	case 8:
	  AEN( 1, fread( &real8, sizeof(double), 1, file), "8 byte xyz" );
	  if ( big_endian ) SWAP_DOUBLE(real8);
	  primal->xyz[i] = (double)real8;
	  break;
	default:
	  printf("%s: %d: primal_from_tri: xyz byte size %d \n",
		 __FILE__,__LINE__,real_byte_size);
	  return NULL;
	}
    }

  AEN( 1, fread( &record_footer, sizeof(int), 1, file), "record footer" );
  if ( big_endian ) SWAP_INT(record_footer);
  AEN( record_header, record_footer, "xyz record mismatch");

  AEN( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  if ( big_endian ) SWAP_INT(record_header);
  AEN( record_header, 3*nface*4, "vertex record wrong size");

  for( j=0; j<nface ; j++ ) 
    {
      for( i=0; i<3 ; i++ ) 
	{
	  AEN( 1, fread( &(primal->f2n[i+4*j]), sizeof(int), 1, file), 
	       "4 byte vertex" );
	  if ( big_endian ) SWAP_INT(primal->f2n[i+4*j]);
	  primal->f2n[i+4*j]--;
	  adj_add( primal->face_adj, primal->f2n[i+4*j], j);
	}
    }
  
  AEN( 1, fread( &record_footer, sizeof(int), 1, file), "record footer" );
  if ( big_endian ) SWAP_INT(record_footer);
  AEN( record_header, record_footer, "vertex record mismatch");

  AEN( 1, fread( &record_header, sizeof(int), 1, file), "record header" );
  if ( big_endian ) SWAP_INT(record_header);
  AEN( record_header, nface*4, "component record wrong size");

  for( j=0; j<nface ; j++ ) 
    {
      AEN( 1, fread( &(primal->f2n[3+4*j]), sizeof(int), 1, file), 
	   "4 byte component" );
      if ( big_endian ) SWAP_INT(primal->f2n[3+4*j]);
    }
  
  AEN( 1, fread( &record_footer, sizeof(int), 1, file), "record footer" );
  if ( big_endian ) SWAP_INT(record_footer);
  AEN( record_header, record_footer, "componet record mismatch");

  TRYN( primal_establish_all( primal ), "primal_establish_all" );

  return primal;
}

void primal_free( Primal primal )
{
  if ( NULL == primal ) return;

  free( primal->xyz );
  free( primal->f2n );
  free( primal->c2n );

  adj_free( primal->face_adj );
  adj_free( primal->cell_adj );

  if ( NULL != primal->c2e ) free( primal->c2e );
  if ( NULL != primal->e2n ) free( primal->e2n );

  if ( NULL != primal->c2t ) free( primal->c2t );
  if ( NULL != primal->t2n ) free( primal->t2n );

  if ( NULL != primal->surface_node ) free( primal->surface_node );
  if ( NULL != primal->surface_volume_node ) 
    free( primal->surface_volume_node );

  free( primal );
}

KNIFE_STATUS primal_copy_volume( Primal primal, 
				 double *x, double *y, double *z,
				 int *c2n )
{
  int node, cell;

  for( node=0; node<primal_nnode(primal) ; node++ ) 
    {
      primal->xyz[0+3*node] = x[node];
      primal->xyz[1+3*node] = y[node];
      primal->xyz[2+3*node] = z[node];
    }

  for( cell=0; cell<primal_ncell(primal) ; cell++ ) 
    {
      primal->c2n[0+4*cell] = c2n[0+4*cell]-1;
      primal->c2n[1+4*cell] = c2n[1+4*cell]-1;
      primal->c2n[2+4*cell] = c2n[2+4*cell]-1;
      primal->c2n[3+4*cell] = c2n[3+4*cell]-1;
      adj_add( primal->cell_adj, primal->c2n[0+4*cell], cell);
      adj_add( primal->cell_adj, primal->c2n[1+4*cell], cell);
      adj_add( primal->cell_adj, primal->c2n[2+4*cell], cell);
      adj_add( primal->cell_adj, primal->c2n[3+4*cell], cell);
    }
  
  return KNIFE_SUCCESS;
}

static int nface_added = 0;

KNIFE_STATUS primal_copy_boundary( Primal primal, int face_id, 
				   int nboundnode, int *inode,
				   int leading_dim, int nface, int *f2n )
{
  int face;
  int node0, node1, node2;
  int face_node, node;

  for(face=0;face<nface;face++){
    if ( nface_added >= primal_nface(primal) )
      {
	printf("primal_copy_boundary nface_added array bound\n");
	return KNIFE_ARRAY_BOUND;
      }
    node0 = f2n[face+0*(leading_dim)] - 1;
    node1 = f2n[face+1*(leading_dim)] - 1;
    node2 = f2n[face+2*(leading_dim)] - 1;
    /* fun3d load balancing does not use inode map */
    if ( node0 < nboundnode ) node0 = inode[node0] - 1;
    if ( node1 < nboundnode ) node1 = inode[node1] - 1;
    if ( node2 < nboundnode ) node2 = inode[node2] - 1;
    primal->f2n[0+4*nface_added] = node0;
    primal->f2n[1+4*nface_added] = node1;
    primal->f2n[2+4*nface_added] = node2;
    primal->f2n[3+4*nface_added] = face_id;
    adj_add( primal->face_adj, primal->f2n[0+4*nface_added], nface_added);
    adj_add( primal->face_adj, primal->f2n[1+4*nface_added], nface_added);
    adj_add( primal->face_adj, primal->f2n[2+4*nface_added], nface_added);

    for (face_node=0;face_node<3;face_node++)
      {
	node = primal->f2n[face_node+4*nface_added];
	if ( node < 0 || node >= primal_nnode(primal) ) 
	  {
	    printf("%s: %d: %s: node out ot range %d %d %d\n",
		   __FILE__,__LINE__,
		   "primal_copy_boundary", face, node, primal_nnode(primal));
	    return KNIFE_ARRAY_BOUND;
	  }
      }

    nface_added++;
  }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_establish_all( Primal primal )
{
  NOT_NULL( primal, "primal NULL" );

  TRY( primal_establish_c2e(primal), "c2e" );
  TRY( primal_establish_c2t(primal), "c2t" );
  TRY( primal_establish_surface_node(primal), "surface_node" );

  return KNIFE_SUCCESS;
}

static void primal_set_cell_edge( Primal primal, 
				  int node0, int node1, int indx);

static void primal_set_cell_edge( Primal primal, 
				  int node0, int node1, int indx)
{
  AdjIterator it;
  int edge;
  int nodes[4];

  for ( it = adj_first(primal->cell_adj, node0);
	adj_valid(it);
	it = adj_next(it) )
    {
      primal_cell(primal, adj_item(it), nodes);
      for ( edge = 0 ; edge < 6; edge++ )
	if ( ( node0 == nodes[primal_cell_edge_node0(edge)] &&
	       node1 == nodes[primal_cell_edge_node1(edge)] )  ||
	     ( node1 == nodes[primal_cell_edge_node0(edge)] &&
	       node0 == nodes[primal_cell_edge_node1(edge)] ) )
	  primal->c2e[edge+6*adj_item(it)] = indx;
    }

}

KNIFE_STATUS primal_establish_c2e( Primal primal )
{
  int cell, edge;
  int nodes[4];
  int edge_index, node0, node1;

  primal->c2e = (int *)malloc(6*primal_ncell(primal)*sizeof(int));
  primal_test_status(primal->c2e,"primal_establish_c2e c2e");
  for(cell=0;cell<6*primal_ncell(primal);cell++) primal->c2e[cell]= EMPTY;

  primal->nedge = 0;
  for(cell=0;cell<primal_ncell(primal);cell++) 
    {
      primal_cell(primal, cell, nodes);
      for(edge=0;edge<6;edge++)
	if (EMPTY == primal->c2e[edge+6*cell])
	  {
	    primal_set_cell_edge(primal, 
				 nodes[primal_cell_edge_node0(edge)],
				 nodes[primal_cell_edge_node1(edge)],
				 primal->nedge);
	    primal->nedge++;
	  }
    }

  primal->e2n = (int *)malloc(2*primal->nedge*sizeof(int));
  primal_test_status(primal->e2n,"primal_establish_c2e e2n");
  for(cell=0;cell<primal_ncell(primal);cell++)
    {
      primal_cell(primal, cell, nodes);
      for(edge=0;edge<6;edge++)
	{
	  edge_index = primal->c2e[edge+6*cell];
	  node0 = nodes[primal_cell_edge_node0(edge)];
	  node1 = nodes[primal_cell_edge_node1(edge)];
	  primal->e2n[0+2*edge_index] = MIN(node0,node1);
	  primal->e2n[1+2*edge_index] = MAX(node0,node1);
	}
    }
 
  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_establish_c2t( Primal primal )
{
  int cell, side;
  int nodes[4];
  int tri_index;
  int node0, node1, node2;
  int other_cell, other_side;
  int n0,n1,n2;

  primal->c2t = (int *)malloc(4*primal_ncell(primal)*sizeof(int));
  primal_test_status(primal->c2t,"primal_establish_c2t c2t");
  for(cell=0;cell<4*primal_ncell(primal);cell++) primal->c2t[cell]= EMPTY;

  primal->ntri = 0;
  for(cell=0;cell<primal_ncell(primal);cell++) 
    {
      primal_cell(primal, cell, nodes);
      for(side=0;side<4;side++)
	if (EMPTY == primal->c2t[side+4*cell])
	  {
	    primal->c2t[side+4*cell] = primal->ntri;
	    node0 = nodes[primal_cell_side_node0(side)];
            node1 = nodes[primal_cell_side_node1(side)];
            node2 = nodes[primal_cell_side_node2(side)];
            if (KNIFE_SUCCESS == primal_find_cell_side(primal,
                                                       node1, node0, node2,
                                                       &other_cell,
                                                       &other_side))
              {
                if (EMPTY != other_cell)
                  primal->c2t[other_side+4*other_cell] = primal->ntri;
              }

	    primal->ntri++;
	  }
    }

  primal->t2n = (int *)malloc(3*primal->ntri*sizeof(int));
  primal_test_status(primal->t2n,"primal_establish_t2n t2n");
  for(cell=0;cell<primal_ncell(primal);cell++)
    {
      primal_cell(primal, cell, nodes);
      for(side=0;side<4;side++)
	{
	  tri_index = primal->c2t[side+4*cell];
	  n0 = nodes[primal_cell_side_node0(side)];
	  n1 = nodes[primal_cell_side_node1(side)];
	  n2 = nodes[primal_cell_side_node2(side)];
	  node0 = MIN(MIN(n0,n1),n2);
	  node2 = MAX(MAX(n0,n1),n2);
	  node1 = n0+n1+n2-node0-node2;
	  primal->t2n[0+3*tri_index] = node0;
	  primal->t2n[1+3*tri_index] = node1;
	  primal->t2n[2+3*tri_index] = node2;
	}
    }
 
  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_establish_surface_node( Primal primal )
{
  int node;
  int face_index, face_node;

  primal->surface_node = (int *)malloc( primal_nnode(primal) * sizeof(int) );
  primal_test_status(primal->surface_node,
		     "primal_establish_surface_node surface_node");
  for ( node = 0 ; node < primal_nnode(primal) ; node++) 
    primal->surface_node[node] = EMPTY;

  primal->surface_nnode = 0;
  for ( face_index = 0 ; face_index < primal_nface(primal) ; face_index++ )
    for (face_node=0;face_node<3;face_node++)
      {
	node = primal->f2n[face_node+4*face_index];
	if ( node < 0 || node >= primal_nnode(primal) ) 
	  {
	    printf("%s: %d: %s: node out of range: f %d of %d, n %d of %d\n",
		   __FILE__,__LINE__,
		   "primal_establish_surface_node", 
		   face_index, primal_nface(primal), 
		   node, primal_nnode(primal));
	    return KNIFE_ARRAY_BOUND;
	  }
	if (EMPTY == primal->surface_node[node]) 
	  {
	    primal->surface_node[node] = primal->surface_nnode;
	    primal->surface_nnode++;
	  }
      }

  primal->surface_volume_node = (int *)malloc( primal->surface_nnode * 
					       sizeof(int) );
  primal_test_status(primal->surface_volume_node,
		     "primal_establish_surface_node surface_volume_node");
  for ( node = 0 ; node < primal_nnode(primal) ; node++)
    if ( EMPTY != primal->surface_node[node] )
      primal->surface_volume_node[primal->surface_node[node]] = node;

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_xyz( Primal primal, int node_index, double *xyz)
{
  if (node_index < 0 || node_index >= primal_nnode(primal) ) 
    return KNIFE_ARRAY_BOUND;

  xyz[0] = primal->xyz[0+3*node_index];
  xyz[1] = primal->xyz[1+3*node_index];
  xyz[2] = primal->xyz[2+3*node_index];

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_face( Primal primal, int face_index, int *face)
{
  if (face_index < 0 || face_index >= primal_nface(primal) ) 
    return KNIFE_ARRAY_BOUND;

  face[0] = primal->f2n[0+4*face_index];
  face[1] = primal->f2n[1+4*face_index];
  face[2] = primal->f2n[2+4*face_index];
  face[3] = primal->f2n[3+4*face_index];

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_max_face_id( Primal primal, int *max_face_id)
{
  int face_index;

  *max_face_id = 0;

  for (face_index=0; face_index<primal_nface(primal); face_index++)
    *max_face_id = MAX(*max_face_id, primal->f2n[3+4*face_index]);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_cell( Primal primal, int cell_index, int *cell)
{
  if (cell_index < 0 || cell_index >= primal_ncell(primal) ) 
    return KNIFE_ARRAY_BOUND;

  cell[0] = primal->c2n[0+4*cell_index];
  cell[1] = primal->c2n[1+4*cell_index];
  cell[2] = primal->c2n[2+4*cell_index];
  cell[3] = primal->c2n[3+4*cell_index];

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_cell_center( Primal primal, int cell_index, double *xyz)
{
  double xyz0[3], xyz1[3], xyz2[3], xyz3[3];

  if (cell_index < 0 || cell_index >= primal_ncell(primal) ) 
    return KNIFE_ARRAY_BOUND;

  primal_xyz( primal, primal->c2n[0+4*cell_index], xyz0);
  primal_xyz( primal, primal->c2n[1+4*cell_index], xyz1);
  primal_xyz( primal, primal->c2n[2+4*cell_index], xyz2);
  primal_xyz( primal, primal->c2n[3+4*cell_index], xyz3);

  xyz[0] = 0.25*(xyz0[0] + xyz1[0] + xyz2[0] + xyz3[0]);
  xyz[1] = 0.25*(xyz0[1] + xyz1[1] + xyz2[1] + xyz3[1]);
  xyz[2] = 0.25*(xyz0[2] + xyz1[2] + xyz2[2] + xyz3[2]);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_edge( Primal primal, int edge_index, int *edge)
{
  if (edge_index < 0 || edge_index >= primal_nedge(primal) ) 
    return KNIFE_ARRAY_BOUND;

  edge[0] = primal->e2n[0+2*edge_index];
  edge[1] = primal->e2n[1+2*edge_index];

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_edge_center( Primal primal, int edge_index, double *xyz)
{
  double xyz0[3], xyz1[3];

  if (edge_index < 0 || edge_index >= primal_nedge(primal) ) 
    return KNIFE_ARRAY_BOUND;

  primal_xyz( primal, primal->e2n[0+2*edge_index], xyz0);
  primal_xyz( primal, primal->e2n[1+2*edge_index], xyz1);

  xyz[0] = 0.5*(xyz0[0] + xyz1[0]);
  xyz[1] = 0.5*(xyz0[1] + xyz1[1]);
  xyz[2] = 0.5*(xyz0[2] + xyz1[2]);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_tri( Primal primal, int tri_index, int *tri)
{
  if (tri_index < 0 || tri_index >= primal_ntri(primal) ) 
    return KNIFE_ARRAY_BOUND;

  tri[0] = primal->t2n[0+3*tri_index];
  tri[1] = primal->t2n[1+3*tri_index];
  tri[2] = primal->t2n[2+3*tri_index];

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_tri_center( Primal primal, int tri_index, double *xyz)
{
  double xyz0[3], xyz1[3], xyz2[3];

  if (tri_index < 0 || tri_index >= primal_ntri(primal) ) 
    return KNIFE_ARRAY_BOUND;

  primal_xyz( primal, primal->t2n[0+3*tri_index], xyz0);
  primal_xyz( primal, primal->t2n[1+3*tri_index], xyz1);
  primal_xyz( primal, primal->t2n[2+3*tri_index], xyz2);

  xyz[0] = (1.0/3.0)*(xyz0[0] + xyz1[0] + xyz2[0]);
  xyz[1] = (1.0/3.0)*(xyz0[1] + xyz1[1] + xyz2[1]);
  xyz[2] = (1.0/3.0)*(xyz0[2] + xyz1[2] + xyz2[2]);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_find_face_side( Primal primal, int node0, int node1,
                                    int *other_face_index, int *other_side ) 
{
  AdjIterator it;
  int side;
  int face[4];

  for ( it = adj_first(primal->face_adj, node0);
	adj_valid(it);
	it = adj_next(it) )
    {
      primal_face(primal, adj_item(it), face);
      for ( side = 0 ; side < 3; side++ )
	{
	  if ( node0 == face[primal_face_side_node0(side)] &&
	       node1 == face[primal_face_side_node1(side)] )
	    {
	      *other_face_index = adj_item(it);
	      *other_side = side;
	      return KNIFE_SUCCESS;
	    }
	}
    }
  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_find_cell_side( Primal primal, 
				    int node0, int node1, int node2,
                                    int *other_cell_index, int *other_side ) 
{
  AdjIterator it;
  int side;
  int cell[4];
  int n0, n1, n2;

  for ( it = adj_first(primal->cell_adj, node0);
	adj_valid(it);
	it = adj_next(it) )
    {
      primal_cell(primal, adj_item(it), cell);
      for ( side = 0 ; side < 4; side++ )
	{
	  n0 = cell[primal_cell_side_node0(side)];
	  n1 = cell[primal_cell_side_node1(side)];
	  n2 = cell[primal_cell_side_node2(side)];
	  if ( (n0 == node0 && n1 == node1 && n2 == node2 ) ||
	       (n1 == node0 && n2 == node1 && n0 == node2 ) ||
	       (n2 == node0 && n0 == node1 && n1 == node2 ) )
	    {
	      *other_cell_index = adj_item(it);
	      *other_side = side;
	      return KNIFE_SUCCESS;
	    }
	}
    }
  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_find_cell_edge( Primal primal, int cell, int edge, 
				    int *cell_edge ) 
{
  int canidate;
  
  for ( canidate = 0 ; canidate < 6 ; canidate++ )
    {
      if (primal_c2e(primal,cell,canidate) == edge)
	{
	  *cell_edge = canidate;
	  return KNIFE_SUCCESS;
	}
    }
  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_find_edge( Primal primal, 
			       int node0, int node1,
			       int *edge_index ) 
{
  AdjIterator it;
  int edge;
  int cell[4];
  int n0, n1;

  for ( it = adj_first(primal->cell_adj, node0);
	adj_valid(it);
	it = adj_next(it) )
    {
      primal_cell(primal, adj_item(it), cell);
      for ( edge = 0 ; edge < 6; edge++ )
	{
	  n0 = cell[primal_cell_edge_node0(edge)];
	  n1 = cell[primal_cell_edge_node1(edge)];
	  if ( (n0 == node0 && n1 == node1 ) ||
	       (n1 == node0 && n0 == node1 ) )
	    {
	      *edge_index = primal->c2e[edge+6*adj_item(it)];
	      return KNIFE_SUCCESS;
	    }
	}
    }
  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_find_tri( Primal primal, int node0, int node1, int node2, 
			      int *tri )
{
  int cell_index, side;

  if (KNIFE_SUCCESS == primal_find_cell_side( primal, node0, node1, node2, 
					      &cell_index, &side ) )
    {
      *tri =  primal_c2t(primal,cell_index,side);
      return KNIFE_SUCCESS;
    }

  /* search for reversed face on boundary */
  if (KNIFE_SUCCESS == primal_find_cell_side( primal, node1, node0, node2, 
					      &cell_index, &side ) )
    {
      *tri =  primal_c2t(primal,cell_index,side);
      return KNIFE_SUCCESS;
    }

  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_find_tri_side( Primal primal, int tri, int node0, int node1,
				   int *tri_side )
{
  int canidate;
  int n0, n1;

  if (tri < 0 || tri >= primal_ntri(primal) ) 
    return KNIFE_ARRAY_BOUND;

  for ( canidate = 0 ; canidate < 3 ; canidate++ )
    {
      n0 = primal->t2n[primal_face_side_node0(canidate)+3*tri];
      n1 = primal->t2n[primal_face_side_node1(canidate)+3*tri];

      if ( ( n0 == node0 && n1 == node1 ) ||
	   ( n1 == node0 && n0 == node1 ) )
	{
	  *tri_side = canidate;
	  return KNIFE_SUCCESS;
	}
    }

  return KNIFE_NOT_FOUND;
}

KNIFE_STATUS primal_scale_about( Primal primal, 
				 double x, double y, double z, double scale )
{
  int node;
  double dx, dy, dz;

  for( node=0; node<primal->nnode ; node++ ) 
    {
      dx = primal->xyz[0+3*node] - x;
      dy = primal->xyz[1+3*node] - y;
      dz = primal->xyz[2+3*node] - z;
      dx *= scale;
      dy *= scale;
      dz *= scale;
      primal->xyz[0+3*node] = x+dx;
      primal->xyz[1+3*node] = y+dy;
      primal->xyz[2+3*node] = z+dz;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_translate( Primal primal, 
			       double dx, double dy, double dz )
{
  int node;

  for( node=0; node<primal->nnode ; node++ ) 
    {
      primal->xyz[0+3*node] += dx;
      primal->xyz[1+3*node] += dy;
      primal->xyz[2+3*node] += dz;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_rotate( Primal primal, 
			    double nx, double ny, double nz, 
			    double angle_in_radians )
{
  int i;
  int node;
  double xyz[3];
  double norm[3];
  double dot, cross[3];
  double cos_angle, sin_angle;

  norm[0] = nx;
  norm[1] = ny;
  norm[2] = nz;
  
  cos_angle = cos(angle_in_radians);
  sin_angle = sin(angle_in_radians);

  for( node=0; node<primal->nnode ; node++ ) 
    {
      for( i=0; i<3 ; i++ ) 
	xyz[i] = primal->xyz[i+3*node];

      dot = ( xyz[0]*norm[0] + xyz[1]*norm[1] + xyz[2]*norm[2] );
      
      cross[0] = xyz[1]*norm[2] - xyz[2]*norm[1];
      cross[1] = xyz[2]*norm[0] - xyz[0]*norm[2];
      cross[2] = xyz[0]*norm[1] - xyz[1]*norm[0];

      for( i=0; i<3 ; i++ ) 
	primal->xyz[i+3*node] = 
	  xyz[i]*cos_angle + norm[i]*dot*(1.0-cos_angle) + cross[i]*sin_angle;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_flip_yz( Primal primal )
{
  int node;
  double x, y, z;
  for( node=0; node<primal->nnode ; node++ ) 
    {
      x = primal->xyz[0+3*node];
      y = primal->xyz[1+3*node];
      z = primal->xyz[2+3*node];
      primal->xyz[0+3*node] = x;
      primal->xyz[1+3*node] = -z;
      primal->xyz[2+3*node] = y;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_flip_zy( Primal primal )
{
  int node;
  double x, y, z;
  for( node=0; node<primal->nnode ; node++ ) 
    {
      x = primal->xyz[0+3*node];
      y = primal->xyz[1+3*node];
      z = primal->xyz[2+3*node];
      primal->xyz[0+3*node] = x;
      primal->xyz[1+3*node] = z;
      primal->xyz[2+3*node] = -y;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_reflect_y( Primal primal )
{
  int node;
  double x, y, z;
  for( node=0; node<primal->nnode ; node++ ) 
    {
      x = primal->xyz[0+3*node];
      y = primal->xyz[1+3*node];
      z = primal->xyz[2+3*node];
      primal->xyz[0+3*node] = x;
      primal->xyz[1+3*node] = -y;
      primal->xyz[2+3*node] = z;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_flip_face_normals( Primal primal )
{
  int face, temp;
  for( face=0; face<primal->nface ; face++ ) 
    {
      temp = primal->f2n[0+4*face];
      primal->f2n[0+4*face] = primal->f2n[1+4*face];
      primal->f2n[1+4*face] = temp;
    }

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_apply_massoud( Primal primal, char *massoud_filename, 
				   KnifeBool verbose )
{
  int var, nvar;
  char line[32768];
  int node, nnode;
  FILE *file;
  double x,y,z;
  int global;

  if (verbose) printf("applying massoud : %s\n", massoud_filename);

  file = fopen(massoud_filename,"r");
  if ( NULL == file )
    {
      printf("%s: %d: NULL file pointer to %s\n",
	     __FILE__,__LINE__,massoud_filename);
      return KNIFE_FAILURE;
    }

  strcpy(line,"");
  while ( strcmp( line, "VARIABLES" ) != 0 ) 
    {
      AEF(1,fscanf(file,"%s",line),"massoud read to 'VARIABLES' failed");
    }
  if (verbose) printf("%s\n",line);

  AEF(1,fscanf(file,"%s",line),"massoud read of = after 'VARIABLES' failed");
  AEF(1,fscanf(file,"%s",line),"massoud read of first var failed"); 

  nvar = 0;
  while ( strcmp( line, "ZONE" ) != 0 ) 
    {
      nvar++;
      if (verbose) printf("%6d %s\n",nvar,line);
      AEF(1,fscanf(file,"%s",line),"massoud read to 'ZONE' failed");
    }

  while ( strcmp( line, "I" ) != 0 ) 
    {
      AEF(1,fscanf(file,"%s",line),"massoud read to 'I' failed");
    }

  AEF(1,fscanf(file,"%s",line),"massoud read of = after 'T' failed");
  AEF(1,fscanf(file,"%d",&nnode),"massoud read of number of nodes");

  if (verbose) printf("%d nodes in massoud file\n",nnode);

  while ( strcmp( line, "F=FEPOINT" ) != 0 ) 
    {
      AEF(1,fscanf(file,"%s",line),"massoud read to 'F=FEPOINT' failed");
    }
  
  for ( node = 0; node < nnode; node++ )
    {
      AEF(4,fscanf(file,"%lf %lf %lf %d",&x,&y,&z,&global),
	  "massoud read of new xyz failed");
      global--;
      if ( global >= 0 && global < primal_nnode(primal) )
	{
	  primal->xyz[0+3*global] = x;
	  primal->xyz[1+3*global] = y;
	  primal->xyz[2+3*global] = z;
	}
      else
	{
	  fclose(file);
	  printf("%s: %d: massoud id %d not within %d nodes\n",
		 __FILE__,__LINE__,global,primal_nnode(primal));
	  return KNIFE_FAILURE;
	}
      for ( var = 4; var < nvar; var++ )
	AEF(1,fscanf(file,"%lf",&x),
	    "massoud read of sensitivity variables failed");
    }

  fclose(file);

  return KNIFE_SUCCESS;
}

Primal primal_subset( Primal primal, Set bcs )
{
  Primal subset;
  int max_face_id;
  int nnode, nface, nbcs;
  int *node_o2n, *face_o2n, *bcs_o2n;
  int face, node, i;
  int nodes[4];

  TRYN( primal_max_face_id( primal, &max_face_id ), 
	"primal_max_face_id failed" );

  node_o2n = (int *)malloc(MAX(primal_nnode(primal),1) * sizeof(int));
  primal_test_malloc(node_o2n,"primal_subset node_o2n");
  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    node_o2n[node] = EMPTY;

  face_o2n = (int *)malloc(MAX(primal_nface(primal),1) * sizeof(int));
  if ( NULL == face_o2n ) free( node_o2n );
  primal_test_malloc(node_o2n,"primal_subset face_o2n");
  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    face_o2n[face] = EMPTY;

  bcs_o2n = (int *)malloc(MAX(max_face_id,1) * sizeof(int));
  if ( NULL == bcs_o2n ) { free( node_o2n ); free( face_o2n ); }
  primal_test_malloc(node_o2n,"primal_subset bcs_o2n");
  for ( i = 0 ; i < max_face_id ; i++ )
    bcs_o2n[i] = EMPTY;

  nface = 0;
  nnode = 0;
  nbcs = 0;
  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      if (set_contains(bcs,nodes[3])) 
	{
	  face_o2n[face] = nface;
	  nface++;
	  for( i=0; i<3 ; i++ ) 
	    {
	      if ( EMPTY == node_o2n[nodes[i]] )
		{
		  node_o2n[nodes[i]] = nnode;
		  nnode++;
		}
	    }
	  if ( 1 > nodes[3] ) 
	    {
	      printf("low bc index %d\n", nodes[3]);
	      free( node_o2n ); free( face_o2n ); free( bcs_o2n );
	      return NULL;
	    }
	  if ( EMPTY == bcs_o2n[nodes[3]-1] )
	    {
	      bcs_o2n[nodes[3]-1] = nbcs+1; /* one-based numbering */
	      nbcs++;
	    }
	}
    }

  /* to keep the face ids in order */
  nbcs = 0;
  for ( i = 0 ; i < max_face_id ; i++ )
    if ( EMPTY != bcs_o2n[i] )
      {
	bcs_o2n[i] = nbcs+1; /* one-based numbering */
	nbcs++;
      }

  subset = primal_create( nnode, nface, 0 );
  if ( NULL == subset ) { free( node_o2n ); free( face_o2n ); free( bcs_o2n );}
  primal_test_malloc(node_o2n,"primal_subset subset");

  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      if ( EMPTY != node_o2n[node] )
	{
	  for( i=0; i<3 ; i++ )	  
	    subset->xyz[i+3*node_o2n[node]] = primal->xyz[i+3*node];
	}
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      if ( EMPTY != face_o2n[face] )
	{
	  primal_face( primal, face, nodes );
	  for( i=0; i<3 ; i++ )	  
	    subset->f2n[i+4*face_o2n[face]] = node_o2n[nodes[i]];
	  subset->f2n[3+4*face_o2n[face]] = 
	    bcs_o2n[nodes[3]-1]; /* one-based numbering */
	}
    }

  TRYN( primal_establish_all( subset ), "primal_establish_all" );

  return subset;
}

KNIFE_STATUS primal_export_tri( Primal primal, char *filename )
{
  FILE *f;
  int node, face;
  double xyz[3];
  int nodes[4];

  if (NULL == filename)
    {
      f = fopen( "primal.tri", "w" );
    } else {
      f = fopen( filename, "w" );
    }

  if ( NULL == f ) return KNIFE_FILE_ERROR;

  fprintf( f, "%d %d\n", primal_nnode(primal), primal_nface(primal) );

  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      primal_xyz( primal, node, xyz );
      fprintf( f, "%25.17e %25.17e %25.17e\n", xyz[0], xyz[1], xyz[2] );
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      fprintf( f, "%d %d %d\n", nodes[0]+1, nodes[1]+1, nodes[2]+1 );
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      fprintf( f, "%d\n", nodes[3] );
    }

  fclose(f);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_export_fast( Primal primal, char *filename )
{
  FILE *f;
  int node, face, cell;
  double xyz[3];
  int nodes[4];

  if (NULL == filename)
    {
      f = fopen( "primal.fgrid", "w" );
    } else {
      f = fopen( filename, "w" );
    }

  if ( NULL == f ) return KNIFE_FILE_ERROR;

  fprintf( f, "%d %d %d\n", 
	   primal_nnode(primal), primal_nface(primal), primal_ncell(primal) );

  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      primal_xyz( primal, node, xyz );
      fprintf( f, "%25.17e\n", xyz[0] );
    }
  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      primal_xyz( primal, node, xyz );
      fprintf( f, "%25.17e\n", xyz[1] );
    }
  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      primal_xyz( primal, node, xyz );
      fprintf( f, "%25.17e\n", xyz[2] );
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      fprintf( f, "%d %d %d\n", nodes[0]+1, nodes[1]+1, nodes[2]+1 );
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      fprintf( f, "%d\n", nodes[3] );
    }

  for ( cell = 0 ; cell < primal_ncell(primal) ; cell++ )
    {
      primal_cell( primal, cell, nodes );
      fprintf( f, "%d %d %d %d\n", 
	       nodes[0], nodes[1], nodes[2], nodes[3] );
    }

  fclose(f);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_export_single_zone_tec( Primal primal, char *filename )
{
  FILE *f;
  int node, face;
  double xyz[3];
  int nodes[4];

  if (NULL == filename)
    {
      f = fopen( "primal.t", "w" );
    } else {
      f = fopen( filename, "w" );
    }

  if ( NULL == f ) return KNIFE_FILE_ERROR;

  fprintf( f, "title=\"tecplot knife primal geometry file\"\n" );
  fprintf( f, "variables=\"x\",\"y\",\"z\"\n" );

  fprintf( f,
	   "zone t=surf, i=%d, j=%d, f=fepoint, et=triangle\n",
	   primal_nnode(primal), primal_nface(primal) );

  for ( node = 0 ; node < primal_nnode(primal) ; node++ )
    {
      primal_xyz( primal, node, xyz );
      fprintf( f, "%25.17e %25.17e %25.17e\n", xyz[0], xyz[1], xyz[2] );
    }

  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      primal_face( primal, face, nodes );
      fprintf( f, "%d %d %d\n", nodes[0]+1, nodes[1]+1, nodes[2]+1 );
    }

  fclose(f);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_export_tec( Primal primal, char *filename )
{
  FILE *f;
  int node, face;
  double xyz[3];
  int nodes[4];
  int *g2l, *l2g;
  Set set;
  int zone, faceid;
  int nnode, ntri;

  if (NULL == filename)
    {
      f = fopen( "primal.t", "w" );
    } else {
      f = fopen( filename, "w" );
    }

  if ( NULL == f ) return KNIFE_FILE_ERROR;

  fprintf( f, "title=\"tecplot knife primal geometry file\"\n" );
  fprintf( f, "variables=\"x\",\"y\",\"z\"\n" );

  NOT_NULL( set = set_create( 100, 100 ), "set creation failed");
  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      TRY( primal_face( primal, face, nodes ), "face");
      TRY(set_insert( set, nodes[3] ), "set insert" );
    }

  NOT_NULL( g2l = (int *) malloc( primal_nnode(primal)*sizeof(int) ),
	    "allocate g2l" );

  for ( zone = 0 ; zone < set_size(set) ; zone++ )
    {

      faceid = set_item( set, zone );

      for( node = 0; node < primal_nnode(primal) ; node++ )
	g2l[node] = EMPTY;

      ntri = 0;
      for ( face = 0 ; face < primal_nface(primal) ; face++ )
	{
	  TRY( primal_face( primal, face, nodes ), "face");
	  if ( faceid == nodes[3] )
	    {
	      ntri++;
	      g2l[nodes[0]] = 1;
	      g2l[nodes[1]] = 1;
	      g2l[nodes[2]] = 1;
	    }
	}

      nnode = 0;
      for( node = 0; node < primal_nnode(primal) ; node++ )
	if ( EMPTY != g2l[node] )
	  {
	    g2l[node] = nnode;
	    nnode++;
	  }

      fprintf( f,
	       "zone t=face%d, i=%d, j=%d, f=fepoint, et=triangle\n",
	       faceid, nnode, ntri );
      
      NOT_NULL( l2g = (int *) malloc( nnode*sizeof(int) ),
		"allocate l2g" );
      for ( node = 0 ; node < primal_nnode(primal) ; node++ )
	if ( EMPTY != g2l[node] )
	  l2g[g2l[node]] = node;

      for ( node = 0 ; node < nnode ; node++ )
	{
	  TRY( primal_xyz( primal, l2g[node], xyz ), "xyz");
	  fprintf( f, "%25.17e %25.17e %25.17e\n", xyz[0], xyz[1], xyz[2] );
	}

      free( l2g );

      for ( face = 0 ; face < primal_nface(primal) ; face++ )
	{
	  TRY( primal_face( primal, face, nodes ), "face");
	  if ( faceid == nodes[3] )
	    {
	      fprintf( f, "%d %d %d\n", 
		       g2l[nodes[0]]+1, g2l[nodes[1]]+1, g2l[nodes[2]]+1 );
	    }
	}
    }

  free(g2l);
  set_free(set);

  fclose(f);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_export_vtk( Primal primal, char *filename )
{
  FILE *f;
  int node, face;
  double xyz[3];
  int nodes[4];
  int *g2l, *l2g;
  Set set;
  int zone, faceid;
  int nnode, ntri;

  if (NULL == filename)
    {
      f = fopen( "primal.vtu", "w" );
    } else {
      f = fopen( filename, "w" );
    }

  if ( NULL == f ) return KNIFE_FILE_ERROR;

  fprintf( f, "<?xml version=\"1.0\"?>\n" );
  fprintf( f, "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n" );
  fprintf( f, "  <UnstructuredGrid>\n" );

  NOT_NULL( set = set_create( 100, 100 ), "set creation failed");
  for ( face = 0 ; face < primal_nface(primal) ; face++ )
    {
      TRY( primal_face( primal, face, nodes ), "face");
      TRY(set_insert( set, nodes[3] ), "set insert" );
    }

  NOT_NULL( g2l = (int *) malloc( primal_nnode(primal)*sizeof(int) ),
	    "allocate g2l" );

  for ( zone = 0 ; zone < set_size(set) ; zone++ )
    {

      faceid = set_item( set, zone );

      for( node = 0; node < primal_nnode(primal) ; node++ )
	g2l[node] = EMPTY;

      ntri = 0;
      for ( face = 0 ; face < primal_nface(primal) ; face++ )
	{
	  TRY( primal_face( primal, face, nodes ), "face");
	  if ( faceid == nodes[3] )
	    {
	      ntri++;
	      g2l[nodes[0]] = 1;
	      g2l[nodes[1]] = 1;
	      g2l[nodes[2]] = 1;
	    }
	}

      nnode = 0;
      for( node = 0; node < primal_nnode(primal) ; node++ )
	if ( EMPTY != g2l[node] )
	  {
	    g2l[node] = nnode;
	    nnode++;
	  }

      fprintf( f,
	       "    <Piece NumberOfPoints=\"%d\" NumberOfCells=\"%d\">\n",
	       nnode, ntri );
      
      NOT_NULL( l2g = (int *) malloc( nnode*sizeof(int) ),
		"allocate l2g" );
      for ( node = 0 ; node < primal_nnode(primal) ; node++ )
	if ( EMPTY != g2l[node] )
	  l2g[g2l[node]] = node;

      fprintf( f,"      <Points Scalars=\"my_scalars\">\n" );
      fprintf( f,"        <DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">\n" );
      
      for ( node = 0 ; node < nnode ; node++ )
	{
	  TRY( primal_xyz( primal, l2g[node], xyz ), "xyz");
	  fprintf( f, "%25.17e %25.17e %25.17e\n", xyz[0], xyz[1], xyz[2] );
	}
      fprintf( f,"        </DataArray>\n" );
      fprintf( f,"      </Points>\n" );

      free( l2g );

      fprintf( f,"      <Cells>\n" );
      fprintf( f,"        <DataArray type=\"Int32\" Name=\"connectivity\" format=\"ascii\">\n" );
      for ( face = 0 ; face < primal_nface(primal) ; face++ )
	{
	  TRY( primal_face( primal, face, nodes ), "face");
	  if ( faceid == nodes[3] )
	    {
	      fprintf( f, "%d %d %d\n", 
		       g2l[nodes[0]], g2l[nodes[1]], g2l[nodes[2]] );
	    }
	}
      fprintf( f,"        </DataArray>\n" );

      fprintf( f,"        <DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">\n" );
      for ( face = 0 ; face < ntri ; face++ )
	{
	  fprintf( f, "%d\n", 3*(face+1) );
	}
      fprintf( f,"        </DataArray>\n" );

      fprintf( f,"        <DataArray type=\"Int32\" Name=\"types\" format=\"ascii\">\n" );
      for ( face = 0 ; face < ntri ; face++ )
	{
	  fprintf( f, "%d\n", 5 );
	}
      fprintf( f,"        </DataArray>\n" );

      fprintf( f,"      </Cells>\n" );
      fprintf( f,"    </Piece>\n" );
    }
  fprintf( f,"  </UnstructuredGrid>\n" );
  fprintf( f,"</VTKFile>\n" );

  free(g2l);
  fclose(f);

  return KNIFE_SUCCESS;
}

