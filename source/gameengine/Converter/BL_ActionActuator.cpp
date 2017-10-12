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

/** \file gameengine/Converter/BL_ActionActuator.cpp
 *  \ingroup bgeconv
 */


#include "SCA_LogicManager.h"
#include "BL_ActionActuator.h"
#include "BL_ArmatureObject.h"
#include "BL_SkinDeformer.h"
#include "BL_ActionManager.h"
#include "KX_GameObject.h"
#include "KX_Globals.h"
#include <string>
#include "MEM_guardedalloc.h"
#include "DNA_nla_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "MT_Matrix4x4.h"

#include "BKE_action.h"
#include "EXP_FloatValue.h"
#include "EXP_PyObjectPlus.h"
#include "KX_PyMath.h"

extern "C" {
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "RNA_access.h"
#include "RNA_define.h"
}

BL_ActionActuator::BL_ActionActuator(SCA_IObject *gameobj,
					const std::string& propname,
					const std::string& framepropname,
					float starttime,
					float endtime,
					struct bAction *action,
					short	playtype,
					short	blend_mode,
					short	blendin,
					short	priority,
					short	layer,
					float	layer_weight,
					short	ipo_flags,
					short	end_reset,
					float	stride) 
	: SCA_IActuator(gameobj, KX_ACT_ACTION),
		
	m_lastpos(0, 0, 0),
	m_blendframe(0),
	m_flag(0),
	m_startframe (starttime),
	m_endframe(endtime) ,
	m_localtime(starttime),
	m_blendin(blendin),
	m_blendstart(0),
	m_stridelength(stride),
	m_layer_weight(layer_weight),
	m_playtype(playtype),
	m_blendmode(blend_mode),
	m_priority(priority),
	m_layer(layer),
	m_ipo_flags(ipo_flags),
	m_action(action),
	m_propname(propname),
	m_framepropname(framepropname)
{
	if (!end_reset)
		m_flag |= ACT_FLAG_CONTINUE;
};

BL_ActionActuator::~BL_ActionActuator()
{
}

void BL_ActionActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();

	m_localtime = m_startframe;	
}

CValue* BL_ActionActuator::GetReplica()
{
	BL_ActionActuator* replica = new BL_ActionActuator(*this);//m_float,GetName());
	replica->ProcessReplica();
	return replica;
}

bool BL_ActionActuator::Update(double curtime)
{
	KX_GameObject *obj = (KX_GameObject*)GetParent();
	short playtype = BL_Action::ACT_MODE_PLAY;
	float start = m_startframe;
	float end = m_endframe;

	// If we don't have an action, we can't do anything
	if (!m_action) {
		return false;
	}

	// Convert our playtype to one that BL_Action likes
	switch (m_playtype) {
		case ACT_ACTION_LOOP_END:
		case ACT_ACTION_LOOP_STOP:
		{
			playtype = BL_Action::ACT_MODE_LOOP;
			break;
		}
		case ACT_ACTION_PINGPONG:
		{
			playtype = BL_Action::ACT_MODE_PING_PONG;
			break;
		}
	}

	const bool useContinue = (m_flag & ACT_FLAG_CONTINUE);

	// Handle events
	const bool negativeEvent = m_negevent;
	const bool positiveEvent = m_posevent;
	RemoveAllEvents();

	if (m_flag & ACT_FLAG_ACTIVE) {
		// "Active" actions need to keep updating their current frame
		m_localtime = obj->GetActionFrame(m_layer);

		// Handle a frame property if it's defined
		if (!m_framepropname.empty()) {
			CValue* oldprop = obj->GetProperty(m_framepropname);
			CValue* newval = new CFloatValue(obj->GetActionFrame(m_layer));
			if (oldprop) {
				oldprop->SetValue(newval);
			}
			else {
				obj->SetProperty(m_framepropname, newval);
			}

			newval->Release();
		}
	}

	// Handle a finished animation
	if ((m_flag & ACT_FLAG_PLAY_END) && (m_flag & ACT_FLAG_ACTIVE) && obj->IsActionDone(m_layer)) {
		m_flag &= ~ACT_FLAG_ACTIVE;
		m_flag &= ~ACT_FLAG_PLAY_END;
		return false;
	}

	// If a different action is playing, we've been overruled and are no longer active
	if (obj->GetCurrentAction(m_layer) != m_action && !obj->IsActionDone(m_layer)) {
		m_flag &= ~ACT_FLAG_ACTIVE;
	}

	if (positiveEvent) {
		switch (m_playtype) {
			case ACT_ACTION_PLAY:
			{
				if (!(m_flag & ACT_FLAG_ACTIVE)) {
					m_localtime = start;
					m_flag |= ACT_FLAG_PLAY_END;
				}
				ATTR_FALLTHROUGH;
			}
			case ACT_ACTION_LOOP_END:
			case ACT_ACTION_LOOP_STOP:
			case ACT_ACTION_PINGPONG:
			{
				if (!(m_flag & ACT_FLAG_ACTIVE) && Play(obj, start, end, playtype)) {
					m_flag |= ACT_FLAG_ACTIVE;
					if (useContinue) {
						obj->SetActionFrame(m_layer, m_localtime);
					}
				}
				break;
			}
			case ACT_ACTION_FROM_PROP:
			{
				CValue* prop = GetParent()->GetProperty(m_propname);
				// If we don't have a property, we can't do anything, so just bail
				if (!prop) {
					return false;
				}

				const float frame = prop->GetNumber();
				if (Play(obj, frame, frame, playtype)) {
					m_flag |= ACT_FLAG_ACTIVE;
				}
				break;
			}
			case ACT_ACTION_FLIPPER:
			{
				if ((!(m_flag & ACT_FLAG_ACTIVE) || m_flag & ACT_FLAG_PLAY_END) && Play(obj, start, end, playtype)) {
					m_flag |= ACT_FLAG_ACTIVE;
					m_flag &= ~ACT_FLAG_PLAY_END;
					if (useContinue) {
						obj->SetActionFrame(m_layer, m_localtime);
					}
				}
				break;
			}
		}
	}
	else if ((m_flag & ACT_FLAG_ACTIVE) && negativeEvent)
	{
		m_localtime = obj->GetActionFrame(m_layer);
		bAction *curr_action = obj->GetCurrentAction(m_layer);
		if (curr_action && curr_action != m_action)
		{
			// Someone changed the action on us, so we wont mess with it
			// Hopefully there wont be too many problems with two actuators using
			// the same action...
			m_flag &= ~ACT_FLAG_ACTIVE;
			return false;
		}

		switch (m_playtype) {
			case ACT_ACTION_FROM_PROP:
			case ACT_ACTION_LOOP_STOP:
				obj->StopAction(m_layer); // Stop the action after getting the frame

				// We're done
				m_flag &= ~ACT_FLAG_ACTIVE;
				return false;
			case ACT_ACTION_LOOP_END:
				// Convert into a play and let it finish
				obj->SetPlayMode(m_layer, BL_Action::ACT_MODE_PLAY);

				m_flag |= ACT_FLAG_PLAY_END;
				break;

			case ACT_ACTION_FLIPPER:
				// Convert into a play action and play back to the beginning
				float temp = end;
				end = start;
				start = curr_action ? obj->GetActionFrame(m_layer) : temp;
				Play(obj, start, end, BL_Action::ACT_MODE_PLAY);
				m_flag |= ACT_FLAG_PLAY_END;
				break;
		}
	}

	return m_flag & ACT_FLAG_ACTIVE;
}

void BL_ActionActuator::DecLink()
{
	SCA_IActuator::DecLink();
	/* In this case no controllers use this action actuator,
	   and it should stop its action. */
	if (m_links == 0) {
		KX_GameObject *obj = (KX_GameObject *)GetParent();
		obj->StopAction(m_layer);
	}
}

bool BL_ActionActuator::Play(KX_GameObject *obj, float start, float end, short mode)
{
	const short blendmode = (m_blendmode == ACT_ACTION_ADD) ? BL_Action::ACT_BLEND_ADD : BL_Action::ACT_BLEND_BLEND;
	return obj->PlayAction(m_action->id.name + 2, start, end, m_layer, m_priority, m_blendin, mode, m_layer_weight, m_ipo_flags, 1.0f, blendmode);
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject BL_ActionActuator::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"BL_ActionActuator",
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
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef BL_ActionActuator::Methods[] = {
	{nullptr,nullptr} //Sentinel
};

EXP_Attribute BL_ActionActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW("frameStart", 0, MAXFRAMEF, BL_ActionActuator, m_startframe),
	KX_PYATTRIBUTE_FLOAT_RW("frameEnd", 0, MAXFRAMEF, BL_ActionActuator, m_endframe),
	KX_PYATTRIBUTE_FLOAT_RW("blendIn", 0, MAXFRAMEF, BL_ActionActuator, m_blendin),
	KX_PYATTRIBUTE_RW_FUNCTION("action", BL_ActionActuator, pyattr_get_action, pyattr_set_action),
	KX_PYATTRIBUTE_SHORT_RW("priority", 0, 100, false, BL_ActionActuator, m_priority),
	KX_PYATTRIBUTE_SHORT_RW("layer", 0, MAX_ACTION_LAYERS-1, true, BL_ActionActuator, m_layer),
	KX_PYATTRIBUTE_FLOAT_RW("layerWeight", 0, 1.0, BL_ActionActuator, m_layer_weight),
	KX_PYATTRIBUTE_RW_FUNCTION("frame", BL_ActionActuator, pyattr_get_frame, pyattr_set_frame),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, MAX_PROP_NAME, false, BL_ActionActuator, m_propname),
	KX_PYATTRIBUTE_STRING_RW("framePropName", 0, MAX_PROP_NAME, false, BL_ActionActuator, m_framepropname),
	KX_PYATTRIBUTE_RW_FUNCTION("useContinue", BL_ActionActuator, pyattr_get_use_continue, pyattr_set_use_continue),
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("blendTime", 0, MAXFRAMEF, BL_ActionActuator, m_blendframe, CheckBlendTime),
	KX_PYATTRIBUTE_SHORT_RW_CHECK("mode",0,100,false,BL_ActionActuator,m_playtype,CheckType),
	KX_PYATTRIBUTE_NULL //Sentinel
};

PyObject *BL_ActionActuator::pyattr_get_action(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	return PyUnicode_FromString(self->GetAction() ? self->GetAction()->id.name+2 : "");
}

int BL_ActionActuator::pyattr_set_action(PyObjectPlus *self_v, const EXP_Attribute *attrdef, PyObject *value)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	
	if (!PyUnicode_Check(value))
	{
		PyErr_SetString(PyExc_ValueError, "actuator.action = val: Action Actuator, expected the string name of the action");
		return PY_SET_ATTR_FAIL;
	}

	bAction *action= nullptr;
	std::string val = _PyUnicode_AsString(value);
	
	if (val != "")
	{
		action= (bAction*)self->GetLogicManager()->GetActionByName(val);
		if (!action)
		{
			PyErr_SetString(PyExc_ValueError, "actuator.action = val: Action Actuator, action not found!");
			return PY_SET_ATTR_FAIL;
		}
	}
	
	self->SetAction(action);
	return PY_SET_ATTR_SUCCESS;

}

PyObject *BL_ActionActuator::pyattr_get_use_continue(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	return PyBool_FromLong(self->m_flag & ACT_FLAG_CONTINUE);
}

int BL_ActionActuator::pyattr_set_use_continue(PyObjectPlus *self_v, const EXP_Attribute *attrdef, PyObject *value)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	
	if (PyObject_IsTrue(value))
		self->m_flag |= ACT_FLAG_CONTINUE;
	else
		self->m_flag &= ~ACT_FLAG_CONTINUE;
	
	return PY_SET_ATTR_SUCCESS;
}

PyObject *BL_ActionActuator::pyattr_get_frame(PyObjectPlus *self_v, const EXP_Attribute *attrdef)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	return PyFloat_FromDouble(((KX_GameObject*)self->m_gameobj)->GetActionFrame(self->m_layer));
}

int BL_ActionActuator::pyattr_set_frame(PyObjectPlus *self_v, const EXP_Attribute *attrdef, PyObject *value)
{
	BL_ActionActuator* self = static_cast<BL_ActionActuator*>(self_v);
	
	((KX_GameObject*)self->m_gameobj)->SetActionFrame(self->m_layer, PyFloat_AsDouble(value));
	
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON
