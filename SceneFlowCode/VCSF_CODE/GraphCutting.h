////////////////////////////////////////////////////////////////
//      Container for graph cut by LS-AUX or truncation       //
////////////////////////////////////////////////////////////////

/*
Copyright (C) 2014 Christoph Vogel, PhD. Student ETH Zurich
All rights reserved.

This software is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this software.
*/

#ifndef __Graph_Cutting__h
#define __Graph_Cutting__h

#include "Math/VectorT.h"
#include "Math/Mat3x3T.h"
#include <map>
using namespace std;
using namespace Math;

//#include "DataContainers.h"
#include "../maxflow-v3.03.src/graph.h"

//simple truncation delivers bad results and energies - just for testing
//#define _truncation_test_

template<typename Scalar> 
class GraphCutContainer
{
  	
  typedef Graph<int,int,int> GraphType;
  typedef Math::Mat3x3T<Scalar>     M3;
  typedef Math::VectorT<Scalar, 4>  P4;
  typedef Math::VectorT<Scalar, 3>  P3;
  typedef Math::VectorT<Scalar, 2>  P2;
  typedef Math::VectorT<int, 4>  P4i;
  typedef Math::VectorT<int, 2>  P2i;

  typedef std::pair<int,int> mapkey;
  typedef std::map< mapkey, P4 > edgeMap;


public:

  GraphCutContainer() : nNodes(0), nEdges(0), g(NULL), lsa0Score(0), non_subs(0), num_edges(0) {};

  ~GraphCutContainer() {deleteMem();};

  void deleteMem()
  {
    if (g!=NULL) 
      delete g;
    g=NULL;
  }

  void create(int _nNodes, int _nEdges)
  {
    nNodes = _nNodes;
    nEdges = _nEdges;
    deleteMem();
  	g = new GraphType( nNodes, nEdges);
  }

  void setup(int nVars )
  {
  	g->add_node(nVars);
    unaries.clear();
    unaries.assign(nVars, P2(0.,0.));
    lsaAuxPairs.clear();
    lsaAuxScores.clear();
    lsa0Score =0;
    multiEdges.clear();

//    binaryPairs.clear();
//    binaries.clear();
  }

  void Reset() {reset();};

  void reset()
  {
    non_subs = 0;
    num_edges = 0;
    g->reset();
  };

  // finializes the submodular graph
  void finalize()
  {
    for (int i=0;i<unaries.size(); i++)
    {
      Scalar minUn = std::min(unaries[i][0], unaries[i][1]);
//      g->add_tweights( i, unaries[i][0]-minUn, unaries[i][1]-minUn );

      // energy with all 0 is - since no 00 edge exists only the unaries:
      if ( !lsaAuxPairs.empty() )
      {
        lsa0Score += int(unaries[i][0]-minUn);
      }

      g->add_tweights( i, unaries[i][1]-minUn, unaries[i][0]-minUn );
    }
  };

  int solve()
  {
    int flow =0;
    if ( !lsaAuxPairs.empty() )
    {
      flow = solve_lsa();
      return flow;
    }
  	flow = g -> maxflow();
  //	printf("Flow = %d\n", flow);
    return flow;
  }

  int GetLabel(int j)
  {
    return (g->what_segment(j) != GraphType::SOURCE);// 0 if 
  }

  void AddUnaryTerm( int p, const P2& values )
  {
    unaries[p] += values;
  }

  void AddUnaryTerm( int p, Scalar u0, Scalar u1)
  {
    unaries[p] += P2(u0,u1);
  }

  void AddPairwiseTerm( int p, int q, Scalar f00, Scalar f01, Scalar f10, Scalar f11 )
  {
    AddPairwiseTerm( p, q, P4(f00,f01, f10, f11) );
  }


  void AddPairwiseTerm( int p, int q, P4& ff )
  {
    Scalar min_e00_01 = std::min(ff[0], ff[1]);
    ff[0] -= min_e00_01;
    ff[1] -= min_e00_01;
    unaries[p][0] += min_e00_01;

    //
    Scalar min_e10_11 = std::min(ff[2], ff[3]);
    ff[2] -= min_e10_11;
    ff[3] -= min_e10_11;
    unaries[p][1] += min_e10_11;
    /////////
    Scalar min_e00_10 = std::min(ff[0], ff[2]);
    ff[0] -= min_e00_10;
    ff[2] -= min_e00_10;
    unaries[q][0] += min_e00_10;

    //
    Scalar min_e01_11 = std::min(ff[1], ff[3]);
    ff[1] -= min_e01_11;
    ff[3] -= min_e01_11;
    unaries[q][1] += min_e01_11;

//    if (ff[1] != ff[2]) // otherwise pointless
    g->add_edge( p, q, ff[1], ff[2] );
  }

  void AddPairwiseTermTrunc( int p, int q, Scalar f00, Scalar f01, Scalar f10, Scalar f11 )
  {
    P4 temp = P4( f00,f01,f10,f11);
    AddPairwiseTermTrunc( p, q, temp );
  }

  /// truncate if not submodular
  void AddPairwiseTermTrunc( int p, int q, P4& ff )
  {
//    int non_sub=0;
    if (ff[0]+ff[3] > ff[1]+ff[2])
    {
      if (ff[0]>ff[3])
        ff[0] = std::max( Scalar(0), ff[1]+ff[2]-ff[3]);// submodular now
      else
        ff[3] = std::max( Scalar(0), ff[1]+ff[2]-ff[0]);// submodular now
    }
    if (ff[0]+ff[3] > ff[1]+ff[2])
    {
      if (ff[0]>ff[3])
        ff[0] = std::max( Scalar(0), ff[1]+ff[2]-ff[3]);// submodular now
      else
        ff[3] = std::max( Scalar(0), ff[1]+ff[2]-ff[0]);// submodular now
//      non_sub++;
    }

    AddPairwiseTerm( p, q, ff );

  }
  /////////////////

  void StorePairwiseTerm( int p, int q, const P4& ff )
  {
    mapkey mykey( std::min(p,q), std::max(p,q) );

    if ( multiEdges.find(mykey) == multiEdges.end() )
      multiEdges[mykey] = ff;
    else
      multiEdges[mykey] += ff;
  }

  void mergeParallelEdges_Aux( )
  {
    typename edgeMap::iterator it(multiEdges.begin()) , it_end (multiEdges.end());
    for (;it!=it_end;it++)
    {
	  P4 temp = it->second - P4( (it->second).minComp());
      AddPairwiseTermLSAAux( (it->first).first, (it->first).second, temp );
    }
  }


  void AddPairwiseTermLSAAux( int p, int q, P4& ff )
  {
    // debug: falls back to truncation
#ifdef _truncation_test_
    AddPairwiseTermTrunc( p, q, ff );return;
#endif

    //    int non_sub=0;
    // submod:
    num_edges++;
    if (ff[0]+ff[3] > ff[1]+ff[2] + 0.1)
    {
      non_subs++;
      AddNonSubPairwiseTerm_lsa_aux( p,q, ff );
    }
    else
      AddPairwiseTerm( p, q, ff );
  }

  int get_NonSubs()  {return non_subs;};
  int get_NumEdges() {return num_edges;}; 

  int solve_lsa()
  {
    // assume that i have a reasonable energy for 00 solution, now approx and bound at 00:

    // add 00 terms:
//    std::vector<int> prevSolution( unaries.size(), 0 );

    // 
    std::vector<int> prevSolutionP( lsaAuxPairs.size(), 0 );
    std::vector<int> prevSolutionQ( lsaAuxPairs.size(), 0 );
    for (int i=0; i < lsaAuxPairs.size(); i++ )
    {
      prevSolutionP[ i ] = lsaAuxScores[i]/2;
      prevSolutionQ[ i ] = lsaAuxScores[i]/2;
      g->add_tweights( lsaAuxPairs[i][0], lsaAuxScores[i]/2, 0 );
      g->add_tweights( lsaAuxPairs[i][1], lsaAuxScores[i]/2, 0 );
    }
    //lsa0Score is the current 'best' - lets hope so at least
    //    g->add_tweights( i, unaries[i][1]-minUn, unaries[i][0]-minUn );

  	int flow = g -> maxflow();
    int goodcase = 0;

    // ok - good
//    if ( flow < lsa0Score )
//      printf("Flow = %d < lsa0Score %d\n", flow,lsa0Score);

    // should not happen:
    if ( flow > lsa0Score )
      printf("Warning Energy increased ?? Flow = %d > lsa0Score %d\n", flow,lsa0Score);

    if ( flow < lsa0Score )
      lsa0Score = flow;
  //	printf("Flow = %d\n", flow);
    
    return flow;
  }

private:

  void AddNonSubPairwiseTerm_lsa_aux( int p, int q, P4& ff )
  {
    Scalar min_e00_01 = std::min(ff[0], ff[1]);
    ff[0] -= min_e00_01;
    ff[1] -= min_e00_01;
    unaries[p][0] += min_e00_01;

    Scalar min_e00_10 = std::min(ff[0], ff[2]);
    ff[0] -= min_e00_10;
    ff[2] -= min_e00_10;
    unaries[q][0] += min_e00_10;
    ///
    if (ff[0] > 0)
      ff -= P4(ff[0]);
    //
    Scalar min_e10_11 = std::min(ff[2], ff[3]);
    ff[2] -= min_e10_11;
    ff[3] -= min_e10_11;
    unaries[p][1] += min_e10_11;
    /////////

    //
    Scalar min_e01_11 = std::min(ff[1], ff[3]);
    ff[1] -= min_e01_11;
    ff[3] -= min_e01_11;
    unaries[q][1] += min_e01_11;
    // 
    // now only ff[3] > 0 !

    assert(ff[0]==0 && ff[1]==0 && ff[2] == 0 && ff[3] > 0);

    // no, do something else - no edge but linearization
    //g->add_edge( p, q, ff[1], ff[2] );
    lsaAuxPairs.push_back( P2i (p,q) );
    lsaAuxScores.push_back( ff[3] );
  }


  GraphType *g;

  std::vector<P2> unaries;
  //std::vector<P2> binaries;

  std::vector<P2i> lsaAuxPairs;
  std::vector<int> lsaAuxScores;

  int nNodes;
  int nEdges;
  int non_subs;
  int num_edges;

  int lsa0Score;

  /// in case of multiple edges - these must be merged first before put into GC
  edgeMap multiEdges;
};
#endif