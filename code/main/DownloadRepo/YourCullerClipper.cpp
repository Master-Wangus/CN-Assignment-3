/*!
@file    YourCullerClipper.cpp
@author  Prasanna Ghali       (pghali@digipen.edu)

All content (c) 2002 DigiPen Institute of Technology, all rights reserved.
*//*__________________________________________________________________________*/

/*                                                                   includes
----------------------------------------------------------------------------- */

#include "YourCullerClipper.h"

/*                                                                  functions
----------------------------------------------------------------------------- */
/*  _________________________________________________________________________ */
gfxFrustum YourClipper::
/*! Get view frame frustum plane equations

  @param perspective_mtx	--> Matrix manifestation of perspective (or, orthographic)
  transform.

  @return	--> gfxFrustum
  Plane equations of the six surfaces that specify the view volume in view frame.
*/
ComputeFrustum(gfxMatrix4 const& perspective_mtx)
{
	//@todo Implement me.
	  //left side = -r0 -r3
	  //right side = r0 -r3
	  //btm = -r1 -r3
	  //top = r1-r3
	  //near = -r2-r3
	  //far = r2-r3
	gfxVector4 r0 = perspective_mtx.GetCol4(0);
	gfxVector4 r1 = perspective_mtx.GetCol4(1);
	gfxVector4 r2 = perspective_mtx.GetCol4(2);
	gfxVector4 r3 = perspective_mtx.GetCol4(3);

	gfxVector3 r0vec3 = perspective_mtx.GetCol3(0);
	gfxVector3 r1vec3 = perspective_mtx.GetCol3(1);
	gfxVector3 r2vec3 = perspective_mtx.GetCol3(2);
	gfxVector3 r3vec3 = perspective_mtx.GetCol3(3);

	float r0vec3_length = r0vec3.Length();
	float r1vec3_length = r1vec3.Length();
	float r2vec3_length = r2vec3.Length();
	float r3vec3_length = r3vec3.Length();

	//normalize r0 vector
	r0.x /= r0vec3_length;
	r0.y /= r0vec3_length;
	r0.z /= r0vec3_length;
	r0.w /= r0vec3_length;

	//normalize r1 vector
	r1.x /= r1vec3_length;
	r1.y /= r1vec3_length;
	r1.z /= r1vec3_length;
	r1.w /= r1vec3_length;

	//normalize r2 vector
	r2.x /= r2vec3_length;
	r2.y /= r2vec3_length;
	r2.z /= r2vec3_length;
	r2.w /= r2vec3_length;

	//normalize r3 vector
	r3.x /= r3vec3_length;
	r3.y /= r3vec3_length;
	r3.z /= r3vec3_length;
	r3.w /= r3vec3_length;

	gfxVector4 leftSide = (-1 * (r0 - r3)) ;
	gfxVector4 rightSide = (r0 - r3);
	gfxVector4 btm = (- 1 * (r1 - r3));
	gfxVector4 top = (r1 - r3);
	gfxVector4 nearside = ( - 1 * (r2 - r3));
	gfxVector4 farside = (r2 - r3);


	//create a gameobject
	gfxFrustum gO;

	//set the vector of the calculated position into leftside
	gO.l = { leftSide.x,leftSide.y,leftSide.z,leftSide.w };
	//set the vector of the calculated position into rightside
	gO.r = { rightSide.x,rightSide.y,rightSide.z,rightSide.w };
	//set the vector of the calculated position into btmside
	gO.b = { btm.x,btm.y,btm.z,btm.w };
	//set the vector of the calculated position into topside
	gO.t = { top.x,top.y,top.z,top.w };
	//set the vector of the calculated position into nearside
	gO.n = { nearside.x,nearside.y,nearside.z,nearside.w };
	//set the vector of the calculated position into farside
	gO.f = { farside.x,farside.y,farside.z,farside.w };
		
    return gO;
}

/*  _________________________________________________________________________ */
bool YourClipper::
/*! Performing culling.

@param bs		--> View-frame definition of the bounding sphere of object
which is being tested for inclusion, exclusion, or
intersection with view frustum.
@param f		--> View-frame frustum plane equations.
@param oc		--> Six-bit flag specifying the frustum planes intersected
by bounding sphere of object. A given bit of the outcode
is set if the sphere crosses the appropriate plane for that
outcode bit - otherwise the bit is cleared.

@return
True if the vertices bounded by the sphere should be culled.
False otherwise.

If the return value is false, the outcode oc indicates which planes
the sphere intersects with. A given bit of the outcode is set if the
sphere crosses the appropriate plane for that outcode bit.
*/
Cull(gfxSphere const& bounding_sphere, gfxFrustum const& frustum, gfxOutCode *ptr_outcode) 
{
  //@todo Implement me.	

   //bool checks
   bool insideOutsideCheck = false;
  *ptr_outcode = 0; // for now, assume bounding sphere completely inside frustum
  
  //loop with 6 as there is only 6 mplanes i never normalized
  //cause i normalized at calculated frustrum
  for (size_t i = 0; i < 6; ++i)
  {
	  const auto& frustumPlane = frustum.mPlanes[i];
	  signed distanceCheck = (frustumPlane.a * bounding_sphere.center.x)
		  + (frustumPlane.b * bounding_sphere.center.y)
		  + (frustumPlane.c * bounding_sphere.center.z)
		  + (frustumPlane.d);
	  //perform a inside outside test for sphere center with respect to plane
	  //inside check
	  // nhat * c + d <= -r
	  if (distanceCheck <= -bounding_sphere.radius)
	  {
		  insideOutsideCheck = true;
	  }
	  //outside check
	  // nhat * c + d > r
	  else if (distanceCheck > bounding_sphere.radius)
	  {
		  insideOutsideCheck = true;
	  }
	  //check if the plan intercept
	  // if -r < nhat * c + d < r
	  else if (distanceCheck > -bounding_sphere.radius && distanceCheck < bounding_sphere.radius)
	  {
		  //bitshift by 1 bit 0001
		  *ptr_outcode |= (1 < i);
		  insideOutsideCheck = false;
	  }
  }
  
  return insideOutsideCheck;	  // for now, assume object is not trivially rejected
}

/*  _________________________________________________________________________ */
gfxVertexBuffer YourClipper::
/*!
Perform clipping.

@param outcode	--> Outcode of view-frame of bounding sphere of object specifying
the view-frame frustum planes that the sphere is straddling.

@param vertex_buffer	--> The input vertex buffer contains three points 
forming a triangle.
Each vertex has x_c, y_c, z_c, and w_c fields that describe the position
of the vertex in clip space. Additionally, each vertex contains an array
of floats (bs[6]) that contains the boundary condition for each clip plane,
and an outcode value specifying which planes the vertex is inside.

The gfxClipPlane enum in GraphicsPipe.h contains the indices into bs
array. gfxClipCode contains bit values for each clip plane code.

If an object's bounding sphere could not be trivially accepted nor rejected,
it is reasonable to expect that the object is straddling only a
subset of the six frustum planes. This means that the object's triangles
need not be clipped against all the six frustum planes but only against
the subset of planes that the bounding sphere is straddling.
Furthermore, even if the bounding sphere is straddling a subset of planes,
the triangles themselves can be trivially accepted or rejected.
To implement the above two insights, use argument outcode - the object bounding
sphere's outcode which was previously returned by Cull().

Notes:
When computing clip frame intersection points, ensure that all necessary information
required to project and rasterize the vertex is computed using linear interpolation
between the inside and outside vertices. This includes:
clip frame coordinates: (c_x, c_y, c_z, c_w),
texture coordinates: (u, v),
vertex color coordinates: (r, g, b, a),
boundary conditions: bc[GFX_CPLEFT], bc[GFX_CPRIGHT], ...
outcode: oc

As explained in class, consistency in computing computing the parameter t
using t = 0 for inside point and t = 1 for outside point helps in preventing
tears and other artifacts.

Although the input primitive is a triangle, after clipping, the output
primitive may be a convex polygon with more than 3 vertices. In that
case, you must produce as output an ordered list of clipped vertices
that form triangles when taken in groups of three.

@return None
*/
Clip(gfxOutCode outcode, gfxVertexBuffer const& vertex_buffer) 
{
	//make a vertex gO so i can modifiy without the const
	gfxVertexBuffer vertex = vertex_buffer;

	// If the triangle is completely inside or outside the clipping region, return an empty vertex buffer
	if (outcode == 0 || ((vertex_buffer[0].oc & vertex_buffer[1].oc & vertex_buffer[2].oc) != 0))
	{
		return gfxVertexBuffer();
	}

	// If the triangle is completely inside the clipping region, return the original vertex buffer
	if ((vertex_buffer[0].oc | vertex_buffer[1].oc | vertex_buffer[2].oc) == 0)
	{
		return vertex_buffer;
	}

	//set a cliplane for me to do projection 
								//left			//right		//bottom	 //top		   //near	    //far
	gfxVector4 clipPlane[6] = { { -1,0,0,-1 }, {1,0,0,-1}, { 0,-1,0,-1 },{ 0,1,0,-1 },{ 0,0,-1,-1 },{ 0,0,1,-1 } };
	//to calculate my intersection value
	gfxVertex intersection{};
	//clip against each of the six clipping planes
	for (size_t i = 0; i < 6; ++i)
	{
		//create a temporary vertex buffer to store the output of the current clipping plane
		std::vector<gfxVertex> tempVertex = vertex_buffer;
		int index{};
		vertex.clear();

		//clip each edge of the polygon against the current clipping plane
		for (size_t j = 0; j < tempVertex.size(); ++j)
		{
			const gfxVertex& p0 = tempVertex[j];

			// Determine the index of the next vertex in the loop
			if (j == tempVertex.size() - 1)
			{
				index = 0;
			}
			else
			{
				index = j + 1;
			}

			//increment to next vert
			const gfxVertex& p1 = tempVertex[index];

			//calculate p0 BC
			float p0BC = p0.x_c * clipPlane[i].x + p0.y_c * clipPlane[i].y +
				p0.z_c * clipPlane[i].z + p0.w_c * clipPlane[i].w;

			//calculate p1 BC
			float p1BC = p1.x_c * clipPlane[i].x + p1.y_c * clipPlane[i].y +
				p1.z_c * clipPlane[i].z + p1.w_c * clipPlane[i].w;

			//calculate t
			float t = p0BC / (p0BC - p1BC);

			//interpolate the intersection with t
			intersection.x_c =  (p1.x_c - p0.x_c) * t;
			intersection.y_c =  (p1.y_c - p0.y_c) * t;
			intersection.z_c =  (p1.z_c - p0.z_c) * t;
			intersection.w_c =  (p1.w_c - p0.w_c) * t;

			intersection.s =  (p1.s - p0.s) * t;
			intersection.t =  (p1.t - p0.t) * t;

			intersection.r = (p1.r - p0.r) * t;
			intersection.g = (p1.g - p0.g) * t;
			intersection.b = (p1.b - p0.b) * t;
			intersection.a = (p1.a - p0.a) * t;

			//clip the edge based on the edge conditions and add the resulting vertex to the output buffer
		
			//rule 1 outside to inside
			if (p0BC > 0 && p1BC <= 0)
			{
				vertex.push_back(intersection);
				vertex.push_back(p1);
			}
			//rule 2 both inside
			else if (p0BC <= 0 && p1BC <= 0)
			{
				vertex.push_back(p1);
			}
			//rule 3 inside to outside
			else if (p0BC <= 0 && p1BC > 0)
			{
				vertex.push_back(intersection);
			}
		}
	}

	//if the resulting vertex buffer contains more than three vertices, triangulate the polygon
	if (vertex.size() > 3)
	{
		gfxVertexBuffer final_vertex_buffer;

		//triangulate the polygon by creating triangles from the vertices
		for (int i = 2; i < vertex.size(); ++i)
		{
			final_vertex_buffer.push_back(vertex[0]);
			final_vertex_buffer.push_back(vertex[i - 1]);
			final_vertex_buffer.push_back(vertex[i]);
		}

		//return the final triangulated vertex buffer
		return final_vertex_buffer;
	}
	else
	{
		return vertex;
	}


}

	

