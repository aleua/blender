/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_PolyProxy.cpp
 *  \ingroup ketsji
 */


#ifdef WITH_PYTHON

#include "KX_PolyProxy.h"
#include "KX_MeshProxy.h"
#include "RAS_MeshObject.h"
#include "RAS_MaterialBucket.h"
#include "RAS_IDisplayArray.h"
#include "KX_VertexProxy.h"
#include "KX_BlenderMaterial.h"
#include "EXP_ListWrapper.h"

#include "KX_PyMath.h"

PyTypeObject KX_PolyProxy::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"KX_PolyProxy",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_PolyProxy::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getNumVertex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isVisible),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isCollider),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialName),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getTextureName),
	KX_PYMETHODTABLE(KX_PolyProxy,getVertexIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMesh),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterial),
	{nullptr,nullptr} //Sentinel
};

EXP_Attribute KX_PolyProxy::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("material_name", KX_PolyProxy, pyattr_get_material_name),
	KX_PYATTRIBUTE_RO_FUNCTION("texture_name", KX_PolyProxy, pyattr_get_texture_name),
	KX_PYATTRIBUTE_RO_FUNCTION("material", KX_PolyProxy, pyattr_get_material),
	KX_PYATTRIBUTE_RO_FUNCTION("material_id", KX_PolyProxy, pyattr_get_material_id),
	KX_PYATTRIBUTE_RO_FUNCTION("v1", KX_PolyProxy, pyattr_get_v1),
	KX_PYATTRIBUTE_RO_FUNCTION("v2", KX_PolyProxy, pyattr_get_v2),
	KX_PYATTRIBUTE_RO_FUNCTION("v3", KX_PolyProxy, pyattr_get_v3),
	KX_PYATTRIBUTE_RO_FUNCTION("v4", KX_PolyProxy, pyattr_get_v4),
	KX_PYATTRIBUTE_RO_FUNCTION("visible", KX_PolyProxy, pyattr_get_visible),
	KX_PYATTRIBUTE_RO_FUNCTION("collide", KX_PolyProxy, pyattr_get_collide),
	KX_PYATTRIBUTE_RO_FUNCTION("vertices", KX_PolyProxy, pyattr_get_vertices),
	KX_PYATTRIBUTE_NULL	//Sentinel
};

KX_PolyProxy::KX_PolyProxy(KX_MeshProxy *meshProxy, RAS_MeshObject *mesh, const RAS_MeshObject::PolygonInfo& polygon)
	:m_meshProxy(meshProxy),
	m_polygon(polygon),
	m_mesh(mesh)
{
	Py_INCREF(m_meshProxy->GetProxy());
}

KX_PolyProxy::~KX_PolyProxy()
{
	Py_DECREF(m_meshProxy->GetProxy());
}


// stuff for cvalue related things
std::string KX_PolyProxy::GetName()
{
	return "polygone";
}

const RAS_MeshObject::PolygonInfo& KX_PolyProxy::GetPolygon() const
{
	return m_polygon;
}

// stuff for python integration

PyObject *KX_PolyProxy::pyattr_get_material_name(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterialName();
}

PyObject *KX_PolyProxy::pyattr_get_texture_name(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetTextureName();
}

PyObject *KX_PolyProxy::pyattr_get_material(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterial();
}

PyObject *KX_PolyProxy::pyattr_get_material_id(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterialIndex();
}

PyObject *KX_PolyProxy::pyattr_get_v1(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon.indices[0]);
}

PyObject *KX_PolyProxy::pyattr_get_v2(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon.indices[1]);
}

PyObject *KX_PolyProxy::pyattr_get_v3(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon.indices[2]);
}

PyObject *KX_PolyProxy::pyattr_get_v4(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	return PyLong_FromLong(0);
}

PyObject *KX_PolyProxy::pyattr_get_visible(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PyisVisible();
}

PyObject *KX_PolyProxy::pyattr_get_collide(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PyisCollider();
}

static int kx_poly_proxy_get_vertices_size_cb(void *self_v)
{
	return 3;
}

static PyObject *kx_poly_proxy_get_vertices_item_cb(void *self_v, int index)
{
	KX_PolyProxy *self = static_cast<KX_PolyProxy *>(self_v);
	const RAS_MeshObject::PolygonInfo& polygon = self->GetPolygon();
	RAS_IDisplayArray *array = polygon.array;
	KX_VertexProxy *vert = new KX_VertexProxy(array, array->GetVertex(polygon.indices[index]));

	return vert->GetProxy();
}

PyObject *KX_PolyProxy::pyattr_get_vertices(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	return (new CListWrapper(self_v,
		((KX_PolyProxy *)self_v)->GetProxy(),
		nullptr,
		kx_poly_proxy_get_vertices_size_cb,
		kx_poly_proxy_get_vertices_item_cb,
		nullptr,
		nullptr))->NewProxy(true);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialIndex,
"getMaterialIndex() : return the material index of the polygon in the mesh\n")
{
	return PyLong_FromLong(m_polygon.matId);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getNumVertex,
"getNumVertex() : returns the number of vertex of the polygon\n")
{
	return PyLong_FromLong(3);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isVisible,
"isVisible() : returns whether the polygon is visible or not\n")
{
	return PyLong_FromLong(m_polygon.flags & RAS_MeshObject::PolygonInfo::VISIBLE);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isCollider,
"isCollider() : returns whether the polygon is receives collision or not\n")
{
	return PyLong_FromLong(m_polygon.flags & RAS_MeshObject::PolygonInfo::COLLIDER);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialName,
"getMaterialName() : returns the polygon material name, \"\" if no material\n")
{
	return PyUnicode_FromStdString(m_meshProxy->GetMesh()->GetMaterialName(m_polygon.matId));
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getTextureName,
"getTexturelName() : returns the polygon texture name, \"\" if no texture\n")
{
	return PyUnicode_FromStdString(m_meshProxy->GetMesh()->GetTextureName(m_polygon.matId));
}

KX_PYMETHODDEF_DOC(KX_PolyProxy, getVertexIndex,
"getVertexIndex(vertex) : returns the mesh vertex index of a polygon vertex\n"
"vertex: index of the vertex in the polygon: 0->2\n"
"return value can be used to retrieve the vertex details through mesh proxy\n")
{
	int index;
	if (!PyArg_ParseTuple(args,"i:getVertexIndex",&index))
	{
		return nullptr;
	}
	if (index < 0 || index > 3)
	{
		PyErr_SetString(PyExc_AttributeError, "poly.getVertexIndex(int): KX_PolyProxy, expected an index between 0-2");
		return nullptr;
	}
	if (index < 3)
	{
		return PyLong_FromLong(m_polygon.indices[index]);
	}
	return PyLong_FromLong(0);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMesh,
"getMesh() : returns a mesh proxy\n")
{
	return m_meshProxy->GetProxy();
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterial,
"getMaterial() : returns a material\n")
{
	RAS_MeshMaterial *meshmat = m_meshProxy->GetMesh()->GetMeshMaterial(m_polygon.matId);
	RAS_IPolyMaterial *polymat = meshmat->GetBucket()->GetPolyMaterial();
	KX_BlenderMaterial *mat = static_cast<KX_BlenderMaterial *>(polymat);
	return mat->GetProxy();
}

#endif // WITH_PYTHON
