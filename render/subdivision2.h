// Aqsis
// Copyright � 1997 - 2001, Paul C. Gregory
//
// Contact: pgregory@aqsis.com
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


/** \file
		\brief Declares the classes for subdivision surfaces.
		\author Paul C. Gregory (pgregory@aqsis.com)
*/


#ifndef	SUBDIVISION2_H_LOADED
#define	SUBDIVISION2_H_LOADED

#include "aqsis.h"
#include "lath.h"
#include "vector3d.h"
#include "refcount.h"
#include "surface.h"
#include "polygon.h"

START_NAMESPACE( Aqsis )

//------------------------------------------------------------------------------
/**
 *	Container for the topology description of a mesh.
 *	Holds information about which Laths represent which facets and vertices, and 
 *  provides functions to build topology data structures from unstructured meshes.
 */

class CqSubdivision2 : public CqRefCount, public CqMotionSpec<CqPolygonPoints*>
{
public:
	///	Constructor.
	CqSubdivision2( CqPolygonPoints* pPoints = NULL );

	///	Destructor.
	virtual ~CqSubdivision2();

	CqLath* pFacet(TqInt iIndex);
	CqLath* pVertex(TqInt iIndex);

	/// Get the number of faces representing this topology.
	TqInt	cFacets() const		{return(m_apFacets.size());}
	/// Get the number of laths representing this topology.
	TqInt	cLaths() const		{return(m_apLaths.size());}
	/// Get the number of faces representing this topology.
	TqInt	cVertices() const	{return(m_aapVertices.size());}

	/// Get a refrence to the array of autoatically generated laths.
	const std::vector<CqLath*>& apLaths() const	
								{return(m_apLaths);}

	/// Get pointer to the vertex storage class
	CqPolygonPoints* pPoints( TqInt TimeIndex = 0 ) const	
	{
		return(GetMotionObject( Time( TimeIndex ) ));
	}

	void		Prepare(TqInt cVerts);
	CqLath*		AddFacet(TqInt cVerts, TqInt* pIndices);
	TqBool		Finalise();
	void		SubdivideFace(CqLath* pFace, std::vector<CqLath*>& apSubFaces);
	TqBool		CanUsePatch( CqLath* pFace );
	void		SetInterpolateBoundary( TqBool state = TqTrue )
				{
					m_bInterpolateBoundary = state;
				}
	TqBool		isInterpolateBoundary( ) const
				{
					return( m_bInterpolateBoundary );
				}
	void		SetHoleFace( TqInt iFaceIndex )
				{
					m_mapHoles[ iFaceIndex ] = TqTrue;
				}
	TqBool		isHoleFace( TqInt iFaceIndex ) const
				{
					return( m_mapHoles.find( iFaceIndex ) != m_mapHoles.end() );
				}
	void		AddSharpEdge( CqLath* pLath, TqFloat Sharpness )
				{
					m_mapSharpEdges[pLath] = Sharpness;
				}
	TqFloat		EdgeSharpness( CqLath* pLath )
				{
					if( m_mapSharpEdges.find( pLath ) != m_mapSharpEdges.end() )
						return( m_mapSharpEdges[ pLath ] );
					return( 0.0f );
				}
	void		AddSharpCorner( CqLath* pLath, TqFloat Sharpness )
				{
					std::vector<CqLath*> aQve;
					pLath->Qve( aQve );
					std::vector<CqLath*>::iterator iVE;
					for( iVE = aQve.begin(); iVE != aQve.end(); iVE++ )
						m_mapSharpCorners[(*iVE)] = Sharpness;
				}
	TqFloat		CornerSharpness( CqLath* pLath )
				{
					if( m_mapSharpCorners.find( pLath ) != m_mapSharpCorners.end() )
						return( m_mapSharpCorners[ pLath ] );
					return( 0.0f );
				}

	TqInt		AddVertex(CqLath* pVertex);
	template<class TypeA, class TypeB>
	void		CreateVertex(CqParameterTyped<TypeA, TypeB>* pParam, CqLath* pVertex, TqInt iIndex)
				{
					TypeA S = TypeA(0.0f);
					TypeA Q = TypeA(0.0f);
					TypeA R = TypeA(0.0f);
					TqInt n;
					
					if(pParam->Class() == class_vertex)
					{
						// Determine if we have a boundary vertex.
						if( pVertex->isBoundaryVertex() )
						{
							// The vertex is on a boundary.
							/// \note If "interpolateboundary" is not specified, we will never see this as
							/// the boundary facets aren't rendered. So we don't need to check for "interpolateboundary" here.
							std::vector<CqLath*> apQve;
							pVertex->Qve(apQve);							
							// Is the valence == 2 ?
							if( apQve.size() == 2 )
							{
								// Yes, boundary with valence 2 is corner.
								pParam->pValue( iIndex )[0] = pParam->pValue( pVertex->VertexIndex() )[0];
							}
							else
							{
								// No, boundary is average of two adjacent boundary edges, and original point.
								// Get the midpoints of the adjacent boundary edges
								std::vector<CqLath*> aQve;
								pVertex->Qve( aQve );

								TqInt cBoundaryEdges = 0;
								std::vector<CqLath*>::iterator iE;
								for( iE = aQve.begin(); iE != aQve.end(); iE++ )
								{
									// Only consider the boundary edges.
									if( NULL == (*iE)->ec() )
									{
										if( (*iE)->VertexIndex() == pVertex->VertexIndex() )
											R += pParam->pValue( (*iE)->ccf()->VertexIndex() )[0];
										else
											R += pParam->pValue( (*iE)->VertexIndex() )[0];
										cBoundaryEdges++;
									}
								}
								assert( cBoundaryEdges == 2 );
								
								// Get the current vertex;
								S = pParam->pValue( pVertex->VertexIndex() )[0];
								pParam->pValue( iIndex )[0] = ( R + ( S * 6.0f ) ) / 8.0f;
							}
						}
						else
						{
							// Check if a sharp corner vertex.
							if( CornerSharpness( pVertex ) > 0.0f )
							{
								pParam->pValue( iIndex )[0] = pParam->pValue( pVertex->VertexIndex() )[0];
							}
							else
							{
								// Check if crease vertex.
								std::vector<CqLath*> aQve;
								pVertex->Qve( aQve );

								CqLath* hardEdge1 = NULL;
								CqLath* hardEdge2 = NULL;
								CqLath* hardEdge3 = NULL;
								TqInt se = 0;
								std::vector<CqLath*>::iterator iEdge;
								for( iEdge = aQve.begin(); iEdge != aQve.end(); iEdge++ )
								{
									float h = EdgeSharpness( (*iEdge) );
									if( hardEdge1 == NULL || h > EdgeSharpness(hardEdge1) )
									{
										hardEdge3 = hardEdge2;
										hardEdge2 = hardEdge1;
										hardEdge1 = *iEdge;
									}
									else if( hardEdge2 == NULL || h > EdgeSharpness(hardEdge2) )
									{
										hardEdge3 = hardEdge2;
										hardEdge2 = *iEdge;
									}
									else if( hardEdge3 == NULL || h > EdgeSharpness(hardEdge3) )
									{
										hardEdge3 = *iEdge;
									}

									if( h > 0.0f )
									{
										se++;
								//		printf("h = %f\n", h);
									}
								}

								TypeA softPos;
								TypeA semiSharpPos;
								TypeA sharpPos;
								// Smooth
								// Vertex point is...
								//    Q     2R     S(n-3)
								//   --- + ---- + --------
								//    n      n        n
								//
								// Q = Average of face points surrounding old vertex
								// R = average of midpoints of edges surrounding old vertex
								// S = old vertex
								// n = number of edges sharing the old vertex.

								n = aQve.size();

								// Get the face points of the surrounding faces
								std::vector<CqLath*> aQvf;
								pVertex->Qvf( aQvf );
								std::vector<CqLath*>::iterator iF;
								for( iF = aQvf.begin(); iF != aQvf.end(); iF++ )
								{
									std::vector<CqLath*> aQfv;
									(*iF)->Qfv(aQfv);
									std::vector<CqLath*>::iterator iV;
									TypeA Val = TypeA(0.0f);
									for( iV = aQfv.begin(); iV != aQfv.end(); iV++ )
										Val += pParam->pValue( (*iV)->VertexIndex() )[0];
									Val /= static_cast<TqFloat>( aQfv.size() );
									Q += Val;
								}
								Q /= aQvf.size();
								Q /= n;

								// Get the midpoints of the surrounding edges
								TypeA A = pParam->pValue( pVertex->VertexIndex() )[0];
								TypeA B = TypeA(0.0f);
								std::vector<CqLath*>::iterator iE;
								for( iE = aQve.begin(); iE != aQve.end(); iE++ )
								{
									B = pParam->pValue( (*iE)->ccf()->VertexIndex() )[0];
									R += (A+B)/2.0f;
								}
								R *= 2.0f;
								R /= n;
								R /= n;

								// Get the current vertex;
								S = pParam->pValue( pVertex->VertexIndex() )[0];
								S *= static_cast<TqFloat>(n-3);
								S /= n;

								//pParam->pValue( iIndex )[0] = Q+R+S;
								softPos = Q+R+S;

								if( se >= 2 )
								{
									// Crease
									// Get the midpoints of the surrounding 2 hardest edges
									R = pParam->pValue(hardEdge1->ccf()->VertexIndex() )[0];
									R = R + pParam->pValue(hardEdge2->ccf()->VertexIndex() )[0];

									// Get the current vertex;
									S = pParam->pValue( pVertex->VertexIndex() )[0];
									semiSharpPos =  ( R + ( S * 6.0f ) ) / 8.0f;
								}

								sharpPos = pParam->pValue( pVertex->VertexIndex() )[0];

								// Blend the three values together weighted by the sharpness values.
								TypeA Pos;
								float h2 = hardEdge2 != NULL ? EdgeSharpness(hardEdge2) : 0.0f;
								float h3 = hardEdge3 != NULL ? EdgeSharpness(hardEdge3) : 0.0f;
								Pos = (1.0f - h2)*softPos;
								Pos = Pos + (h2 - h3)*semiSharpPos;
								Pos = Pos + h3*sharpPos;
								pParam->pValue( iIndex )[0] = Pos;
							}
						}
					}
					else
					{
						pParam->pValue( iIndex )[0] = pParam->pValue( pVertex->VertexIndex() )[0];
					}
				}
	TqInt		AddEdgeVertex(CqLath* pEdge);
	template<class TypeA, class TypeB>
	void		CreateEdgeVertex(CqParameterTyped<TypeA, TypeB>* pParam, CqLath* pEdge, TqInt iIndex)
				{
					TypeA A = TypeA(0.0f);
					TypeA B = TypeA(0.0f);
					TypeA C = TypeA(0.0f);

					if(pParam->Class() == class_vertex)
					{
						if( NULL != pEdge->ec() )
						{
							// Edge point is the average of the centrepoint of the original edge and the
							// average of the two new face points of the adjacent faces.
							std::vector<CqLath*> aQef;
							pEdge->Qef( aQef );
							std::vector<CqLath*>::iterator iF;
							for( iF = aQef.begin(); iF != aQef.end(); iF++ )
							{
								std::vector<CqLath*> aQfv;
								(*iF)->Qfv(aQfv);
								std::vector<CqLath*>::iterator iV;
								TypeA Val = TypeA(0.0f);
								for( iV = aQfv.begin(); iV != aQfv.end(); iV++ )
									Val += pParam->pValue( (*iV)->VertexIndex() )[0];
								Val/=static_cast<TqFloat>( aQfv.size() );
								C += Val;
							}
							C /= static_cast<TqFloat>(aQef.size());

							A = pParam->pValue( pEdge->VertexIndex() )[0];
							B = pParam->pValue( pEdge->ccf()->VertexIndex() )[0];

							float h = EdgeSharpness( pEdge );
							A = ((1.0f+h)*(A+B)) / 2.0f;
							A = (A + (1.0f-h)*C) / 2.0f;
						}
						else
						{
							A = pParam->pValue( pEdge->VertexIndex() )[0];
							B = pParam->pValue( pEdge->ccf()->VertexIndex() )[0];
							A = (A+B)/2.0f;
						}
					}
					else
					{
						A = pParam->pValue( pEdge->VertexIndex() )[0];
						B = pParam->pValue( pEdge->ccf()->VertexIndex() )[0];
						A = (A+B)/2.0f;
					}
					pParam->pValue( iIndex )[0] = A;
				}
	TqInt		AddFaceVertex(CqLath* pFace);
	template<class TypeA, class TypeB>
	void		CreateFaceVertex(CqParameterTyped<TypeA, TypeB>* pParam, CqLath* pFace, TqInt iIndex)
				{
					// Face point is just the average of the original faces vertices.
					std::vector<CqLath*> aQfv;
					pFace->Qfv(aQfv);
					std::vector<CqLath*>::iterator iV;
					TypeA Val = TypeA(0.0f);
					for( iV = aQfv.begin(); iV != aQfv.end(); iV++ )
						Val += pParam->pValue( (*iV)->VertexIndex() )[0];
					Val/=static_cast<TqFloat>( aQfv.size() );
					pParam->pValue( iIndex )[0] = Val;
				}

		// Overrides from CqMotionSpec
		virtual	void	ClearMotionObject( CqPolygonPoints*& A ) const
			{}
		;
		virtual	CqPolygonPoints* ConcatMotionObjects( CqPolygonPoints* const & A, CqPolygonPoints* const & B ) const
		{
			return ( A );
		}
		virtual	CqPolygonPoints* LinearInterpolateMotionObjects( TqFloat Fraction, CqPolygonPoints* const & A, CqPolygonPoints* const & B ) const
		{
			return ( A );
		}


	void		OutputMesh(const char* fname, std::vector<CqLath*>* paFaces = 0);
	void		OutputInfo(const char* fname, std::vector<CqLath*>* paFaces = 0);
private:
	///	Declared private to prevent copying.
	CqSubdivision2(const CqSubdivision2&);
	///	Declared private to prevent copying.
	CqSubdivision2&	operator=(const CqSubdivision2&);

	/// Array of pointers to laths, one each representing each facet.
	std::vector<CqLath*>				m_apFacets;
	/// Array of arrays of pointers to laths each array representing the total laths referencing a single vertex.
	std::vector<std::vector<CqLath*> >	m_aapVertices;
	/// Array of lath pointers, one for each lath generated.
	std::vector<CqLath*>				m_apLaths;
	/// Map of face indices which are to be treated as holes in the surface, i.e. not rendered.
	std::map<TqInt, TqBool>				m_mapHoles;
	/// Flag indicating whether this surface interpolates it's boundaries or not.
	TqBool								m_bInterpolateBoundary;
	/// Map of sharp edges.
	std::map<CqLath*, TqFloat>			m_mapSharpEdges;
	/// Map of sharp corners.
	std::map<CqLath*, TqFloat>			m_mapSharpCorners;


	/// Flag indicating whether the topology structures have been finalised.
	TqBool							m_fFinalised;
};



class CqSurfaceSubdivisionPatch : public CqBasicSurface
{
	public:
		CqSurfaceSubdivisionPatch(CqSubdivision2* pTopology, CqLath* pFace)
		{
			assert(NULL != pTopology);
			// Reference the topology class
			pTopology->AddRef();
			m_pTopology = pTopology;
			m_pFace = pFace;
		}

		virtual	~CqSurfaceSubdivisionPatch()
		{
			assert(NULL != m_pTopology);
			// Unreference the subdivision topology class
			m_pTopology->Release();
		}

		/** Get the pointer to the subdivision surface hull that this patch is part of.
		 */
		CqSubdivision2*	pTopology() const
		{
			return( m_pTopology );
		}

		/** Get the index of the face on the hull that this patch refers to.
		 */
		CqLath*	pFace() const
		{
			return( m_pFace );
		}

		virtual	const IqAttributes*	pAttributes() const
		{
			return ( pTopology()->pPoints()->pAttributes() );
		}
		virtual	const IqTransform*	pTransform() const
		{
			return ( pTopology()->pPoints()->pTransform() );
		}
		// Required implementations from IqSurface
		virtual void	Transform( const CqMatrix& matTx, const CqMatrix& matITTx, const CqMatrix& matRTx, TqInt iTime = 0 )
		{
			//pTopology()->pPoints( iTime )->Transform( matTx, matITTx, matRTx );
		}
		// NOTE: These should never be called.
		virtual	TqUint	cUniform() const
		{
			return ( 0 );
		}
		virtual	TqUint	cVarying() const
		{
			return ( 0 );
		}
		virtual	TqUint	cVertex() const
		{
			return ( 0 );
		}
		virtual	TqUint	cFaceVarying() const
		{
			return ( 0 );
		}

		// Implementations required by CqBasicSurface
		virtual	CqBound	Bound() const;
		virtual	CqMicroPolyGridBase* Dice();
		virtual	TqInt	Split( std::vector<CqBasicSurface*>& aSplits );
		virtual TqBool	Diceable();

		/** Determine whether the passed surface is valid to be used as a 
		 *  frame in motion blur for this surface.
		 */
		virtual TqBool	IsMotionBlurMatch( CqBasicSurface* pSurf )
		{
			return( TqFalse );
		}

		void StoreDice( CqMicroPolyGrid* pGrid, CqPolygonPoints* pPoints, TqInt iParam, TqInt iData );

	private:
		CqSubdivision2*	m_pTopology;
		CqLath*			m_pFace;
};

//----------------------------------------------------------------------
/** \class CqSurfacePointsPolygons
 * Container surface to store the polygons making up a RiPointsPolygons surface.
 */

class CqSurfaceSubdivisionMesh : public CqSurface
{
	public:	
		CqSurfaceSubdivisionMesh(CqSubdivision2* pTopology, TqInt NumFaces) : 
			m_NumFaces(NumFaces),
			m_pTopology( pTopology )
		{
			assert( NULL != m_pTopology );
			m_pTopology->AddRef();
		}
		virtual	~CqSurfaceSubdivisionMesh()
		{
			m_pTopology->Release();
		}

		/** Get the gemoetric bound of this GPrim.
		 */
		virtual	CqBound	Bound() const;
		/** Dice this GPrim.
		 * \return A pointer to a new micropolygrid..
		 */
		virtual	CqMicroPolyGridBase* Dice()
		{
			return(NULL);
		}
		/** Split this GPrim into a number of other GPrims.
		 * \param aSplits A reference to a CqBasicSurface array to fill in with the new GPrim pointers.
		 * \return Integer count of new GPrims created.
		 */
		virtual	TqInt	Split( std::vector<CqBasicSurface*>& aSplits );
		/** Determine whether this GPrim is diceable at its current size.
		 */
		virtual TqBool	Diceable()
		{
			return( TqFalse );
		}

		virtual void	Transform( const CqMatrix& matTx, const CqMatrix& matITTx, const CqMatrix& matRTx, TqInt iTime = 0 )
		{
			assert( NULL != m_pTopology );
			m_pTopology->pPoints()->Transform( matTx, matITTx, matRTx, iTime );
		}

		virtual TqBool	IsMotionBlurMatch( CqBasicSurface* pSurf )
		{
			return( TqFalse );
		}

		virtual	TqUint	cUniform() const
		{
			return ( m_NumFaces );
		}
		virtual	TqUint	cVarying() const
		{
			assert( NULL != m_pTopology );
			assert( NULL != m_pTopology->pPoints() );
			return ( m_pTopology->pPoints()->cVarying() );
		}
		virtual	TqUint	cVertex() const
		{
			assert( NULL != m_pTopology );
			assert( NULL != m_pTopology->pPoints() );
			return ( m_pTopology->pPoints()->cVarying() );
		}
		virtual	TqUint	cFaceVarying() const
		{
			/// \todo Must work out what this value should be.
			return ( 1 );
		}

	private:
		TqInt	m_NumFaces;
		CqSubdivision2*	m_pTopology;		///< Pointer to the associated CqPolygonPoints class.
};


END_NAMESPACE( Aqsis )

#endif	//	SUBDIVISION2_H_LOADED
