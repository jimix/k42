/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: litegraph.h,v 1.1 2005/09/01 21:27:35 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:
       A 'lite' graph interface. Hides away the complex implementation of graph operations
       (graph_internal.cc)
       
 *****************************************************************************/

#ifndef LITEGRAPH_H
#define LITEGRAPH_H

/*****************************************************************************
Define FULL if your graph could have multiple cycles. The only
operation available is 'print loops'.

Leave FULL undefined for graphs without cycles. Print path and print loop 
operations available.

*****************************************************************************/


//#define FULL 1

typedef unsigned vertexId_t;

extern void * newGraph();
extern void   deleteGraph(void * pg);
extern void   addEdge(void * pg, std::string fromVertex, std::string toVertex);
extern void   removeEdge(void * pg, std::string fromVertex, std::string toVertex);

#ifdef FULL

extern void   printLoops(void * pg);

#else

extern void   printGraph(void * pg, std::string start);
extern bool   hasCycle(void * pg);
extern bool   causesCycle(void * pg, std::string fromVertex, std::string toVertex);
extern void   printPath(void * pg, std::string fromVertex, std::string toVertex);

#endif //FULL

class LiteGraph {

  // PROTECTED
 protected:
  void * pg;

  // PUBLIC
 public:

  LiteGraph()
    {
      pg = ::newGraph();
    }
  void addEdge(std::string fromVertex, std::string toVertex) 
    {
      ::addEdge(pg, fromVertex, toVertex);
    }
  void removeEdge(std::string fromVertex, std::string toVertex) 
    {
      ::removeEdge(pg, fromVertex, toVertex);
    }

#ifdef FULL

  void printLoops()
    {
      return ::printLoops(pg);
    }

#else
  void printGraph(std::string start)
    {
      ::printGraph(pg, start);
    }
  bool hasCycle()
    {
      return ::hasCycle(pg);
    }
  bool causesCycle(std::string fromVertex, std::string toVertex)
    {
      return ::causesCycle(pg, fromVertex, toVertex);
    }
  void printPath(std::string fromVertex, std::string toVertex)
    {
      return ::printPath(pg, fromVertex, toVertex);
    }
#endif
};
#endif // LITEGRAPH_H
