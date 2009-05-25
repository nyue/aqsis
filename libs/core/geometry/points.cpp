// Aqsis
// Copyright c 1997 - 2001, Paul C. Gregory
//
// Contact: pgregory@aqsis.org
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
		\brief Implements CqPoints using small regular polygon (first try) This is more or less an experimentation with the parser. Later a micropolygon grid will be used to be more efficient to shade/render.
		\author M. Joron (joron@sympatico.ca)
*/
/*    References:
 *          [PIXA89]  Pixar, The RenderMan Interface, Version 3.2, 
 *                    Richmond, CA, September 1989.  
 *
 */

#include	<cmath>

#include	"points.h"
#include	"imagebuffer.h"
#include	"renderer.h"
#include	"bucketprocessor.h"

namespace Aqsis {

CqObjectPool<CqMovingMicroPolygonKeyPoints>	CqMovingMicroPolygonKeyPoints::m_thePool;
CqObjectPool<CqMicroPolygonPoints>	CqMicroPolygonPoints::m_thePool;
CqObjectPool<CqMicroPolygonMotionPoints>	CqMicroPolygonMotionPoints::m_thePool;


void CqPointsKDTreeData::SetpPoints( const CqPoints* pPoints )
{
	m_pPointsSurface = pPoints;
}

void CqPointsKDTreeData::FreePoints()
{}

bool CqPointsKDTreeData::CqPointsKDTreeDataComparator::operator()(TqInt a, TqInt b)
{
	return( ( m_pPointsSurface->pPoints()->P()->pValue( a )[0][m_Dim] ) < ( m_pPointsSurface->pPoints()->P()->pValue( b )[0][m_Dim] ) );
}


//---------------------------------------------------------------------
/** Constructor.
 */

CqPoints::CqPoints( TqInt nvertices, const boost::shared_ptr<CqPolygonPoints>& pPoints ) : 
		m_pPoints(pPoints),
		m_nVertices(nvertices),
		m_KDTree(&m_KDTreeData),
		m_MaxWidth(0)
{
	//assert( NULL != pPoints );
	assert( nvertices > 0 );

	m_KDTreeData.SetpPoints(this);

	m_widthParamIndex = -1;
	m_constantwidthParamIndex = -1;

	std::vector<CqParameter*>::iterator iUP;
	TqInt index = 0;
	for( iUP = pPoints->aUserParams().begin(); iUP != pPoints->aUserParams().end(); iUP++, index++ )
		if( (*iUP)->strName() == "constantwidth" && (*iUP)->Type() == type_float && (*iUP)->Class() == class_constant )
			m_constantwidthParamIndex = index;
		else if( (*iUP)->strName() == "width" && (*iUP)->Type() == type_float && (*iUP)->Class() == class_varying )
			m_widthParamIndex = index;

	STATS_INC( GPR_points );
}


//---------------------------------------------------------------------
/** Create a clone of this points class
 */

CqSurface* CqPoints::Clone() const 
{
	// Make a 'complete' clone of this primitive, which means cloning the points too.
	CqPolygonPoints* clone_points = static_cast<CqPolygonPoints*>(m_pPoints->Clone());

	CqPoints* clone = new CqPoints(m_nVertices, boost::shared_ptr<CqPolygonPoints>(clone_points));
	CqSurface::CloneData( clone );

//	clone->m_nVertices = m_nVertices;
//	clone->m_pPoints = boost::shared_ptr<CqPolygonPoints>(clone_points);

//	clone->m_KDTreeData.SetpPoints( clone );

//	clone->m_widthParamIndex = m_widthParamIndex;
//	clone->m_constantwidthParamIndex = m_constantwidthParamIndex;
//	clone->m_MaxWidth = m_MaxWidth;

	clone->InitialiseKDTree();
	clone->InitialiseMaxWidth();

	return ( clone );
}




//---------------------------------------------------------------------
/** Dice the quadric into a grid of MPGs for rendering.
 */

CqMicroPolyGridBase* CqPoints::Dice()
{
	assert( pPoints() );

	CqMicroPolyGridPoints* pGrid = new CqMicroPolyGridPoints();
	pGrid->Initialise(nVertices(), 1, shared_from_this());

	TqInt lUses = Uses();

	// Dice the primitive variables.
	if ( USES( lUses, EnvVars_Cs ) && ( pGrid->pVar(EnvVars_Cs) ) )
	{
		if ( pPoints()->bHasVar(EnvVars_Cs) )
			NaturalDice( pPoints()->Cs(), nVertices(), 1, pGrid->pVar(EnvVars_Cs) );
		else if ( NULL != pAttributes() ->GetColorAttribute( "System", "Color" ) )
			pGrid->pVar(EnvVars_Cs) ->SetColor( pAttributes() ->GetColorAttribute( "System", "Color" ) [ 0 ] );
		else
			pGrid->pVar(EnvVars_Cs) ->SetColor( CqColor( 1, 1, 1 ) );
	}

	if ( USES( lUses, EnvVars_Os ) && ( NULL != pGrid->pVar(EnvVars_Os) ) )
	{
		if ( pPoints()->bHasVar(EnvVars_Os) )
			NaturalDice( pPoints()->Os(), nVertices(), 1, pGrid->pVar(EnvVars_Os) );
		else if ( NULL != pAttributes() ->GetColorAttribute( "System", "Opacity" ) )
			pGrid->pVar(EnvVars_Os) ->SetColor( pAttributes() ->GetColorAttribute( "System", "Opacity" ) [ 0 ] );
		else
			pGrid->pVar(EnvVars_Os) ->SetColor( CqColor( 1, 1, 1 ) );
	}

	if ( USES( lUses, EnvVars_s ) && ( NULL != pGrid->pVar(EnvVars_s) ) && pPoints()->bHasVar(EnvVars_s) )
		NaturalDice( pPoints()->s(), nVertices(), 1, pGrid->pVar(EnvVars_s) );

	if ( USES( lUses, EnvVars_t ) && ( NULL != pGrid->pVar(EnvVars_t) ) && pPoints()->bHasVar(EnvVars_t) )
		NaturalDice( pPoints()->t(), nVertices(), 1, pGrid->pVar(EnvVars_t) );

	if ( USES( lUses, EnvVars_u ) && ( NULL != pGrid->pVar(EnvVars_u) ) && pPoints()->bHasVar(EnvVars_u) )
		NaturalDice( pPoints()->u(), nVertices(), 1, pGrid->pVar(EnvVars_u) );

	if ( USES( lUses, EnvVars_v ) && ( NULL != pGrid->pVar(EnvVars_v) ) && pPoints()->bHasVar(EnvVars_v) )
		NaturalDice( pPoints()->v(), nVertices(), 1, pGrid->pVar(EnvVars_v) );


	if ( NULL != pGrid->pVar(EnvVars_P) )
		NaturalDice( pPoints( 0 )->P(), nVertices(), 1, pGrid->pVar(EnvVars_P) );

	// If the shaders need N and they have been explicitly specified, then bilinearly interpolate them.
	if ( USES( lUses, EnvVars_N ) && ( NULL != pGrid->pVar(EnvVars_N) ) && pPoints()->bHasVar(EnvVars_N) )
	{
		NaturalDice( pPoints()->N(), nVertices(), 1, pGrid->pVar(EnvVars_N) );
		pGrid->SetbShadingNormals( true );
	}

	if ( USES( lUses, EnvVars_Ng ) )
	{
		CqVector3D	N(0,0,-1);
		//N = QGetRenderContext() ->matSpaceToSpace( "camera", "object", NULL, pGrid->pTransform() ) * N;
		TqUint u;
		for ( u = 0; u < nVertices(); u++ )
		{
			pGrid->pVar(EnvVars_Ng)->SetNormal( N, u );
		}
		pGrid->SetbGeometricNormals( true );
	}

	// Now we need to dice the user specified parameters as appropriate.
	std::vector<CqParameter*>::iterator iUP;
	std::vector<CqParameter*>::iterator end = pPoints()->aUserParams().end();
	for ( iUP = pPoints()->aUserParams().begin(); iUP != end ; iUP++ )
	{
		/// \todo: Must transform point/vector/normal/matrix parameter variables from 'object' space to current before setting.
		boost::shared_ptr<IqShader> pShader;
		if ( pShader = pGrid->pAttributes() ->pshadSurface(QGetRenderContext()->Time()) )
		{
			IqShaderData* pVar = pShader->FindArgument( ( *iUP )->strName() );
			if ( NULL != pVar )
			{
				/// \todo: Find out how to handle arrays.
				if(pVar->Type() == ( *iUP )->Type())
					NaturalDice( ( *iUP ), nVertices(), 1, pVar );
			}
		}

		if ( pShader = pGrid->pAttributes() ->pshadDisplacement(QGetRenderContext()->Time()) )
		{
			IqShaderData* pVar = pShader->FindArgument( ( *iUP )->strName() );
			if ( NULL != pVar )
			{
				/// \todo: Find out how to handle arrays.
				if(pVar->Type() == ( *iUP )->Type())
					NaturalDice( ( *iUP ), nVertices(), 1, pVar );
			}
		}

		if ( pShader = pGrid->pAttributes() ->pshadAtmosphere(QGetRenderContext()->Time()) )
		{
			IqShaderData* pVar = pShader->FindArgument( ( *iUP )->strName() );
			if ( NULL != pVar )
			{
				/// \todo: Find out how to handle arrays.
				if(pVar->Type() == ( *iUP )->Type())
					NaturalDice( ( *iUP ), nVertices(), 1, pVar );
			}
		}
	}

	return( pGrid );
}


namespace {

/** \brief Implementation of dicing for points (helper function for CqPoints::NaturalDice)
 *
 * \param pParam - pointer to the parameter to take values from
 * \param paramIdx - indices in the full parameter array (pParam) which
 *                   correspond to the current gprim
 * \param diceSize - number of points in the diced output
 * \param pData - destination for diced shader data.
 */
template <class T, class SLT>
void pointsNaturalDice(CqParameter* pParam, const std::vector<TqInt>& paramIdx,
		TqInt diceSize, IqShaderData* pData)
{
	CqParameterTyped<T, SLT>* pTParam = static_cast<CqParameterTyped<T, SLT>*>(pParam);
	for(TqInt i = 0; i < diceSize; i++ )
	{
		IqShaderData* arrayValue;
		for(TqInt j = 0; j < pTParam->Count(); j++)
		{
			arrayValue = pData->ArrayEntry(j);
			arrayValue->SetValue( paramToShaderType<SLT, T>( pTParam->pValue() [ paramIdx[i] ] ), i );
		}
	}
}

} // unnamed namespace

void CqPoints::NaturalDice(CqParameter* pParam, TqInt uDiceSize, TqInt vDiceSize,
		IqShaderData* pData )
{
	switch(pParam->Type())
	{
		case type_float:
			pointsNaturalDice<TqFloat, TqFloat>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_integer:
			pointsNaturalDice<TqInt, TqFloat>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_point:
		case type_vector:
		case type_normal:
			pointsNaturalDice<CqVector3D, CqVector3D>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_hpoint:
			pointsNaturalDice<CqVector4D, CqVector3D>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_color:
			pointsNaturalDice<CqColor, CqColor>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_string:
			pointsNaturalDice<CqString, CqString>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		case type_matrix:
			pointsNaturalDice<CqMatrix, CqMatrix>(pParam, m_KDTree.aLeaves(), uDiceSize, pData);
			break;
		default:
			// left blank to avoid compiler warnings about unhandled types
			break;
	}
}

//---------------------------------------------------------------------
/** Determine whether the quadric is suitable for dicing.
 */

bool	CqPoints::Diceable()
{
	TqUint gridsize = 256;

	const TqInt* poptGridSize = QGetRenderContext() ->poptCurrent()->GetIntegerOption( "limits", "gridsize" );
	if ( poptGridSize )
		gridsize = (TqUint) poptGridSize[ 0 ];

	if( nVertices() > gridsize )
		return ( false );
	else
		return ( true );
}


//---------------------------------------------------------------------
/** Get the geometric bound of this GPrim in 'current' space.
 */

void	CqPoints::Bound(CqBound* bound) const
{
/*	for ( t = 0; t < cTimes(); t++ )
	{
		CqPolygonPoints* pTimePoints = pPoints( t ).get();*/
		for( TqUint i = 0; i < nVertices(); i++ )
			bound->Encapsulate( vectorCast<CqVector3D>(m_pPoints->P()->pValue(m_KDTree.aLeaves()[i])[0]) );
/*	}*/

	// Expand the bound to take into account the width of the particles.
	bound->vecMax() += CqVector3D( m_MaxWidth, m_MaxWidth, m_MaxWidth );
	bound->vecMin() -= CqVector3D( m_MaxWidth, m_MaxWidth, m_MaxWidth );

	AdjustBoundForTransformationMotion( bound );
}

//---------------------------------------------------------------------
/** Split this GPrim into bicubic patches.
 */

TqInt CqPoints::Split( std::vector<boost::shared_ptr<CqSurface> >& aSplits )
{
 	TqInt median = nVertices()/2;
 	// Split the KDTree and create two new primitives containing the split points set.
 	boost::shared_ptr<CqPoints> pA( new CqPoints( m_nVertices, pPoints() ) );
 	boost::shared_ptr<CqPoints> pB( new CqPoints( m_nVertices, pPoints() ) );
 
 	pA->m_nVertices = median;
 	pB->m_nVertices = nVertices()-median;
 
 	pA->SetSurfaceParameters( *this );
 	pB->SetSurfaceParameters( *this );
 
 	KDTree().Subdivide( pA->KDTree(), pB->KDTree() );
 
 	aSplits.push_back( pA );
 	aSplits.push_back( pB );
 
 	return( 2 );
}


//---------------------------------------------------------------------
/** Split the points, taking the split information from the specified donor points surfaces.
 */

TqInt CqPoints::CopySplit( std::vector<boost::shared_ptr<CqSurface> >& aSplits, CqPoints* pFrom1, CqPoints* pFrom2 )
{
	// Split the KDTree and create two new primitives containing the split points set.
 	boost::shared_ptr<CqPoints> pA( new CqPoints( m_nVertices, pPoints() ) );
 	boost::shared_ptr<CqPoints> pB( new CqPoints( m_nVertices, pPoints() ) );
 
 	pA->m_nVertices = pFrom1->m_nVertices;
 	pB->m_nVertices = pFrom2->m_nVertices;
 
 	pA->SetSurfaceParameters( *this );
 	pB->SetSurfaceParameters( *this );
 
 	pA->KDTree() = pFrom1->KDTree();
 	pB->KDTree() = pFrom2->KDTree();
 
 	aSplits.push_back( pA );
 	aSplits.push_back( pB );
 
 	return( 2 );
}


//---------------------------------------------------------------------
/** Initialise the KDTree to contain all the points in the list. Settign the
 * index list to the canonical form.
 */

void CqPoints::InitialiseKDTree()
{
	m_KDTree.aLeaves().reserve( nVertices() );
	for( TqUint i = 0; i < nVertices(); i++ )
		m_KDTree.aLeaves().push_back( i );
}


void CqPoints::InitialiseMaxWidth()
{
	TqInt cu = nVertices();	// Only need cu, as we know cv is 1.

	CqMatrix matObjectToCamera;
	QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pTransform().get(), QGetRenderContext()->Time(), matObjectToCamera );
	const CqParameterTypedConstant<TqFloat, type_float, TqFloat>* pConstantWidthParam = constantwidth( );

	CqVector3D Point0 = matObjectToCamera * CqVector3D(0,0,0);

	TqFloat i_radius = 1.0f;
	if( NULL != pConstantWidthParam )
		i_radius = pConstantWidthParam->pValue( 0 )[ 0 ];
	for ( TqInt iu = 0; iu < cu; iu++ )
	{
		TqFloat radius;
		// Find out if the "width" parameter was specified.
		CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pWidthParam = width( 0 );

		if( NULL != pWidthParam )
			i_radius = pWidthParam->pValue( KDTree().aLeaves()[ iu ] )[ 0 ];

		radius = i_radius;
		// Get point in camera space.
		CqVector3D Point1 = matObjectToCamera * CqVector3D(radius,0,0);
		radius = (Point1-Point0).Magnitude();

		m_MaxWidth = max(m_MaxWidth, radius );
	}
}

//---------------------------------------------------------------------
/** Transform the points by the transformation matrices provided.
 */

void	CqPoints::Transform( const CqMatrix& matTx, const CqMatrix& matITTx, const CqMatrix& matRTx, TqInt iTime )
{
	CqPolygonPoints* pTimePoints = pPoints( iTime ).get();
	pTimePoints->Transform(matTx, matITTx, matRTx, 0);
}

void CqMicroPolyGridPoints::Split( long xmin, long xmax, long ymin, long ymax )
{
	if ( NULL == pVar(EnvVars_P) )
		return ;

	TqInt cu = uGridRes();	// Only need cu, as we know cv is 1.

	ADDREF( this );

	CqMatrix matCameraToObject0;
	QGetRenderContext() ->matSpaceToSpace( "camera", "object", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matCameraToObject0 );
	CqMatrix matObjectToCamera;
	QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matObjectToCamera );
	CqMatrix matCameraToRaster;
	QGetRenderContext() ->matSpaceToSpace( "camera", "raster", NULL, NULL, pSurface()->pTransform()->Time(0), matCameraToRaster );

	CqVector3D vecdefOriginRaster = matCameraToRaster * CqVector3D( 0.0f,0.0f,0.0f );

	// Get a pointer to the surface, so that we can interrogate the "width" parameters.
	CqPoints* pPoints = static_cast<CqPoints*>( pSurface() );

	const CqParameterTypedConstant<TqFloat, type_float, TqFloat>* pConstantWidthParam = pPoints->constantwidth( );

	AQSIS_TIMER_START(Project_points);
	// Transform the whole grid to hybrid camera/raster space

	CqVector3D* pP;
	pVar(EnvVars_P) ->GetPointPtr( pP );

	AQSIS_TIMER_STOP(Project_points);

	TqInt iu;
	TqInt iTime, tTime = pSurface()->pTransform()->cTimes();

	if( tTime > 1 )
	{
		// Get an array of P's for all time positions.
		std::vector<std::vector<CqVector3D> > aaPtimes;
		aaPtimes.resize( pSurface()->pTransform()->cTimes() );

		// Array of cached object to camera matrices for each time slot.
		std::vector<CqMatrix>	amatObjectToCameraT;
		amatObjectToCameraT.resize( pSurface()->pTransform()->cTimes() );
		std::vector<CqMatrix>	amatNObjectToCameraT;
		amatNObjectToCameraT.resize( pSurface()->pTransform()->cTimes() );

		CqMatrix matObjectToCameraT;
		register TqInt i;
		TqInt gsmin1 = GridSize() - 1;


		for( iTime = 0; iTime < tTime; iTime++ )
		{
			CqMatrix matCameraToObjectT;
			QGetRenderContext() ->matSpaceToSpace( "camera", "object", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time( iTime ), matCameraToObjectT );
			QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(),  pSurface()->pTransform()->Time( iTime ), amatObjectToCameraT[ iTime ] );
			QGetRenderContext() ->matNSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time( iTime ), amatNObjectToCameraT[ iTime ] );

			aaPtimes[ iTime ].resize( gsmin1 + 1 );

			for ( i = gsmin1; i >= 0; i-- )
			{
				// This makes sure all our points are in object space.
				aaPtimes[ iTime ][ i ] = matCameraToObject0 * pP[ i ];
			}
		}

		for ( iu = 0; iu < cu; iu++ )
		{
			CqMicroPolygonMotionPoints* pNew = new CqMicroPolygonMotionPoints( this, iu );

			TqFloat radius;
			TqFloat i_radius = 1.0f;
			if( NULL != pConstantWidthParam )
				i_radius = pConstantWidthParam->pValue( 0 )[ 0 ];
			// Find out if the "width" parameter was specified.
			CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pWidthParam = pPoints->width( 0 );

			if( NULL != pWidthParam )
				i_radius = pWidthParam->pValue( pPoints->KDTree().aLeaves()[ iu ] )[ 0 ];

			for( iTime = 0; iTime < tTime; iTime++ )
			{
				radius = i_radius;
				// Get point in camera space.
				CqVector3D Point, pt, vecCamP;
				Point = pt = vecCamP = amatObjectToCameraT[ iTime ] * aaPtimes[ iTime ][ iu ];
				// Ensure z is retained in camera space when we convert to raster.
				TqFloat ztemp = Point.z();
				Point = matCameraToRaster * Point;
				Point.z( ztemp );
				pP[ iu ] = Point;

				// first, create a horizontal vector in object space which is
				//  the length of the current width.
				CqVector3D horiz( 1, 0, 0 );
				horiz = amatNObjectToCameraT[ iTime ] * horiz;
				horiz *= radius / horiz.Magnitude();

				// Get the current point in object space.
				CqVector3D pt_delta = pt + horiz;
				pt = amatObjectToCameraT[ iTime ] * pt;
				pt_delta = amatObjectToCameraT[ iTime ] * pt_delta;

				// finally, find the difference between the two points in
				//  the new space - this is the transformed width
				CqVector3D widthVector = pt_delta - pt;
				radius = widthVector.Magnitude();

				CqVector3D vecCamP2 = vecCamP + CqVector3D( radius, 0.0f, 0.0f );
				ztemp = vecCamP2.z();
				CqVector3D vecRasP2 = matCameraToRaster * vecCamP2;
				vecRasP2.z( ztemp );
				TqFloat ras_radius = ( vecRasP2 - Point ).Magnitude();
				radius = ras_radius * 0.5f;

				pNew->AppendKey( Point, radius, pSurface()->pTransform()->Time( iTime ) );
			}
			boost::shared_ptr<CqMicroPolygon> pMP( pNew );
			QGetRenderContext()->pImage()->AddMPG( pMP );
		}
	}
	else
	{
		iTime = 0;
		CqMatrix matWorldToObjectT;
		QGetRenderContext() ->matSpaceToSpace( "world", "object", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time( iTime ), matWorldToObjectT );
		CqMatrix matObjectToCameraT;
		QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time( iTime ), matObjectToCameraT );
		CqMatrix matNObjectToCameraT;
		QGetRenderContext() ->matNSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time( iTime ), matNObjectToCameraT );

		for ( iu = 0; iu < cu; iu++ )
		{
			// Get point in camera space.
			CqVector3D Point, pt, vecCamP;
			Point = pt = vecCamP = pP[ iu ];
			// Ensure z is retained in camera space when we convert to raster.
			TqFloat ztemp = Point.z();
			Point = matCameraToRaster * Point;
			Point.z( ztemp );
			pP[ iu ] = Point;

			TqFloat radius = 1.0f;
			if( NULL != pConstantWidthParam )
				radius = pConstantWidthParam->pValue( 0 )[ 0 ];
			// Find out if the "width" parameter was specified.
			CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pWidthParam = pPoints->width( 0 );

			if( NULL != pWidthParam )
				radius = pWidthParam->pValue( pPoints->KDTree().aLeaves()[ iu ] )[ 0 ];

			// first, create a horizontal vector in camera space which is
			//  the length of the current width in current space
			CqVector3D horiz( 1, 0, 0 );
			horiz = matNObjectToCameraT * horiz;
			horiz *= radius / horiz.Magnitude();

			// Get the current point in object space.
			CqVector3D pt_delta = pt + horiz;
			pt = matObjectToCameraT * pt;
			pt_delta = matObjectToCameraT * pt_delta;

			// finally, find the difference between the two points in
			//  the new space - this is the transformed width
			CqVector3D widthVector = pt_delta - pt;
			radius = widthVector.Magnitude();

			CqVector3D vecCamP2 = vecCamP + CqVector3D( radius, 0.0f, 0.0f );
			ztemp = vecCamP2.z();
			CqVector3D vecRasP2 = matCameraToRaster * vecCamP2;
			vecRasP2.z( ztemp );
			TqFloat ras_radius = ( vecRasP2 - Point ).Magnitude();
			radius = ras_radius * 0.5f;

			CqMicroPolygonPoints* pNew = new CqMicroPolygonPoints(this, iu);
			pNew->Initialise( radius );

			boost::shared_ptr<CqMicroPolygon> pMP( pNew );
			QGetRenderContext()->pImage()->AddMPG( pMP );
		}
	}

	RELEASEREF( this );
}


bool CqMicroPolygonPoints::Sample( CqHitTestCache& hitTestCache, SqSampleData const& sample, TqFloat& D, TqFloat time, bool UsingDof ) const
{
	const CqVector2D& vecSample = sample.position;

	CqVector3D P;
	pGrid()->pVar(EnvVars_P)->GetPoint(P, m_Index);
	if( (CqVector2D( P.x(), P.y() ) - vecSample).Magnitude() < m_radius )
	{
		D = P.z();
		return( true );
	}
	return( false );
}

void CqMicroPolygonPoints::CacheOutputInterpCoeffs(SqMpgSampleInfo& cache) const
{
	CacheOutputInterpCoeffsConstant(cache);
}

void CqMicroPolygonPoints::InterpolateOutputs(const SqMpgSampleInfo& cache,
		const CqVector2D& pos, CqColor& outCol, CqColor& outOpac) const
{
	outCol = cache.color;
	outOpac = cache.opacity;
}


//---------------------------------------------------------------------
/** Split the micropolygrid into individual MPGs,
 * \param xmin Integer minimum extend of the image part being rendered, takes into account buckets and clipping.
 * \param xmax Integer maximum extend of the image part being rendered, takes into account buckets and clipping.
 * \param ymin Integer minimum extend of the image part being rendered, takes into account buckets and clipping.
 * \param ymax Integer maximum extend of the image part being rendered, takes into account buckets and clipping.
 */

void CqMotionMicroPolyGridPoints::Split( long xmin, long xmax, long ymin, long ymax )
{
	// Get the main object, the one that was shaded.
	CqMicroPolyGrid * pGridA = static_cast<CqMicroPolyGridPoints*>( GetMotionObject( Time( 0 ) ) );
	TqInt iTime;

	assert(NULL != pGridA->pVar(EnvVars_P) );

	ADDREF( pGridA );

	TqInt cu = pGridA->uGridRes();	// Only need cu, as we know cv is 1.

	CqMatrix matCameraToObject0;
	QGetRenderContext() ->matSpaceToSpace( "camera", "object", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matCameraToObject0 );
	CqMatrix matObjectToCamera;
	QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matObjectToCamera );
	CqMatrix matCameraToRaster;
	QGetRenderContext() ->matSpaceToSpace( "camera", "raster", NULL, NULL, pSurface()->pTransform()->Time(0), matCameraToRaster );

	CqMatrix matTx;
	QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matTx );
	CqMatrix matITTx;
	QGetRenderContext() ->matNSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), pSurface()->pTransform()->Time(0), matITTx );

	CqVector3D vecdefOriginRaster = matCameraToRaster * CqVector3D( 0.0f,0.0f,0.0f );

	TqInt NumTimes = cTimes();

	// Get an array of P's for all time positions.
	std::vector<std::vector<CqVector3D> > aaPtimes;
	aaPtimes.resize( NumTimes );

	// Array of cached object to camera matrices for each time slot.
	std::vector<CqMatrix>	amatObjectToCameraT;
	amatObjectToCameraT.resize( NumTimes );
	std::vector<CqMatrix>	amatNObjectToCameraT;
	amatNObjectToCameraT.resize( NumTimes );

	CqMatrix matObjectToCameraT;
	register TqInt i;
	TqInt gsmin1 = pGridA->pShaderExecEnv()->shadingPointCount() - 1;

	for( iTime = 0; iTime < NumTimes; iTime++ )
	{
		CqMatrix matCameraToObjectT;
		QGetRenderContext() ->matSpaceToSpace( "camera", "object", NULL, pSurface()->pTransform().get(), Time( iTime ), matCameraToObjectT );
		QGetRenderContext() ->matSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(),  Time( iTime ), amatObjectToCameraT[ iTime ] );
		QGetRenderContext() ->matNSpaceToSpace( "object", "camera", NULL, pSurface()->pTransform().get(), Time( iTime ), amatNObjectToCameraT[ iTime ] );

		aaPtimes[ iTime ].resize( gsmin1 + 1 );

		CqMicroPolyGridPoints* pGridT = static_cast<CqMicroPolyGridPoints*>( GetMotionObject( Time( iTime ) ) );

		CqVector3D* pP;
		pGridT->pVar(EnvVars_P) ->GetPointPtr( pP );

		for ( i = gsmin1; i >= 0; i-- )
		{
			// This makes sure all our points are in object space.
			aaPtimes[ iTime ][ i ] = matCameraToObject0 * pP[ i ];
		}
	}

	TqInt iu;
	for ( iu = 0; iu < cu; iu++ )
	{
		CqMicroPolygonMotionPoints* pNew = new CqMicroPolygonMotionPoints( pGridA, iu );

		TqFloat radius;
		TqInt iTime;
		for( iTime = 0; iTime < NumTimes; iTime++ )
		{
			radius = 1.0f;

			CqMicroPolyGridPoints* pGridT = static_cast<CqMicroPolyGridPoints*>( GetMotionObject( Time( iTime ) ) );

			// Get a pointer to the surface, so that we can interrogate the "width" parameters.
			CqPoints* pPoints = static_cast<CqPoints*>( pGridT->pSurface() );

			const CqParameterTypedConstant<TqFloat, type_float, TqFloat>* pConstantWidthParam =
			    pPoints->constantwidth( );

			if( NULL != pConstantWidthParam )
				radius = pConstantWidthParam->pValue( 0 )[ 0 ];

			// Find out if the "width" parameter was specified.
			CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pWidthParam = pPoints->width( 0 );

			if( NULL != pWidthParam )
				radius = pWidthParam->pValue( pPoints->KDTree().aLeaves()[ iu ] )[ 0 ];

			// Get point in camera space.
			CqVector3D Point, pt, vecCamP;
			Point = pt = vecCamP = amatObjectToCameraT[ iTime ] * aaPtimes[ iTime ][ iu ];
			// Ensure z is retained in camera space when we convert to raster.
			TqFloat ztemp = Point.z();
			Point = matCameraToRaster * Point;
			Point.z( ztemp );

			// first, create a horizontal vector in object space which is
			//  the length of the current width.
			CqVector3D horiz( 1, 0, 0 );
			horiz = amatNObjectToCameraT[ iTime ] * horiz;
			horiz *= radius / horiz.Magnitude();

			// Get the current point in object space.
			CqVector3D pt_delta = pt + horiz;
			pt = amatObjectToCameraT[ iTime ] * pt;
			pt_delta = amatObjectToCameraT[ iTime ] * pt_delta;

			// finally, find the difference between the two points in
			//  the new space - this is the transformed width
			CqVector3D widthVector = pt_delta - pt;
			radius = widthVector.Magnitude();

			CqVector3D vecCamP2 = vecCamP + CqVector3D( radius, 0.0f, 0.0f );
			ztemp = vecCamP2.z();
			CqVector3D vecRasP2 = matCameraToRaster * vecCamP2;
			vecRasP2.z( ztemp );
			TqFloat ras_radius = ( vecRasP2 - Point ).Magnitude();
			radius = ras_radius * 0.5f;

			pNew->AppendKey( Point, radius, Time( iTime ) );
		}
		boost::shared_ptr<CqMicroPolygon> pMP( pNew );
		QGetRenderContext()->pImage()->AddMPG( pMP );
	}

	RELEASEREF( pGridA );

	// Delete the donor motion grids, as their work is done.
	/*    for ( iTime = 1; iTime < cTimes(); iTime++ )
	    {
	        CqMicroPolyGrid* pg = static_cast<CqMicroPolyGrid*>( GetMotionObject( Time( iTime ) ) );
	        if ( NULL != pg )
	            RELEASEREF( pg );
	    }
	*/    //		delete( GetMotionObject( Time( iTime ) ) );
}


//---------------------------------------------------------------------
/** Calculate a list of 2D bounds for this micropolygon,
 */

void CqMicroPolygonMotionPoints::BuildBoundList( TqUint timeRanges )
{
	m_BoundList.Clear();

	assert( NULL != m_Keys[0] );

	CqBound start = m_Keys[0]->GetBound();
	TqFloat	startTime = m_Times[ 0 ];
	TqInt cTimes = m_Keys.size();
	for ( TqInt i = 1; i < cTimes; i++ )
	{
		CqBound end = m_Keys[i]->GetBound();
		CqBound mid0( start );
		CqBound mid1;
		TqFloat endTime = m_Times[ i ];
		TqFloat time = startTime;

		TqInt d;
		// arbitary number of divisions, should be related to screen size of blur
		TqInt divisions = 4;
		TqFloat delta = 1.0f / static_cast<TqFloat>( divisions );
		m_BoundList.SetSize( divisions );
		for ( d = 1; d <= divisions; d++ )
		{
			mid1.vecMin() = delta * ( end.vecMin() - start.vecMin() ) + start.vecMin();
			mid1.vecMax() = delta * ( end.vecMax() - start.vecMax() ) + start.vecMax();
			mid0.Encapsulate(&mid1);
			m_BoundList.Set( d-1, mid0, time );
			time = delta * ( endTime - startTime ) + startTime;
			mid0 = mid1;
			delta += delta;
		}
		start = end;
		startTime = endTime;
	}
	m_BoundReady = true;
}


//---------------------------------------------------------------------
/** Sample the specified point against the MPG at the specified time.
 * \param vecSample 2D vector to sample against.
 * \param time Shutter time to sample at.
 * \param D Storage for depth if valid hit.
 * \return Boolean indicating smaple hit.
 */

bool CqMicroPolygonMotionPoints::Sample( CqHitTestCache& hitTestCache, SqSampleData const& sample, TqFloat& D, TqFloat time, bool UsingDof ) const
{
	const CqVector2D& vecSample = sample.position;
	return( fContains( vecSample, D, time ) );
}

void CqMicroPolygonMotionPoints::CacheOutputInterpCoeffs(SqMpgSampleInfo& cache) const
{
	CacheOutputInterpCoeffsConstant(cache);
}

void CqMicroPolygonMotionPoints::InterpolateOutputs(const SqMpgSampleInfo& cache,
		const CqVector2D& pos, CqColor& outCol, CqColor& outOpac) const
{
	outCol = cache.color;
	outOpac = cache.opacity;
}

//---------------------------------------------------------------------
/** Store the vectors of the micropolygon at the specified shutter time.
 * \param vA 3D Vector.
 * \param vB 3D Vector.
 * \param vC 3D Vector.
 * \param vD 3D Vector.
 * \param time Float shutter time that this MPG represents.
 */

void CqMicroPolygonMotionPoints::AppendKey( const CqVector3D& vA, TqFloat radius, TqFloat time )
{
	//	assert( time >= m_Times.back() );

	// Add a new planeset at the specified time.
	CqMovingMicroPolygonKeyPoints* pMP = new CqMovingMicroPolygonKeyPoints( vA, radius );
	m_Times.push_back( time );
	m_Keys.push_back( pMP );
	if ( m_Times.size() == 1 )
		m_Bound = pMP->GetBound();
	else
	{
		CqBound B(pMP->GetBound());
		m_Bound.Encapsulate( &B );
	}
}


//---------------------------------------------------------------------
/** Determinde whether the 2D point specified lies within this micropolygon.
 * \param vecP 2D vector to test for containment.
 * \param Depth Place to put the depth if valid intersection.
 * \param time The frame time at which to check containment.
 * \return Boolean indicating sample hit.
 */

bool CqMicroPolygonMotionPoints::fContains( const CqVector2D& vecP, TqFloat& Depth, TqFloat time ) const
{
	TqInt iIndex = 0;
	TqFloat Fraction = 0.0f;
	bool Exact = true;

	if ( time > m_Times.front() )
	{
		if ( time >= m_Times.back() )
			iIndex = m_Times.size() - 1;
		else
		{
			// Find the appropriate time span.
			iIndex = 0;
			while ( time >= m_Times[ iIndex + 1 ] )
				iIndex += 1;
			Fraction = ( time - m_Times[ iIndex ] ) / ( m_Times[ iIndex + 1 ] - m_Times[ iIndex ] );
			Exact = ( m_Times[ iIndex ] == time );
		}
	}

	if( Exact )
	{
		CqMovingMicroPolygonKeyPoints* pMP1 = m_Keys[ iIndex ];
		return( pMP1->fContains( vecP, Depth, time ) );
	}
	else
	{
		CqMovingMicroPolygonKeyPoints* pMP1 = m_Keys[ iIndex ];
		CqMovingMicroPolygonKeyPoints* pMP2 = m_Keys[ iIndex + 1 ];
		// Check against each line of the quad, if outside any then point is outside MPG, therefore early exit.
		CqVector3D MidPoint = ( ( pMP2->m_Point0 - pMP1->m_Point0 ) * Fraction ) + pMP1->m_Point0;
		TqFloat MidRadius = ( ( pMP2->m_radius - pMP1->m_radius ) * Fraction ) + pMP1->m_radius;
		if( (CqVector2D( MidPoint.x(), MidPoint.y() ) - vecP).Magnitude() < MidRadius )
		{
			Depth = MidPoint.z();
			return( true );
		}
		return ( false );
	}
}



} // namespace Aqsis
//---------------------------------------------------------------------


