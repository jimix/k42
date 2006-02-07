/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MyGraph.H,v 1.1 2005/09/01 21:27:40 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:
      A simple graph class that implements add/remove adge and transitive closure.
      Is used by deadlock.C to detect cycles in the resource allocaion graph.

 *****************************************************************************/
#define MAX_VERTICES 10

class MyGraph {
protected:
  int matrix[MAX_VERTICES][MAX_VERTICES];

public:
  MyGraph()
  {
    for(int i = 0; i < MAX_VERTICES; i++)
      for(int j = 0; j < MAX_VERTICES; j++)
        {
          matrix[i][j] = 0;
        }
  }

  int existsPath(int src, int dest)
  {
    int workMatrix[MAX_VERTICES][MAX_VERTICES];
    int i,j,k;

    // fill out working matrix
    for(i = 0; i < MAX_VERTICES; i++)
      for(j = 0; j < MAX_VERTICES; j++)
        {
          workMatrix[i][j] = matrix[i][j];
        }


    // transitive closure
    for (i = 0; i < MAX_VERTICES; i++) {
      for (j = 0; j < MAX_VERTICES; j++) {
        if (workMatrix[j][i]) {
          for (k = 0; k < MAX_VERTICES; k++) {
            if (workMatrix[i][k]) {
              workMatrix[j][k] = 1;
            }
          }
        }
      }
    }

    return workMatrix[src][dest];
  }

  void addEdge(int src, int dest)
  {
    matrix[src][dest] = 1;
  }

  void removeEdge(int src, int dest)
  {
    matrix[src][dest] = 0;
  }
};

#if 0
#include<stdio.h>
int main(int argc, char **argv)
{
  MyGraph g;

  g.addEdge(1,2);
  g.addEdge(2,3);
  g.addEdge(3,4);

  printf("Exists path 1 -> 4 : %d \n", g.existsPath(1,4));
  printf("Exists path 4 -> 1 : %d \n", g.existsPath(4,1));

  return 0;
}
#endif
