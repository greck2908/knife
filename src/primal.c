
/* storage for a primal tetrahedral grid */

/* $Id$ */

/* Michael A. Park (Mike Park)
 * Computational AeroSciences Branch
 * NASA Langley Research Center
 * Hampton, VA 23681
 * Phone:(757) 864-6604
 * Email: Mike.Park@NASA.Gov
 */

#include <stdlib.h>
#include <stdio.h>
#include "primal.h"

Primal primal_create(int nnode, int nface, int ncell)
{
  Primal primal;

  primal = (Primal) malloc( sizeof(PrimalStruct) );
  primal_test_malloc(primal,"primal_create primal");

  primal->nnode = nnode;
  primal->xyz = (double *)malloc(3 * MAX(primal->nnode,1) * sizeof(double));
  primal_test_malloc(primal->xyz,"primal_create xyz");

  primal->nface = nface;
  primal->f2n = (int *)malloc(4 * MAX(primal->nface,1) * sizeof(int));
  primal_test_malloc(primal->f2n,"primal_create f2n");

  primal->ncell = ncell;
  primal->c2n = (int *)malloc(4 * MAX(primal->ncell,1) * sizeof(int));
  primal_test_malloc(primal->c2n,"primal_create c2n");

  primal->face_adj = adj_create( primal->nnode, primal->nface, 1000 );
  primal->cell_adj = adj_create( primal->nnode, primal->ncell, 1000 );

  primal->nedge = EMPTY;
  primal->c2e = NULL;
  primal->e2n = NULL;

  primal->ntri = EMPTY;
  primal->c2t = NULL;
  primal->t2n = NULL;

  return primal;
}

Primal primal_from_FAST( char *filename )
{
  Primal primal;
  int nnode, nface, ncell;
  int i;
  FILE *file;

  file = fopen(filename,"r");
  fscanf(file,"%d %d %d",&nnode,&nface,&ncell);
  primal = primal_create( nnode, nface, ncell );

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

  primal_establish_c2e(primal);
  primal_establish_c2t(primal);

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

  free( primal );
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

  xyz[1] = 0.25*(xyz0[1] + xyz1[1] + xyz2[1] + xyz3[1]);
  xyz[2] = 0.25*(xyz0[2] + xyz1[2] + xyz2[2] + xyz3[2]);
  xyz[3] = 0.25*(xyz0[3] + xyz1[3] + xyz2[3] + xyz3[3]);

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

  xyz[1] = 0.5*(xyz0[1] + xyz1[1]);
  xyz[2] = 0.5*(xyz0[2] + xyz1[2]);
  xyz[3] = 0.5*(xyz0[3] + xyz1[3]);

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

  xyz[1] = (1.0/3.0)*(xyz0[1] + xyz1[1] + xyz2[1]);
  xyz[2] = (1.0/3.0)*(xyz0[2] + xyz1[2] + xyz2[2]);
  xyz[3] = (1.0/3.0)*(xyz0[3] + xyz1[3] + xyz2[3]);

  return KNIFE_SUCCESS;
}

KNIFE_STATUS primal_find_face_side( Primal primal, int node0, int node1,
                                    int *other_face_index, int *other_side ) {
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
                                    int *other_cell_index, int *other_side ) {
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

KNIFE_STATUS primal_find_edge( Primal primal, 
			       int node0, int node1,
			       int *edge_index ) {
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

