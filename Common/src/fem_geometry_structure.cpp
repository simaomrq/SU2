/*!
 * \file fem_geometry_structure.cpp
 * \brief Main subroutines for creating the primal grid for the FEM solver.
 * \author E. van der Weide
 * \version 4.1.0 "Cardinal"
 *
 * SU2 Lead Developers: Dr. Francisco Palacios (Francisco.D.Palacios@boeing.com).
 *                      Dr. Thomas D. Economon (economon@stanford.edu).
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *
 * Copyright (C) 2012-2015 SU2, the open-source CFD code.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include <iomanip>
#include "../include/fem_geometry_structure.hpp"

void CPointCompare::Copy(const CPointCompare &other) {
  nDim           = other.nDim;
  nodeID         = other.nodeID;
  tolForMatching = other.tolForMatching;

  for(unsigned short l=0; l<nDim; ++l)
    coor[l] = other.coor[l];
}

bool CPointCompare::operator< (const CPointCompare &other) const {
  if(nDim != other.nDim) return nDim < other.nDim;    // This should never be active.

  su2double tol = min(tolForMatching, other.tolForMatching); // Tolerance for comparing.
  for(unsigned short l=0; l<nDim; ++l) {
    if(fabs(coor[l] - other.coor[l]) > tol)
      return coor[l] < other.coor[l];
  }

  return false;  // Both objects are identical.
}

void CPointFEM::Copy(const CPointFEM &other) {
  globalID           = other.globalID;
  periodIndexToDonor = other.periodIndexToDonor;
  coor[0]            = other.coor[0];
  coor[1]            = other.coor[1];
  coor[2]            = other.coor[2];
}

bool CPointFEM::operator< (const CPointFEM &other) const {
  if(periodIndexToDonor != other.periodIndexToDonor)
    return periodIndexToDonor < other.periodIndexToDonor;
  return globalID < other.globalID;
 }

bool CPointFEM::operator==(const CPointFEM &other) const {
 return (globalID           == other.globalID &&
         periodIndexToDonor == other.periodIndexToDonor);
}

su2double CSurfaceElementFEM::DetermineLengthScale(vector<CPointFEM> &meshPoints) {

  /* Variables needed to make a generic treatment possible. */
  unsigned short nDim;
  unsigned short nEdges;
  unsigned long  edgeVertices[4][2];

  su2double lenScale;

  /*--- A distinction must be made between element types. As this is a surface
        element the only options are a line, a triangle and a quadrilateral.
        Determine the number of edges and its connectivities.             ---*/
  switch( VTK_Type ) {

    case LINE: {
      nEdges = 1; nDim = 2;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid.back();
      break;
    }

    case TRIANGLE: {
      nEdges = 3; nDim = 3;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid[nPolyGrid];

      edgeVertices[1][0] = nodeIDsGrid[nPolyGrid];
      edgeVertices[1][1] = nodeIDsGrid.back();

      edgeVertices[2][0] = nodeIDsGrid.back();
      edgeVertices[2][1] = nodeIDsGrid.front();
      break;
    }

    case QUADRILATERAL: {
      nEdges = 4; nDim = 3;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid[nPolyGrid];

      edgeVertices[1][0] = nodeIDsGrid[nPolyGrid];
      edgeVertices[1][1] = nodeIDsGrid.back();

      edgeVertices[2][0] = nodeIDsGrid.back();
      edgeVertices[2][1] = nodeIDsGrid[nPolyGrid*(nPolyGrid+1)];

      edgeVertices[3][0] = nodeIDsGrid[nPolyGrid*(nPolyGrid+1)];
      edgeVertices[3][1] = nodeIDsGrid.front();
      break;
    }

    default: {
      cout << "CSurfaceElementFEM::DetermineLengthScale: This should not happen." << endl;
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
    }
  }
 
  /* Loop over the edges, determine their length and take the minimum
     for the length scale. */
  for(unsigned short i=0; i<nEdges; ++i) {
    unsigned long n0 = edgeVertices[i][0];
    unsigned long n1 = edgeVertices[i][1];

    su2double len = 0.0;
    for(unsigned short l=0; l<nDim; ++l) {
      su2double ds = meshPoints[n1].coor[l] - meshPoints[n0].coor[l];
      len += ds*ds;
    }
    len = sqrt(len);

    if(i == 0) lenScale = len;
    else       lenScale = min(lenScale, len);
  }

  /* Return the length scale. */
  return lenScale;
}

void CSurfaceElementFEM::Copy(const CSurfaceElementFEM &other) {
  VTK_Type           = other.VTK_Type;
  nPolyGrid          = other.nPolyGrid;
  nDOFsGrid          = other.nDOFsGrid;
  indStandardElement = other.indStandardElement;
  volElemID          = other.volElemID;
  boundElemIDGlobal  = other.boundElemIDGlobal;
  nodeIDsGrid        = other.nodeIDsGrid;
}

CMeshFEM::CMeshFEM(CGeometry *geometry, CConfig *config) {

  /*--- Determine the number of ranks and the current rank. ---*/
  int nRank = SINGLE_NODE;
  int rank  = MASTER_NODE;

#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRank);
#endif

  /*--- Copy the number of dimensions. ---*/
  nDim = geometry->GetnDim();

  /*--- Determine a mapping from the global point ID to the local index
        of the points.            ---*/
  map<unsigned long,unsigned long> globalPointIDToLocalInd;
  for(unsigned i=0; i<geometry->local_node; ++i)
    globalPointIDToLocalInd[geometry->node[i]->GetGlobalIndex()] = i;

  /*----------------------------------------------------------------------------*/
  /*--- Step 1: Communicate the elements and the boundary elements to the    ---*/
  /*---         ranks where they will stored during the computation.         ---*/
  /*----------------------------------------------------------------------------*/

  /*--- Determine the ranks to which I have to send my elements. ---*/
  vector<int> sendToRank(nRank, 0);

  for(unsigned long i=0; i<geometry->local_elem; i++) {
    sendToRank[geometry->elem[i]->GetColor()] = 1;
  }

  map<int,int> rankToIndCommBuf;
  for(int i=0; i<nRank; ++i) {
    if( sendToRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  /*--- Definition of the communication buffers, used to send the element data
        to the correct ranks.                ---*/
  int nRankSend = rankToIndCommBuf.size();
  vector<vector<short> >     shortSendBuf(nRankSend,  vector<short>(0));
  vector<vector<long>  >     longSendBuf(nRankSend,   vector<long>(0));
  vector<vector<su2double> > doubleSendBuf(nRankSend, vector<su2double>(0));

  /*--- The first element of longSendBuf will contain the number of elements, which
        are stored in the communication buffers. Initialize this value to 0. ---*/
  for(int i=0; i<nRankSend; ++i) longSendBuf[i].push_back(0);

  /*--- Determine the number of ranks, from which this rank will receive elements. ---*/
  int nRankRecv = nRankSend;

#ifdef HAVE_MPI
  vector<int> sizeRecv(nRank, 1);

  MPI_Reduce_scatter(sendToRank.data(), &nRankRecv, sizeRecv.data(),
                     MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

  /*--- Loop over the local elements to fill the communication buffers with element data. ---*/
  for(unsigned long i=0; i<geometry->local_elem; ++i) {
    int ind = geometry->elem[i]->GetColor();
    map<int,int>::const_iterator MI = rankToIndCommBuf.find(ind);
    ind = MI->second;

    ++longSendBuf[ind][0];   /* The number of elements in the buffers must be incremented. */

    shortSendBuf[ind].push_back(geometry->elem[i]->GetVTK_Type());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNPolyGrid());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNPolySol());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNDOFsGrid());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNDOFsSol());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetnFaces());
    shortSendBuf[ind].push_back( (short) geometry->elem[i]->GetJacobianConsideredConstant());

    longSendBuf[ind].push_back(geometry->elem[i]->GetGlobalElemID());
    longSendBuf[ind].push_back(geometry->elem[i]->GetGlobalOffsetDOFsSol());

    for(unsigned short j=0; j<geometry->elem[i]->GetNDOFsGrid(); ++j)
      longSendBuf[ind].push_back(geometry->elem[i]->GetNode(j));

    for(unsigned short j=0; j<geometry->elem[i]->GetnFaces(); ++j)
      longSendBuf[ind].push_back(geometry->elem[i]->GetNeighbor_Elements(j));

    for(unsigned short j=0; j<geometry->elem[i]->GetnFaces(); ++j) {
      shortSendBuf[ind].push_back(geometry->elem[i]->GetPeriodicIndex(j));
      shortSendBuf[ind].push_back( (short) geometry->elem[i]->GetJacobianConstantFace(j));
    }
  }

  /*--- Determine for each rank to which I have to send elements the data of
        the corresponding nodes.   ---*/
  for(int i=0; i<nRankSend; ++i) {

    /*--- Determine the vector with node IDs in the connectivity
          of the elements for this rank.   ---*/
    vector<long> nodeIDs;

    unsigned long indL = 3;
    unsigned long indS = 3;
    for(unsigned long j=0; j<longSendBuf[i][0]; ++j) {
      short nDOFsGrid = shortSendBuf[i][indS], nFaces = shortSendBuf[i][indS+2];
      indS += 2*nFaces+7;

      for(short k=0; k<nDOFsGrid; ++k, ++indL)
        nodeIDs.push_back(longSendBuf[i][indL]);
      indL += nFaces+2;
    }

    /*--- Sort nodeIDs in increasing order and remove the double entities. ---*/
    sort(nodeIDs.begin(), nodeIDs.end());
    vector<long>::iterator lastNodeID = unique(nodeIDs.begin(), nodeIDs.end());
    nodeIDs.erase(lastNodeID, nodeIDs.end());

    /*--- Add the number of node IDs and the node IDs itself to longSendBuf[i]. ---*/
    longSendBuf[i].push_back(nodeIDs.size());
    longSendBuf[i].insert(longSendBuf[i].end(), nodeIDs.begin(), nodeIDs.end());

    /*--- Copy the coordinates to doubleSendBuf. ---*/
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalPointIDToLocalInd.find(nodeIDs[j]);

      if(LMI == globalPointIDToLocalInd.end()) {
        cout << "Entry not found in map in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      unsigned long ind = LMI->second;
      for(unsigned short l=0; l<nDim; ++l)
        doubleSendBuf[i].push_back(geometry->node[ind]->GetCoord(l));
    }
  }

  /*--- Loop over the boundaries to send the boundary data to the appropriate rank. ---*/
  nMarker = geometry->GetnMarker();
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {

    /* Store the current indices in the longSendBuf, which are used to store the
       number of boundary elements sent to this rank. Initialize this value to 0. */
    vector<long> indLongBuf(nRankSend);
    for(int i=0; i<nRankSend; ++i) {
      indLongBuf[i] = longSendBuf[i].size();
      longSendBuf[i].push_back(0);
    }

    /* Loop over the local boundary elements in geometry for this marker. */
    for(unsigned long i=0; i<geometry->GetnElem_Bound(iMarker); ++i) {

      /* Determine the local ID of the corresponding domain element. */
      unsigned long elemID = geometry->bound[iMarker][i]->GetDomainElement()
                           - geometry->starting_node[rank];

      /* Determine to which rank this boundary element must be sent.
         That is the same as its corresponding domain element.
         Update the corresponding index in longSendBuf. */
      int ind = geometry->elem[elemID]->GetColor();
      map<int,int>::const_iterator MI = rankToIndCommBuf.find(ind);
      ind = MI->second;

      ++longSendBuf[ind][indLongBuf[ind]];

      /* Store the data for this boundary element in the communication buffers. */
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetVTK_Type());
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNPolyGrid());
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNDOFsGrid());

      longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetDomainElement());
      longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetGlobalElemID());

      for(unsigned short j=0; j<geometry->bound[iMarker][i]->GetNDOFsGrid(); ++j)
        longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNode(j));
    }
  }

  /*--- Definition of the communication buffers, used to receive
        the element data from the other correct ranks.        ---*/
  vector<vector<short> >     shortRecvBuf(nRankRecv,  vector<short>(0));
  vector<vector<long>  >     longRecvBuf(nRankRecv,   vector<long>(0));
  vector<vector<su2double> > doubleRecvBuf(nRankRecv, vector<su2double>(0));

  /*--- Communicate the data to the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/

  map<int,int>::const_iterator MI;
#ifdef HAVE_MPI

  /*--- Parallel mode. Send all the data using non-blocking sends. ---*/
  vector<MPI_Request> commReqs(3*nRankSend);
  MI = rankToIndCommBuf.begin();

  for(int i=0; i<nRankSend; ++i, ++MI) {

    int dest = MI->first;
    SU2_MPI::Isend(shortSendBuf[i].data(), shortSendBuf[i].size(), MPI_SHORT,
                   dest, dest, MPI_COMM_WORLD, &commReqs[3*i]);
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest+1, MPI_COMM_WORLD, &commReqs[3*i+1]);
    SU2_MPI::Isend(doubleSendBuf[i].data(), doubleSendBuf[i].size(), MPI_DOUBLE,
                   dest, dest+2, MPI_COMM_WORLD, &commReqs[3*i+2]);
  }

  /* Loop over the number of ranks from which I receive data. */
  for(int i=0; i<nRankRecv; ++i) {

    /* Block until a message with shorts arrives from any processor.
       Determine the source and the size of the message.   */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, &status);
    int source = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_SHORT, &sizeMess);

    /* Allocate the memory for the short receive buffer and receive the message. */
    shortRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(shortRecvBuf[i].data(), sizeMess, MPI_SHORT,
                  source, rank, MPI_COMM_WORLD, &status);

    /* Block until the corresponding message with longs arrives, determine
       its size, allocate the memory and receive the message. */
    MPI_Probe(source, rank+1, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_LONG, &sizeMess);
    longRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  source, rank+1, MPI_COMM_WORLD, &status);

    /* Idem for the message with doubles. */
    MPI_Probe(source, rank+2, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_DOUBLE, &sizeMess);
    doubleRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(doubleRecvBuf[i].data(), sizeMess, MPI_DOUBLE,
                  source, rank+2, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(3*nRankSend, commReqs.data(), MPI_STATUSES_IGNORE);

  /* Wild cards have been used in the communication,
     so synchronize the ranks to avoid problems.    */
  MPI_Barrier(MPI_COMM_WORLD);

#else

  /*--- Sequential mode. Simply copy the buffers. ---*/
  shortRecvBuf[0]  = shortSendBuf[0];
  longRecvBuf[0]   = longSendBuf[0];
  doubleRecvBuf[0] = doubleSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all
        the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankSend; ++i) {
    vector<short>().swap(shortSendBuf[i]);
    vector<long>().swap(longSendBuf[i]);
    vector<su2double>().swap(doubleSendBuf[i]);
  }

  /*--- Allocate the memory for the number of elements for every boundary
        marker and initialize them to zero.     ---*/
  nElem_Bound = new unsigned long[nMarker];
  for(unsigned short i=0; i<nMarker; ++i)
    nElem_Bound[i] = 0;

  /*--- Determine the global element ID's of the elements stored on this rank.
        Sort them in increasing order, such that an easy search can be done.
        In the same loop determine the upper bound for the local nodes (without
        halos) and the number of boundary elements for every marker. ---*/
  local_elem = local_node = 0;
  for(int i=0; i<nRankRecv; ++i) local_elem += longRecvBuf[i][0];

  vector<unsigned long> globalElemID;
  globalElemID.reserve(local_elem);

  for(int i=0; i<nRankRecv; ++i) {
    unsigned long indL = 1, indS = 0;
    for(unsigned long j=0; j<longRecvBuf[i][0]; ++j) {
      globalElemID.push_back(longRecvBuf[i][indL]);

      unsigned short nDOFsGrid = shortRecvBuf[i][indS+3];
      unsigned short nFaces    = shortRecvBuf[i][indS+5];
      indS += 2*nFaces + 7;
      indL += nDOFsGrid + nFaces + 2;
    }

    long nNodesThisRank = longRecvBuf[i][indL];
    local_node += nNodesThisRank;
    indL       += nNodesThisRank+1;

    for(unsigned iMarker=0; iMarker<nMarker; ++iMarker) {
      long nBoundElemThisRank = longRecvBuf[i][indL]; ++indL;
      nElem_Bound[iMarker] += nBoundElemThisRank;

      for(long j=0; j<nBoundElemThisRank; ++j) {
        short nDOFsBoundElem = shortRecvBuf[i][indS+2];
        indS += 3;
        indL += nDOFsBoundElem + 2;
      }
    }
  }

  sort(globalElemID.begin(), globalElemID.end());

  /*--- Determine the global element ID's of the halo elements. A vector of
        unsignedLong2T is used for this purpose, such that a possible periodic
        transformation can be taken into account. Neighbors with a periodic
        transformation will always become a halo element, even if the element
        is stored on this rank.             ---*/
  vector<unsignedLong2T> haloElements;

  for(int i=0; i<nRankRecv; ++i) {
    unsigned long indL = 1, indS = 0;
    for(unsigned long j=0; j<longRecvBuf[i][0]; ++j) {
      unsigned short nDOFsGrid = shortRecvBuf[i][indS+3];
      unsigned short nFaces    = shortRecvBuf[i][indS+5];

      indS += 7;
      indL += nDOFsGrid + 2;
      for(unsigned short k=0; k<nFaces; ++k, indS+=2, ++indL) {
        if(longRecvBuf[i][indL] != -1) {
          bool neighborIsInternal = false;
          if(shortRecvBuf[i][indS] == -1) {
            neighborIsInternal = binary_search(globalElemID.begin(),
                                               globalElemID.end(),
                                               longRecvBuf[i][indL]);
          }

          if( !neighborIsInternal )
            haloElements.push_back(unsignedLong2T(longRecvBuf[i][indL],
                                                  shortRecvBuf[i][indS]+1));
        }
      }
    }
  }

  sort(haloElements.begin(), haloElements.end());
  vector<unsignedLong2T>::iterator lastHalo = unique(haloElements.begin(), haloElements.end());
  haloElements.erase(lastHalo, haloElements.end());

  /*----------------------------------------------------------------------------*/
  /*--- Step 2: Store the elements, nodes and boundary elements in the data  ---*/
  /*---         structures used by the FEM solver.                           ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine the mapping from the global element number to the local entry.
     At the moment the sequence is based on the global element ID, but one can
     change this in order to have a better load balance for the threads. */
  map<unsigned long,  unsigned long> mapGlobalElemIDToInd;
  map<unsignedLong2T, unsigned long> mapGlobalHaloElemToInd;

  unsigned long nVolElemOwned = globalElemID.size();
  nVolElemTot = nVolElemOwned + haloElements.size();

  for(unsigned long i=0; i<nVolElemOwned; ++i)
    mapGlobalElemIDToInd[globalElemID[i]] = i;

  for(unsigned long i=0; i<haloElements.size(); ++i)
    mapGlobalHaloElemToInd[haloElements[i]] = nVolElemOwned + i;

  /*--- Allocate the memory for the volume elements, the nodes
        and the surface elements of the boundaries.    ---*/
  volElem.resize(nVolElemTot);
  meshPoints.reserve(local_node);

  boundaries.resize(nMarker);
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    boundaries[iMarker].markerTag = config->GetMarker_All_TagBound(iMarker);
    boundaries[iMarker].surfElem.reserve(nElem_Bound[iMarker]);
  }

  /*--- Copy the data from the communication buffers. ---*/
  for(int i=0; i<nRankRecv; ++i) {

    /* The data for the volume elements. Loop over these elements in the buffer. */
    unsigned long indL = 1, indS = 0, indD = 0;
    for(unsigned long j=0; j<longRecvBuf[i][0]; ++j) {

      /* Determine the location in volElem where this data must be stored. */
      unsigned long elemID = longRecvBuf[i][indL++];
      map<unsigned long, unsigned long>::iterator MMI = mapGlobalElemIDToInd.find(elemID);
      unsigned long ind = MMI->second;

      /* Store the data. */
      volElem[ind].elemIsOwned        = true;
      volElem[ind].rankOriginal       = rank;
      volElem[ind].periodIndexToDonor = -1;

      volElem[ind].VTK_Type  = shortRecvBuf[i][indS++];
      volElem[ind].nPolyGrid = shortRecvBuf[i][indS++];
      volElem[ind].nPolySol  = shortRecvBuf[i][indS++];
      volElem[ind].nDOFsGrid = shortRecvBuf[i][indS++];
      volElem[ind].nDOFsSol  = shortRecvBuf[i][indS++];
      volElem[ind].nFaces    = shortRecvBuf[i][indS++];

      volElem[ind].JacIsConsideredConstant = (bool) shortRecvBuf[i][indS++];

      volElem[ind].elemIDGlobal        = elemID;
      volElem[ind].offsetDOFsSolGlobal = longRecvBuf[i][indL++];

      volElem[ind].nodeIDsGrid.resize(volElem[ind].nDOFsGrid);
      volElem[ind].JacFacesIsConsideredConstant.resize(volElem[ind].nFaces);

      for(unsigned short k=0; k<volElem[ind].nDOFsGrid; ++k)
        volElem[ind].nodeIDsGrid[k] = longRecvBuf[i][indL++];

      /* Update the counter indL with nFaces, because at this location in
         the long buffer the global ID of the neighboring element is stored.
         This data is not stored in volElem. */
      indL += volElem[ind].nFaces;

      for(unsigned short k=0; k<volElem[ind].nFaces; ++k) {
        ++indS; // At this location the periodic index of the face is stored in
                // shortRecvBuf, which is not stored in volElem.
        volElem[ind].JacFacesIsConsideredConstant[k] = (bool) shortRecvBuf[i][indS++];
      }
    }

    /* The data for the nodes. Loop over these nodes in the buffer and store
       them in meshPoints.             */
    unsigned long nNodesThisRank = longRecvBuf[i][indL++];
    for(unsigned long j=0; j<nNodesThisRank; ++j) {
      CPointFEM thisPoint;
      thisPoint.globalID = longRecvBuf[i][indL++];
      thisPoint.periodIndexToDonor = -1;
      for(unsigned short k=0; k<nDim; ++k)
        thisPoint.coor[k] = doubleRecvBuf[i][indD++];

      meshPoints.push_back(thisPoint);
    }

    /* The data for the boundary markers. Loop over them. */
    for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {

      unsigned long nElemThisRank = longRecvBuf[i][indL++];
      for(unsigned long j=0; j<nElemThisRank; ++j) {
        CSurfaceElementFEM thisSurfElem;

        thisSurfElem.VTK_Type  = shortRecvBuf[i][indS++];
        thisSurfElem.nPolyGrid = shortRecvBuf[i][indS++];
        thisSurfElem.nDOFsGrid = shortRecvBuf[i][indS++];

        thisSurfElem.volElemID         = longRecvBuf[i][indL++];
        thisSurfElem.boundElemIDGlobal = longRecvBuf[i][indL++];

        thisSurfElem.nodeIDsGrid.resize(thisSurfElem.nDOFsGrid);
        for(unsigned short k=0; k<thisSurfElem.nDOFsGrid; ++k)
          thisSurfElem.nodeIDsGrid[k] = longRecvBuf[i][indL++];

        boundaries[iMarker].surfElem.push_back(thisSurfElem);
      }
    }
  }

  /* Sort meshPoints in increasing order and remove the double entities. */
  sort(meshPoints.begin(), meshPoints.end());
  vector<CPointFEM>::iterator lastPoint = unique(meshPoints.begin(), meshPoints.end());
  meshPoints.erase(lastPoint, meshPoints.end());

  /*--- All the data from the receive buffers has been copied in the local
        data structures. Release the memory of the receive buffers. To make
        sure that all the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankRecv; ++i) {
    vector<short>().swap(shortRecvBuf[i]);
    vector<long>().swap(longRecvBuf[i]);
    vector<su2double>().swap(doubleRecvBuf[i]);
  }

  /*--- Sort the surface elements of the boundaries in increasing order. ---*/
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker)
    sort(boundaries[iMarker].surfElem.begin(), boundaries[iMarker].surfElem.end());

  /*----------------------------------------------------------------------------*/
  /*--- Step 3: Communicate the information for the halo elements.           ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine the number of elements per rank of the originally partitioned grid
     stored in cumulative storage format. */
  vector<unsigned long> nElemPerRankOr(nRank+1);

  for(int i=0; i<nRank; ++i) nElemPerRankOr[i] = geometry->starting_node[i];
  nElemPerRankOr[nRank] = geometry->ending_node[nRank-1];

  /* Determine to which ranks I have to send messages to find out the information
     of the halos stored on this rank. */
  sendToRank.assign(nRank, 0);

  for(unsigned long i=0; i<haloElements.size(); ++i) {

    /* Determine the rank where this halo element was originally stored. */
    vector<unsigned long>::iterator low;
    low = lower_bound(nElemPerRankOr.begin(), nElemPerRankOr.end(),
                      haloElements[i].long0);
    unsigned long rankHalo = low - nElemPerRankOr.begin() -1;
    if(*low == haloElements[i].long0) ++rankHalo;

    sendToRank[rankHalo] = 1;
  }

  rankToIndCommBuf.clear();
  for(int i=0; i<nRank; ++i) {
    if( sendToRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  /* Resize the first index of the long send buffers for the communication of
     the halo data.        */
  nRankSend = rankToIndCommBuf.size();
  longSendBuf.resize(nRankSend);

  /*--- Determine the number of ranks, from which this rank will receive elements. ---*/
  nRankRecv = nRankSend;

#ifdef HAVE_MPI
  MPI_Reduce_scatter(sendToRank.data(), &nRankRecv, sizeRecv.data(),
                     MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

  /*--- Loop over the local halo elements to fill the communication buffers. ---*/
  for(unsigned long i=0; i<haloElements.size(); ++i) {

    /* Determine the rank where this halo element was originally stored. */
    vector<unsigned long>::iterator low;
    low = lower_bound(nElemPerRankOr.begin(), nElemPerRankOr.end(),
                      haloElements[i].long0);
    unsigned long ind = low - nElemPerRankOr.begin() -1;
    if(*low == haloElements[i].long0) ++ind;

    /* Convert this rank to the index in the send buffer. */
    MI = rankToIndCommBuf.find(ind);
    ind = MI->second;

    /* Store the global element ID and the periodic index in the long buffer.
       The subtraction of 1 is there to obtain the correct periodic index.
       In haloElements a +1 is added, because this variable is of unsigned long,
       which cannot handle negative numbers. */
    long perIndex = haloElements[i].long1 -1;

    longSendBuf[ind].push_back(haloElements[i].long0);
    longSendBuf[ind].push_back(perIndex);

    /* Determine the index in volElem where this halo must be stored. This info
       is also communicated, such that the return information can be stored in
       the correct location in volElem. */
    map<unsignedLong2T, unsigned long>::const_iterator MMI;
    MMI = mapGlobalHaloElemToInd.find(haloElements[i]);

    longSendBuf[ind].push_back(MMI->second);
  }

  /*--- Resize the first index of the long receive buffer. ---*/
  longRecvBuf.resize(nRankRecv);

  /*--- Communicate the data to the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/

#ifdef HAVE_MPI

  /* Parallel mode. Send all the data using non-blocking sends. */
  commReqs.resize(nRankSend);
  MI = rankToIndCommBuf.begin();

  for(int i=0; i<nRankSend; ++i, ++MI) {
    int dest = MI->first;
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest, MPI_COMM_WORLD, &commReqs[i]);
  }

  /* Define the vector to store the ranks from which the message came. */
  vector<int> sourceRank(nRankRecv);

  /* Loop over the number of ranks from which I receive data. */
  for(int i=0; i<nRankRecv; ++i) {

    /* Block until a message with longs arrives from any processor.
       Determine the source and the size of the message and receive it. */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, &status);
    sourceRank[i] = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_LONG, &sizeMess);

    longRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  sourceRank[i], rank, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(nRankSend, commReqs.data(), MPI_STATUSES_IGNORE);

#else

  /*--- Sequential mode. Simply copy the buffer. ---*/
  longRecvBuf[0] = longSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all the memory
        is deleted, the swap function is used. Afterwards resize the first index
        of the send buffers to nRankRecv, because this number of messages must
        be sent back to the sending ranks with halo information. ---*/
  for(int i=0; i<nRankSend; ++i) {
    vector<long>().swap(longSendBuf[i]);
  }

  shortSendBuf.resize(nRankRecv);
  longSendBuf.resize(nRankRecv);
  doubleSendBuf.resize(nRankRecv);

#ifdef HAVE_MPI
  /* Resize the vector of the communication requests to the number of messages
     to be sent ny this rank. Only in parallel node. */
  commReqs.resize(3*nRankRecv);
#endif

  /*--- Loop over the receive buffers to fill and send the send buffers again. ---*/
  for(int i=0; i<nRankRecv; ++i) {

    /* Vector with node IDs that must be returned to this calling rank.
       Note that also the periodic index must be stored, hence use an
       unsignedLong2T for this purpose. As -1 cannot be stored for an
       unsigned long a 1 is added to the periodic transformation.     */
    vector<unsignedLong2T> nodeIDs;

    /* Determine the number of elements present in longRecvBuf[i] and loop over
       them. Note that in position 0 of longSendBuf the number of elements
       present in communication buffers is stored. */
    long nElemBuf = longRecvBuf[i].size()/3;
    longSendBuf[i].push_back(nElemBuf);
    unsigned long indL = 0;
    for(long j=0; j<nElemBuf; ++j) {

      /* Determine the local index of the element in the original partitioning.
         Check if the index is valid. */
      long locElemInd = longRecvBuf[i][indL] - geometry->starting_node[rank];
      if(locElemInd < 0 || locElemInd >= geometry->npoint_procs[rank]) {
        cout << locElemInd << " " << geometry->npoint_procs[rank] << endl;
        cout << "Invalid local element ID in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      /* Store the periodic index in the short send buffer and the global element
         ID and local element ID (on the calling processor) in the long buffer. */
      longSendBuf[i].push_back(longRecvBuf[i][indL++]);
      short perIndex = (short) longRecvBuf[i][indL++];
      shortSendBuf[i].push_back(perIndex);
      longSendBuf[i].push_back(longRecvBuf[i][indL++]);

      /* Store the relevant information of this element in the short and long
         communication buffers. */
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetVTK_Type());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNPolyGrid());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNPolySol());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNDOFsGrid());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNDOFsSol());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetnFaces());

      longSendBuf[i].push_back(geometry->elem[locElemInd]->GetColor());

      for(unsigned short j=0; j<geometry->elem[locElemInd]->GetNDOFsGrid(); ++j) {
        long thisNodeID = geometry->elem[locElemInd]->GetNode(j);
        longSendBuf[i].push_back(thisNodeID);
        nodeIDs.push_back(unsignedLong2T(thisNodeID,perIndex+1)); // Note the +1.
      }
    }

    /*--- Sort nodeIDs in increasing order and remove the double entities. ---*/
    sort(nodeIDs.begin(), nodeIDs.end());
    vector<unsignedLong2T>::iterator lastNodeID = unique(nodeIDs.begin(), nodeIDs.end());
    nodeIDs.erase(lastNodeID, nodeIDs.end());

    /*--- Add the number of node IDs and the node IDs itself to longSendBuf[i]
          and the periodix index to shortSendBuf. Note again the -1 for the
          periodic index, because an unsigned long cannot represent -1, the
          value for the periodic index when no peridicity is present.       ---*/
    longSendBuf[i].push_back(nodeIDs.size());
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      longSendBuf[i].push_back(nodeIDs[j].long0);
      shortSendBuf[i].push_back( (short) nodeIDs[j].long1-1);
    }

    /*--- Copy the coordinates to doubleSendBuf. ---*/
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalPointIDToLocalInd.find(nodeIDs[j].long0);

      if(LMI == globalPointIDToLocalInd.end()) {
        cout << "Entry not found in map in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      unsigned long ind = LMI->second;
      for(unsigned short l=0; l<nDim; ++l)
        doubleSendBuf[i].push_back(geometry->node[ind]->GetCoord(l));
    }

    /* Release the memory of this receive buffer. */
    vector<long>().swap(longRecvBuf[i]);

    /*--- Send the communication buffers back to the calling rank.
          Only in parallel mode of course.     ---*/
#ifdef HAVE_MPI
    int dest = sourceRank[i];
    SU2_MPI::Isend(shortSendBuf[i].data(), shortSendBuf[i].size(), MPI_SHORT,
                   dest, dest+1, MPI_COMM_WORLD, &commReqs[3*i]);
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest+2, MPI_COMM_WORLD, &commReqs[3*i+1]);
    SU2_MPI::Isend(doubleSendBuf[i].data(), doubleSendBuf[i].size(), MPI_DOUBLE,
                   dest, dest+3, MPI_COMM_WORLD, &commReqs[3*i+2]);
#endif
  }

  /*--- Resize the first index of the receive buffers to nRankSend, such that
        the requested halo information can be received.     ---*/
  shortRecvBuf.resize(nRankSend);
  longRecvBuf.resize(nRankSend);
  doubleRecvBuf.resize(nRankSend);

  /*--- Receive the communication data from the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/
#ifdef HAVE_MPI

  /* Parallel mode. Loop over the number of ranks from which I receive data
     in the return communication, i.e. nRankSend. */
  for(int i=0; i<nRankSend; ++i) {

    /* Block until a message with shorts arrives from any processor.
       Determine the source and the size of the message.   */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank+1, MPI_COMM_WORLD, &status);
    int source = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_SHORT, &sizeMess);

    /* Allocate the memory for the short receive buffer and receive the message. */
    shortRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(shortRecvBuf[i].data(), sizeMess, MPI_SHORT,
                  source, rank+1, MPI_COMM_WORLD, &status);

    /* Block until the corresponding message with longs arrives, determine
       its size, allocate the memory and receive the message. */
    MPI_Probe(source, rank+2, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_LONG, &sizeMess);
    longRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  source, rank+2, MPI_COMM_WORLD, &status);

    /* Idem for the message with doubles. */
    MPI_Probe(source, rank+3, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_DOUBLE, &sizeMess);
    doubleRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(doubleRecvBuf[i].data(), sizeMess, MPI_DOUBLE,
                  source, rank+3, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(3*nRankRecv, commReqs.data(), MPI_STATUSES_IGNORE);

  /* Wild cards have been used in the communication,
     so synchronize the ranks to avoid problems.    */
  MPI_Barrier(MPI_COMM_WORLD);

#else

  /*--- Sequential mode. Simply copy the buffers. ---*/
  shortRecvBuf[0]  = shortSendBuf[0];
  longRecvBuf[0]   = longSendBuf[0];
  doubleRecvBuf[0] = doubleSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all
        the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankRecv; ++i) {
    vector<short>().swap(shortSendBuf[i]);
    vector<long>().swap(longSendBuf[i]);
    vector<su2double>().swap(doubleSendBuf[i]);
  }

  /*----------------------------------------------------------------------------*/
  /*--- Step 4: Build the layer of halo elements from the information in the ---*/
  /*---         receive buffers shortRecvBuf, longRecvBuf and doubleRecvBuf. ---*/
  /*----------------------------------------------------------------------------*/

  /*--- Loop over the receive buffers to store the information of the
        halo elements and the halo points. ---*/
  vector<CPointFEM> haloPoints;
  for(int i=0; i<nRankSend; ++i) {

    /* Initialization of the indices in the communication buffers. */
    unsigned long indL = 1, indS = 0, indD = 0;

    /* Loop over the halo elements received from this rank. */
    for(long j=0; j<longRecvBuf[i][0]; ++j) {

      /* Retrieve the data from the communication buffers. */
      long globElemID = longRecvBuf[i][indL++];
      long indV       = longRecvBuf[i][indL++];

      volElem[indV].elemIDGlobal = globElemID;
      volElem[indV].rankOriginal = longRecvBuf[i][indL++];

      volElem[indV].periodIndexToDonor = shortRecvBuf[i][indS++];
      volElem[indV].VTK_Type           = shortRecvBuf[i][indS++];
      volElem[indV].nPolyGrid          = shortRecvBuf[i][indS++];
      volElem[indV].nPolySol           = shortRecvBuf[i][indS++];
      volElem[indV].nDOFsGrid          = shortRecvBuf[i][indS++];
      volElem[indV].nDOFsSol           = shortRecvBuf[i][indS++];
      volElem[indV].nFaces             = shortRecvBuf[i][indS++];

      volElem[indV].nodeIDsGrid.resize(volElem[indV].nDOFsGrid);
      for(unsigned short k=0; k<volElem[indV].nDOFsGrid; ++k)
        volElem[indV].nodeIDsGrid[k] = longRecvBuf[i][indL++];

      /* Give the member variables that are not obtained via communication their
         values. Some of these variables are not used for halo elements. */
      volElem[indV].elemIsOwned             = false;
      volElem[indV].JacIsConsideredConstant = false;
      volElem[indV].offsetDOFsSolGlobal     = ULONG_MAX;
    }

    /* Store the information of the points in haloPoints. */
    long nPointsThisRank = longRecvBuf[i][indL++];
    for(long j=0; j<nPointsThisRank; ++j) {
      CPointFEM thisPoint;
      thisPoint.globalID           = longRecvBuf[i][indL++];
      thisPoint.periodIndexToDonor = shortRecvBuf[i][indS++];
      for(unsigned short l=0; l<nDim; ++l)
        thisPoint.coor[l] = doubleRecvBuf[i][indD++];

      haloPoints.push_back(thisPoint);
    }

    /* The communication buffers from this rank are not needed anymore.
       Delete them using the swap function. */
    vector<short>().swap(shortRecvBuf[i]);
    vector<long>().swap(longRecvBuf[i]);
    vector<su2double>().swap(doubleRecvBuf[i]);
  }

  /* Remove the duplicate entries from haloPoints. */
  sort(haloPoints.begin(), haloPoints.end());
  lastPoint = unique(haloPoints.begin(), haloPoints.end());
  haloPoints.erase(lastPoint, haloPoints.end());

  /* Initialization of some variables to sort the halo points. */
  Global_nPoint = geometry->GetGlobal_nPoint();
  unsigned long InvalidPointID = Global_nPoint + 10;
  short         InvalidPerInd  = SHRT_MAX;

  /*--- Search for the nonperiodic halo points in the local points to see
        of these points are already stored on this rank. If this is the
        case invalidate this halo and decrease the number of halo points.
        Afterwards remove the invalid halos from the vector.       ---*/
  unsigned long nHaloPoints = haloPoints.size();
  for(unsigned long i=0; i<haloPoints.size(); ++i) {
    if(haloPoints[i].periodIndexToDonor != -1) break;  // Test for nonperiodic.

    if( binary_search(meshPoints.begin(), meshPoints.end(), haloPoints[i]) ) {
      haloPoints[i].globalID           = InvalidPointID;
      haloPoints[i].periodIndexToDonor = InvalidPerInd;
      --nHaloPoints;
    }
  }

  sort(haloPoints.begin(), haloPoints.end());
  haloPoints.resize(nHaloPoints);

  /* Increase the capacity of meshPoints, such that the halo points can be
     stored in there as well. Note that in case periodic points are present
     this is an upper bound. Add the non-periodic halo points to meshPoints. */
  meshPoints.reserve(meshPoints.size() + nHaloPoints);

  for(unsigned long i=0; i<haloPoints.size(); ++i) {
    if(haloPoints[i].periodIndexToDonor != -1) break;  // Test for nonperiodic.
    
    meshPoints.push_back(haloPoints[i]);
  }

  /* Create a map from the global point ID and periodic index to the local
     index in the vector meshPoints. First store the points already present
     in meshPoints. */
  map<unsignedLong2T, unsigned long> mapGlobalPointIDToInd;
  for(unsigned long i=0; i<meshPoints.size(); ++i) {
    unsignedLong2T globIndAndPer;
    globIndAndPer.long0 = meshPoints[i].globalID;
    globIndAndPer.long1 = meshPoints[i].periodIndexToDonor+1;  // Note the +1 again.

    mapGlobalPointIDToInd[globIndAndPer] = i;
  }

  /*--- Convert the global indices in the boundary connectivities to local ones. ---*/
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    for(unsigned long i=0; i<boundaries[iMarker].surfElem.size(); ++i) {

      /* Convert the corresponding volume element from global to local. */
      map<unsigned long, unsigned long>::const_iterator LMI;
      LMI = mapGlobalElemIDToInd.find(boundaries[iMarker].surfElem[i].volElemID);
      boundaries[iMarker].surfElem[i].volElemID = LMI->second;

      /* Convert the global node ID's to local values. Note that for these node
         ID's no periodic transformation can be present. */
      for(unsigned short j=0; j<boundaries[iMarker].surfElem[i].nDOFsGrid; ++j) {
        unsignedLong2T searchItem(boundaries[iMarker].surfElem[i].nodeIDsGrid[j], 0);
        map<unsignedLong2T, unsigned long>::const_iterator LLMI;
        LLMI = mapGlobalPointIDToInd.find(searchItem);
        boundaries[iMarker].surfElem[i].nodeIDsGrid[j] = LLMI->second;
      }
    }
  }

  /*--- The only halo points that must be added the meshPoints are the periodic
        halo points. It must be checked whether or not the periodic points in
        haloPoints match with points in meshPoints. This is done below. ---*/
  for(unsigned long iLow=0; iLow<haloPoints.size(); ) {

    /* Determine the upper index for this periodic transformation. */
    unsigned long iUpp;
    for(iUpp=iLow+1; iUpp<haloPoints.size(); ++iUpp)
      if(haloPoints[iUpp].periodIndexToDonor != haloPoints[iLow].periodIndexToDonor) break;

    /* Check for a true periodic index. */
    short perIndex = haloPoints[iLow].periodIndexToDonor;
    if(perIndex != -1) {

      /* Easier storage of the surface elements. */
      vector<CSurfaceElementFEM> &surfElem = boundaries[perIndex].surfElem;

      /*--- Store the points of this local periodic boundary in a data structure
            that can be used for searching coordinates. ---*/
      vector<CPointCompare> pointsBoundary;
      vector<long> indInPointsBoundary(meshPoints.size(), -1);
      for(unsigned long j=0; j<surfElem.size(); ++j) {

        /* Determine the tolerance for equal points, which is a small value
           times the length scale of this surface element. */
        su2double tolElem = 1.e-4*surfElem[j].DetermineLengthScale(meshPoints);

        /* Loop over the nodes of this surface grid and update the points on
           this periodic boundary. */
        for(unsigned short k=0; k<surfElem[j].nDOFsGrid; ++k) {
          unsigned long nn = surfElem[j].nodeIDsGrid[k];

          if(indInPointsBoundary[nn] == -1) {
            /* Point is not stored yet in pointsBoundary. Do so now. */
            indInPointsBoundary[nn] = pointsBoundary.size();

            CPointCompare thisPoint;
            thisPoint.nDim           = nDim;
            thisPoint.nodeID         = nn;
            thisPoint.tolForMatching = tolElem;
            for(unsigned short l=0; l<nDim; ++l)
              thisPoint.coor[l] = meshPoints[nn].coor[l];

            pointsBoundary.push_back(thisPoint);
          }
          else {
            /* Point is already stored in pointsBoundary. Update the tolerance. */
            nn = indInPointsBoundary[nn];
            pointsBoundary[nn].tolForMatching = min(pointsBoundary[nn].tolForMatching, tolElem);
          }
        }
      }

      /* Sort pointsBoundary in increasing order, such that binary searches can
         be carried out later on. */
      sort(pointsBoundary.begin(), pointsBoundary.end());

      /* Get the data for the periodic transformation to the donor. */
      su2double *center = config->GetPeriodicRotCenter(config->GetMarker_All_TagBound(perIndex));
      su2double *angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(perIndex));
      su2double *trans  = config->GetPeriodicTranslation(config->GetMarker_All_TagBound(perIndex));

      /*--- Compute the rotation matrix and translation vector for the
            transformation from the donor. This is the transpose of the
            transformation to the donor.               ---*/

      /* Store (center-trans) as it is constant and will be added on. */
      su2double translation[] = {center[0] - trans[0],
                                 center[1] - trans[1],
                                 center[2] - trans[2]};

      /* Store angles separately for clarity. Compute sines/cosines. */
      su2double theta = angles[0];
      su2double phi   = angles[1];
      su2double psi   = angles[2];

      su2double cosTheta = cos(theta), cosPhi = cos(phi), cosPsi = cos(psi);
      su2double sinTheta = sin(theta), sinPhi = sin(phi), sinPsi = sin(psi);

      /* Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis, then z-axis. */
      su2double rotMatrix[3][3];
      rotMatrix[0][0] =  cosPhi*cosPsi;
      rotMatrix[0][1] =  cosPhi*sinPsi;
      rotMatrix[0][2] = -sinPhi;

      rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;
      rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;
      rotMatrix[1][2] = sinTheta*cosPhi;

      rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
      rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
      rotMatrix[2][2] = cosTheta*cosPhi;

      /* Loop over the halo points for this periodic transformation. */
      for(unsigned long i=iLow; i<iUpp; ++i) {

        /* Apply the periodic transformation to the coordinates
           stored in this halo point. */
        su2double dx =             haloPoints[i].coor[0] - center[0];
        su2double dy =             haloPoints[i].coor[1] - center[1];
        su2double dz = nDim == 3 ? haloPoints[i].coor[2] - center[2] : 0.0;

        haloPoints[i].coor[0] = rotMatrix[0][0]*dx + rotMatrix[0][1]*dy
                              + rotMatrix[0][2]*dz + translation[0];
        haloPoints[i].coor[1] = rotMatrix[1][0]*dx + rotMatrix[1][1]*dy
                              + rotMatrix[1][2]*dz + translation[1];
        haloPoints[i].coor[2] = rotMatrix[2][0]*dx + rotMatrix[2][1]*dy
                              + rotMatrix[2][2]*dz + translation[2];

        /* Create an object of the type CPointCompare, which can be used
           to search the points on the periodic boundary, pointsBoundary. */
        CPointCompare thisPoint;
        thisPoint.nDim   = nDim;
        thisPoint.nodeID = ULONG_MAX;
        thisPoint.tolForMatching = 1.e+10;   // Just a large value.
        for(unsigned short l=0; l<nDim; ++l)
          thisPoint.coor[l] = haloPoints[i].coor[l];

        /* Check if this point is present in pointsBoundary. */
        if( binary_search(pointsBoundary.begin(), pointsBoundary.end(), thisPoint) ) {

          /* This point is present on the boundary. Find its position and
             store it in the mapping to the local points in meshPoints. */
          vector<CPointCompare>::const_iterator periodicLow;
          periodicLow = lower_bound(pointsBoundary.begin(), pointsBoundary.end(), thisPoint);

          unsignedLong2T globIndAndPer;
          globIndAndPer.long0 = haloPoints[i].globalID;
          globIndAndPer.long1 = haloPoints[i].periodIndexToDonor+1;  // Note the +1 again.

          mapGlobalPointIDToInd[globIndAndPer] = periodicLow->nodeID;
        }
        else {

          /* This point is not present yet on this rank yet. Store it in the
             mapping to the local points in mesh points and create it. */
          unsignedLong2T globIndAndPer;
          globIndAndPer.long0 = haloPoints[i].globalID;
          globIndAndPer.long1 = haloPoints[i].periodIndexToDonor+1;  // Note the +1 again.

          mapGlobalPointIDToInd[globIndAndPer] = meshPoints.size();
          meshPoints.push_back(haloPoints[i]);
        }
      }
    }

    /* Set iLow to iUpp for the next periodic transformation. */
    iLow = iUpp;
  }

  /*--- Convert the global node numbering in the elements to a local numbering. ---*/
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    for(unsigned short j=0; j<volElem[i].nDOFsGrid; ++j) {
      unsignedLong2T searchItem(volElem[i].nodeIDsGrid[j],
                                volElem[i].periodIndexToDonor+1); // Again the +1.
      map<unsignedLong2T, unsigned long>::const_iterator LLMI;
      LLMI = mapGlobalPointIDToInd.find(searchItem);
      volElem[i].nodeIDsGrid[j] = LLMI->second;
    }
  }
}

CMeshFEM_DG::CMeshFEM_DG(CGeometry *geometry, CConfig *config) : CMeshFEM(geometry, config) {

}

void CMeshFEM_DG::SetFaces(void) {

  /*---------------------------------------------------------------------------*/
  /*--- Step 1: Determine the faces of the locally stored part of the grid. ---*/
  /*---------------------------------------------------------------------------*/

  cout << "CMeshFEM_DG::SetFaces: Not implemented yet." << endl;
#ifndef HAVE_MPI
  exit(EXIT_FAILURE);
#else
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Abort(MPI_COMM_WORLD,1);
  MPI_Finalize();
#endif
}

void CMeshFEM_DG::SetSendReceive(CConfig *config) {

  /*--- Determine the number of ranks and the current rank. ---*/
  int nRank = SINGLE_NODE;
  int rank  = MASTER_NODE;

#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRank);
#endif

  /*----------------------------------------------------------------------------*/
  /*--- Step 1: Determine the ranks which this rank has to communicate       ---*/
  /*---         during the actual communication of halo data, as well as the ---*/
  /*            that must be communicated.                                   ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine for every element the local offset of the solution DOFs. */
  volElem[0].offsetDOFsSolLocal = 0;
  for(unsigned long i=1; i<nVolElemTot; ++i)
    volElem[i].offsetDOFsSolLocal = volElem[i-1].offsetDOFsSolLocal
                                  + volElem[i-1].nDOFsSol;

  /* Determine the ranks which this rank will communicate. */
  vector<bool> commWithRank(nRank, false);
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( !volElem[i].elemIsOwned ) commWithRank[volElem[i].rankOriginal] = true;
  }

  map<int,int> rankToIndCommBuf;
  for(int i=0; i<nRank; ++i) {
    if( commWithRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  ranksComm.resize(rankToIndCommBuf.size());
  map<int,int>::const_iterator MI = rankToIndCommBuf.begin();
  for(int i=0; i<rankToIndCommBuf.size(); ++i, ++MI)
    ranksComm[i] = MI->first;

  /* Define and determine the buffers to send the global indices of my halo
     elements to the appropriate ranks and the vectors which store the
     DOFs that I will receive from these ranks. */
  vector<vector<unsigned long> > longBuf(rankToIndCommBuf.size(), vector<unsigned long>(0));
  DOFsReceive.resize(rankToIndCommBuf.size());

  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( !volElem[i].elemIsOwned ) {
      MI = rankToIndCommBuf.find(volElem[i].rankOriginal);
      longBuf[MI->second].push_back(volElem[i].elemIDGlobal);

      for(unsigned long j=0; j<volElem[i].nDOFsSol; ++j)
        DOFsReceive[MI->second].push_back(volElem[i].offsetDOFsSolLocal+j);
    }
  }

  /* Determine the mapping from global element ID to local owned element ID. */
  map<unsigned long,unsigned long> globalElemIDToLocalInd;
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( volElem[i].elemIsOwned )
      globalElemIDToLocalInd[volElem[i].elemIDGlobal] = i;
  }

  /* Resize the first index of the vectors to store the DOFs that must be sent. */
  DOFsSend.resize(rankToIndCommBuf.size());

  /*--- Make a distinction between sequential and parallel mode to
        determine the DOFs to be sent.                 ---*/

#ifdef HAVE_MPI
  /*--- Parallel mode. Send all the data using non-blocking sends. ---*/
  vector<MPI_Request> commReqs(ranksComm.size());

  for(unsigned long i=0; i<ranksComm.size(); ++i) {
    int dest = ranksComm[i];
    SU2_MPI::Isend(longBuf[i].data(), longBuf[i].size(), MPI_UNSIGNED_LONG,
                   dest, dest, MPI_COMM_WORLD, &commReqs[i]);
  }

  /* Loop over the number of ranks from which I receive data about the
     global element ID's that I must sent. */
  for(unsigned long i=0; i<ranksComm.size(); ++i) {

    /* Receive the messages in the order specified in ranksComm.
       First probe the message to find out the size. */
    MPI_Status status;
    MPI_Probe(ranksComm[i], rank, MPI_COMM_WORLD, &status);

    int sizeMess;
    MPI_Get_count(&status, MPI_UNSIGNED_LONG, &sizeMess);

    /* Allocate the memory for a buffer to receive the data
       and receive the data using a blocking receive. */
    vector<unsigned long> longRecvBuf(sizeMess);
    SU2_MPI::Recv(longRecvBuf.data(), sizeMess, MPI_UNSIGNED_LONG,
                  ranksComm[i], rank, MPI_COMM_WORLD, &status);

    /* Loop over the elements of longRecvBuf and set the contents
       of DOFsSend accordingly. */
    for(int j=0; j<sizeMess; ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalElemIDToLocalInd.find(longRecvBuf[j]);

      if(LMI == globalElemIDToLocalInd.end()) {
        cout << "This should not happen in CMeshFEM_DG::SetSendReceive" << endl;
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
      }

      for(unsigned long k=0; k<volElem[LMI->second].nDOFsSol; ++k)
        DOFsSend[i].push_back(volElem[LMI->second].offsetDOFsSolLocal+k);
    }
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(ranksComm.size(), commReqs.data(), MPI_STATUSES_IGNORE);

#else
  /* Sequential mode. Search for the local index of the global element ID.
     Note that in sequential mode there are only halo elements when periodic
     boundaries are present in the grid. */
  if( longBuf.size() ) {
    for(unsigned long i=0; i<longBuf[0].size(); ++i) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalElemIDToLocalInd.find(longBuf[0][i]);

      if(LMI == globalElemIDToLocalInd.end()) {
        cout << "This should not happen in CMeshFEM_DG::SetSendReceive" << endl;
        exit(EXIT_FAILURE);
      }

      for(unsigned long j=0; j<volElem[LMI->second].nDOFsSol; ++j)
        DOFsSend[0].push_back(volElem[LMI->second].offsetDOFsSolLocal+j);
    }
  }

#endif
}
