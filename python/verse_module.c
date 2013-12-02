/*
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2012, Jiri Hnidek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END BSD LICENSE BLOCK *****
 *
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#include <Python.h>
#include <structmember.h>
#include <assert.h>

#if PY_MAJOR_VERSION >= 3
#include <bytesobject.h>
#endif

#include "verse.h"
#include "verse_types.h"


/* Python object storing errors string */
static PyObject *VerseError;


#if PY_MAJOR_VERSION >= 3
/* The module doc string */
PyDoc_STRVAR(verse__doc__,
"Python Verse module that enables communication with Verse server");
#endif

/* Object for session */
typedef struct session_SessionObject {
	PyObject_HEAD
	PyObject		*hostname;
	PyObject		*service;
	/* Hidden members of session object */
	uint8_t			session_id;
	uint8_t			flags;
} session_SessionObject;


/* List of session objects */
static session_SessionObject* session_list[256] = {0,};


/**
 * \brief Declaration of session object members
 */
static struct PyMemberDef Session_members[] = {
	{"hostname", T_OBJECT_EX, offsetof(session_SessionObject, hostname), READONLY,
		"hostname of verse server"},
	{"service", T_OBJECT_EX, offsetof(session_SessionObject, service), READONLY,
		"port name or port number"},
	{NULL, 0, 0, 0, NULL}  /* Sentinel */
};

/**
 * \brief Generic default callback function that print function name,
 * session ID and arguments
 */
static PyObject *_print_callback_arguments(const char *func_name, PyObject *self, PyObject *args)
{
	session_SessionObject *session = (session_SessionObject *)self;
	printf("default %s(session_id: %d, ", &func_name[7], session->session_id);
	printf("args: "); PyObject_Print(args, stdout, Py_PRINT_RAW); printf(")\n");
	Py_RETURN_NONE;
}


/* Layer UnSet */
static PyObject *Session_receive_layer_unset_value(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_unset_value(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_layer_unset_value", "(IHI)",
				node_id, layer_id, item_id);
	return;
}

static PyObject *Session_send_layer_unset_value(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t layer_id;
	uint32_t item_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "layer_id", "item_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHI", kwlist,
			&prio, &node_id, &layer_id, &item_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_unset_value(session->session_id, prio, node_id, layer_id, item_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_unset_value command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Layer Set Value */
static PyObject *Session_receive_layer_set_value(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_set_value(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL) {
		PyObject *tuple_values = NULL;
		PyObject *py_value = NULL;
		int i;

		/* Create tuple for values */
		tuple_values = PyTuple_New(count);
		if(tuple_values == NULL)
			return;

		/* Fill tuple with values from array */
		for (i = 0; i < count; i++) {
			switch(data_type) {
			case VRS_VALUE_TYPE_UINT8:
				py_value = PyLong_FromLong(((uint8_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT16:
				py_value = PyLong_FromLong(((uint16_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT32:
				py_value = PyLong_FromLong(((uint32_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT64:
				py_value = PyLong_FromLong(((uint64_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL16:
				py_value = PyFloat_FromDouble(((uint16_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL32:
				py_value = PyFloat_FromDouble(((float*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL64:
				py_value = PyFloat_FromDouble(((double*)value)[i]);
				break;
			default:
				Py_DECREF(tuple_values);
				return;
			}
			if(py_value == NULL) {
				Py_DECREF(tuple_values);
				return;
			}
			PyTuple_SetItem(tuple_values, i, py_value);
		}
		PyObject_CallMethod((PyObject*)session, "_receive_layer_set_value", "(IHIO)",
				node_id, layer_id, item_id, tuple_values);
	}
	return;
}

static PyObject *Session_send_layer_set_value(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t layer_id;
	uint32_t item_id;
	uint8_t data_type, count;
	PyObject *tuple_values;
	PyObject *py_value;
	Py_ssize_t size;
	long l;
	double d;
	int i, ret;
	void *values = NULL;
	static char *kwlist[] = {"prio", "node_id", "layer_id", "item_id", "data_type", "values", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHIBO", kwlist,
			&prio, &node_id, &layer_id, &item_id, &data_type, &tuple_values)) {
		return NULL;
	}

	/* Check if value is tuple */
	if(tuple_values == NULL || !PyTuple_Check(tuple_values)) {
		PyErr_SetString(VerseError, "Value is not tuple");
		return NULL;
	}

	size = PyTuple_Size(tuple_values);
	if(!(size>0 && size <=4)) {
		PyErr_SetString(VerseError, "Wrong size of tuple");
		return NULL;
	} else {
		count = size;
	}

	/* Convert tuple to array of values */
	switch(data_type) {
	case VRS_VALUE_TYPE_UINT8:
		values = (uint8_t*)malloc(count*sizeof(uint8_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyLong_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT8_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint8_t*)values)[i] = (uint8_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		values = (uint16_t*)malloc(count*sizeof(uint16_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyLong_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT16_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint16_t*)values)[i] = (uint16_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		values = (uint32_t*)malloc(count*sizeof(uint32_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyLong_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT32_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint32_t*)values)[i] = (uint32_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		values = (uint64_t*)malloc(count*sizeof(uint64_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyLong_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				((uint64_t*)values)[i] = (uint64_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		values = (real16*)malloc(count*sizeof(real16));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				/* TODO: add range check */
				((real16*)values)[i] = (real16)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		values = (real32*)malloc(count*sizeof(real32));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				((real32*)values)[i] = (real32)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		values = (real64*)malloc(count*sizeof(real64));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					PyErr_SetString(VerseError, "Wrong type of tuple item");
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				((real64*)values)[i] = (real64)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	default:
		PyErr_SetString(VerseError, "Unsupported value type");
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_set_value(session->session_id, prio, node_id, layer_id, item_id, data_type, count, values);

	/* Free memory allocated for values */
	if(values != NULL) {
		free(values);
		values = NULL;
	}

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_set_value command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Layer UnSubscribe */
static PyObject *Session_receive_layer_unsubscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_unsubscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_layer_unsubscribe", "(IHII)",
				node_id, layer_id, version, crc32);
	return;
}

static PyObject *Session_send_layer_unsubscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t layer_id;
	uint8_t versing;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "layer_id", "versing", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHB", kwlist,
			&prio, &node_id, &layer_id, &versing)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_unsubscribe(session->session_id, prio, node_id, layer_id, versing);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_unsubscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Layer Subscribe */
static PyObject *Session_receive_layer_subscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_subscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_layer_subscribe", "(IHII)",
				node_id, layer_id, version, crc32);
	return;
}

static PyObject *Session_send_layer_subscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t layer_id;
	uint32_t version;
	uint32_t crc32;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "layer_id", "version", "crc32", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHII", kwlist,
			&prio, &node_id, &layer_id, &version, &crc32)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_subscribe(session->session_id, prio, node_id, layer_id, version, crc32);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_subscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Layer Destroy */
static PyObject *Session_receive_layer_destroy(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_layer_destroy", "(IH)",
				node_id, layer_id);
	return;
}

static PyObject *Session_send_layer_destroy(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t layer_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "layer_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIH", kwlist,
			&prio, &node_id, &layer_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_destroy(session->session_id, prio, node_id, layer_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_destroy command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Layer Create */
static PyObject *Session_receive_layer_create(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_layer_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint16_t layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t custom_type)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_layer_create", "(IHHBBH)",
				node_id, parent_layer_id, layer_id, data_type, count, custom_type);
	return;
}

static PyObject *Session_send_layer_create(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t parent_layer_id;
	uint8_t data_type;
	uint8_t count;
	uint16_t custom_type;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "parent_layer_id", "data_type", "count", "custom_type", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHBBH", kwlist,
			&prio, &node_id, &parent_layer_id, &data_type, &count, &custom_type)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_layer_create(session->session_id, prio, node_id, parent_layer_id, data_type, count, custom_type);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send layer_create command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Tag Set Value */
static PyObject *Session_receive_tag_set_values(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_tag_set_value(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value)
{
	session_SessionObject *session = session_list[session_id];

	if(session != NULL) {
		PyObject *tuple_values = NULL;
		PyObject *py_value = NULL;
		int i;

		/* Create tuple for values */
		tuple_values = PyTuple_New(count);
		if(tuple_values == NULL)
			return;

		/* Fill tuple with values from array */
		for (i = 0; i < count; i++) {
			switch(data_type) {
			case VRS_VALUE_TYPE_UINT8:
				py_value = PyLong_FromLong(((uint8_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT16:
				py_value = PyLong_FromLong(((uint16_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT32:
				py_value = PyLong_FromLong(((uint32_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_UINT64:
				py_value = PyLong_FromLong(((uint64_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL16:
				py_value = PyFloat_FromDouble(((uint16_t*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL32:
				py_value = PyFloat_FromDouble(((float*)value)[i]);
				break;
			case VRS_VALUE_TYPE_REAL64:
				py_value = PyFloat_FromDouble(((double*)value)[i]);
				break;
			case VRS_VALUE_TYPE_STRING8:
				if(count==1) {
					py_value = PyUnicode_FromString((char*)value);
				}
				break;
			default:
				Py_DECREF(tuple_values);
				return;
			}
			if(py_value == NULL) {
				Py_DECREF(tuple_values);
				return;
			}
			PyTuple_SetItem(tuple_values, i, py_value);
		}

		PyObject_CallMethod((PyObject*)session, "_receive_tag_set_values", "(IHHO)",
				node_id, taggroup_id, tag_id, tuple_values);
	}
	return;
}

static PyObject *_send_tag_set_tuple(session_SessionObject *session,
		uint8_t prio,
		uint32_t node_id,
		uint16_t taggroup_id,
		uint16_t tag_id,
		uint8_t data_type,
		PyObject *tuple_values)
{
	uint8_t count;
	void *values = NULL;
	PyObject *py_value;
	PyObject *tmp;
	Py_ssize_t size;
	long l;
	double d;
	int i, ret;

	size = PyTuple_Size(tuple_values);
	if(!(size>0 && size <=4)) {
		PyErr_SetString(VerseError, "Wrong size of tuple");
		return NULL;
	} else {
		count = size;
	}

	switch(data_type) {
	case VRS_VALUE_TYPE_UINT8:
		values = (uint8_t*)malloc(count*sizeof(uint8_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
#if PY_MAJOR_VERSION >= 3
				if(!PyLong_Check(py_value)) {
#else
				if(!PyInt_Check(py_value)) {
#endif
					char err_message[256];
					sprintf(err_message, "Tuple item type is not int. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT8_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint8_t*)values)[i] = (uint8_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		values = (uint16_t*)malloc(count*sizeof(uint16_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
#if PY_MAJOR_VERSION >= 3
				if(!PyLong_Check(py_value)) {
#else
				if(!PyInt_Check(py_value)) {
#endif
					char err_message[256];
					sprintf(err_message, "Tuple item type is not int. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT16_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint16_t*)values)[i] = (uint16_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		values = (uint32_t*)malloc(count*sizeof(uint32_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
#if PY_MAJOR_VERSION >= 3
				if(!PyLong_Check(py_value)) {
#else
				if(!PyInt_Check(py_value)) {
#endif
					char err_message[256];
					sprintf(err_message, "Tuple item type is not int. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				if(!(l>=0 && l<=UINT32_MAX)) {
					PyErr_SetString(VerseError, "Long out of data_type range");
					free(values);
					return NULL;
				}
				((uint32_t*)values)[i] = (uint32_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		values = (uint64_t*)malloc(count*sizeof(uint64_t));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
#if PY_MAJOR_VERSION >= 3
				if(!PyLong_Check(py_value)) {
#else
				if(!PyInt_Check(py_value)) {
#endif
					char err_message[256];
					sprintf(err_message, "Tuple item type is not int. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				l = PyLong_AsLong(py_value);
				((uint64_t*)values)[i] = (uint64_t)l;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		values = (real16*)malloc(count*sizeof(real16));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					char err_message[256];
					sprintf(err_message, "Tuple item type is not float. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				/* TODO: add range check */
				((real16*)values)[i] = (real16)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		values = (real32*)malloc(count*sizeof(real32));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					char err_message[256];
					sprintf(err_message, "Tuple item type is not float. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				((real32*)values)[i] = (real32)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		values = (real64*)malloc(count*sizeof(real64));
		for(i=0; i<count; i++) {
			py_value = PyTuple_GetItem(tuple_values, i);
			if(py_value != NULL) {
				if(!PyFloat_Check(py_value)) {
					char err_message[256];
					sprintf(err_message, "Tuple item type is not float. Item type is: %s",
							py_value->ob_type->tp_name);
					PyErr_SetString(VerseError, err_message);
					free(values);
					return NULL;
				}
				d = PyFloat_AsDouble(py_value);
				((real64*)values)[i] = (real64)d;
			} else {
				free(values);
				return NULL;
			}
		}
		break;
	case VRS_VALUE_TYPE_STRING8:
		/* Only one string value could be send to verse server */
		if(count != 1) {
			PyErr_SetString(VerseError, "Wrong size of tuple");
			return NULL;
		}
		py_value = PyTuple_GetItem(tuple_values, 0);
		if(py_value != NULL) {
#if PY_MAJOR_VERSION >= 3
			if(!PyUnicode_Check(py_value)) {
#else
			if(!PyString_Check(py_value) && !PyUnicode_Check(py_value)) {
#endif
				char err_message[256];
				sprintf(err_message, "Tuple item type is not str. Item type is: %s",
						py_value->ob_type->tp_name);
				PyErr_SetString(VerseError, err_message);
				return NULL;
			}
		}
#if PY_MAJOR_VERSION >= 3
		tmp = PyUnicode_AsEncodedString(py_value, "utf-8", "Error: encoding string");
		values = PyBytes_AsString(tmp);
#else
		(void)tmp;
		values = PyString_AsString(py_value);
#endif
		break;
	default:
		PyErr_SetString(VerseError, "Unsupported value type");
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_tag_set_value(session->session_id, prio, node_id, taggroup_id, tag_id, data_type, count, values);

	/* Free memory allocated for values */
	if(values != NULL && data_type!=VRS_VALUE_TYPE_STRING8) {
		free(values);
		values = NULL;
	}

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send tag_set_value command");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *Session_send_tag_set_values(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	uint16_t tag_id;
	uint8_t data_type;
	PyObject *values;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", "tag_id", "data_type", "values", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHHBO", kwlist,
			&prio, &node_id, &taggroup_id, &tag_id, &data_type, &values)) {
		return NULL;
	}

	/* Check if value is tuple */
	if(values != NULL && PyTuple_Check(values)) {
		return _send_tag_set_tuple(session, prio, node_id, taggroup_id, tag_id, data_type, values);
	} else {
		PyErr_SetString(VerseError, "Value is not tuple");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Tag Destroy */
static PyObject *Session_receive_tag_destroy(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}


static void cb_c_receive_tag_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_tag_destroy", "(IHH)",
				node_id, taggroup_id, tag_id);
	return;
}

static PyObject *Session_send_tag_destroy(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	uint16_t tag_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", "tag_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHH", kwlist,
			&prio, &node_id, &taggroup_id, &tag_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_tag_destroy(session->session_id, prio, node_id, taggroup_id, tag_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send tag_destroy command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Tag Create */
static PyObject *Session_receive_tag_create(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_tag_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t custom_type)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_tag_create", "(IHHBBH)",
				node_id, taggroup_id, tag_id, data_type, count, custom_type);
	return;
}

static PyObject *Session_send_tag_create(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	uint8_t data_type;
	uint8_t count;
	uint16_t custom_type;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", "data_type", "count", "custom_type", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHBBH", kwlist,
			&prio, &node_id, &taggroup_id, &data_type, &count, &custom_type)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_tag_create(session->session_id, prio, node_id, taggroup_id, data_type, count, custom_type);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send tag_create command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* TagGroup UnSubscribe */
static PyObject *Session_receive_taggroup_unsubscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_taggroup_unsubscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_taggroup_unsubscribe", "(IHII)",
				node_id, taggroup_id, version, crc32);
	return;
}

static PyObject *Session_send_taggroup_unsubscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	uint8_t versing;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", "versing", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHB", kwlist,
			&prio, &node_id, &taggroup_id, &versing)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_taggroup_unsubscribe(session->session_id, prio, node_id, taggroup_id, versing);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send taggroup_unsubscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* TagGroup Subscribe */
static PyObject *Session_receive_taggroup_subscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_taggroup_subscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_taggroup_subscribe", "(IHII)",
				node_id, taggroup_id, version, crc32);
	return;
}

static PyObject *Session_send_taggroup_subscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	uint32_t version;
	uint32_t crc32;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", "version", "crc32", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHII", kwlist,
			&prio, &node_id, &taggroup_id, &version, &crc32)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_taggroup_subscribe(session->session_id, prio, node_id, taggroup_id, version, crc32);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send taggroup_subscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* TagGroup Destroy */
static PyObject *Session_receive_taggroup_destroy(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_taggroup_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_taggroup_destroy", "(IH)",
				node_id, taggroup_id);
	return;
}

static PyObject *Session_send_taggroup_destroy(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t taggroup_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "taggroup_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIH", kwlist,
			&prio, &node_id, &taggroup_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_taggroup_destroy(session->session_id, prio, node_id, taggroup_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send taggroup_destroy command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* TagGroup Create */
static PyObject *Session_receive_taggroup_create(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_taggroup_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t custom_type)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_taggroup_create", "(IHH)",
				node_id, taggroup_id, custom_type);
	return;
}

static PyObject *Session_send_taggroup_create(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t custom_type;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "custom_type", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIH", kwlist,
			&prio, &node_id, &custom_type)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_taggroup_create(session->session_id, prio, node_id, custom_type);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send taggroup_create command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node UnLock */
static PyObject *Session_receive_node_unlock(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_unlock(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t avatar_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_unlock", "(II)",
				node_id, avatar_id);
	return;
}

static PyObject *Session_send_node_unlock(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BI", kwlist,
			&prio, &node_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_unlock(session->session_id, prio, node_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_unlock command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Lock */
static PyObject *Session_receive_node_lock(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_lock(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t avatar_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_lock", "(II)",
				node_id, avatar_id);
	return;
}

static PyObject *Session_send_node_lock(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BI", kwlist,
			&prio, &node_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_lock(session->session_id, prio, node_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_lock command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Owner */
static PyObject *Session_receive_node_owner(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_owner(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t user_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_owner", "(IH)",
				node_id, user_id);
	return;
}

static PyObject *Session_send_node_owner(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t user_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "user_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIH", kwlist,
			&prio, &node_id, &user_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_owner(session->session_id, prio, node_id, user_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_owner command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Perm */
static PyObject *Session_receive_node_perm(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_perm(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t user_id,
		const uint8_t perm)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_perm", "(IHB)",
				node_id, user_id, perm);
	return;
}

static PyObject *Session_send_node_perm(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint16_t user_id;
	uint8_t perm;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "user_id", "perm", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIHB", kwlist,
			&prio, &node_id, &user_id, &perm)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_perm(session->session_id, prio, node_id, user_id, perm);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_perm command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Link */
static PyObject *Session_receive_node_link(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_link(const uint8_t session_id,
		const uint32_t parent_node_id,
		const uint32_t child_node_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_link", "(II)",
				parent_node_id, child_node_id);
	return;
}

static PyObject *Session_send_node_link(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t parent_node_id;
	uint32_t child_node_id;
	int ret;
	static char *kwlist[] = {"prio", "parent_node_id", "child_node_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BII", kwlist,
			&prio, &parent_node_id, &child_node_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_link(session->session_id, prio, parent_node_id, child_node_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_link command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Priority */
static PyObject *Session_send_node_priority(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint8_t priority;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "priority", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIB", kwlist,
			&prio, &node_id, &priority)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_prio(session->session_id, prio, node_id, priority);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_prio command");
		return NULL;
	}

	Py_RETURN_NONE;
}

/* Node UnSubscribe */
static PyObject *Session_receive_node_unsubscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_unsubscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_unsubscribe", "(III)",
				node_id, version, crc32);
	return;
}

static PyObject *Session_send_node_unsubscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint8_t versioning;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "versioning", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIB", kwlist,
			&prio, &node_id, &versioning)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_unsubscribe(session->session_id, prio, node_id, versioning);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_unsubscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Subscribe */
static PyObject *Session_receive_node_subscribe(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_subscribe(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_subscribe", "(III)",
				node_id, version, crc32);
	return;
}

static PyObject *Session_send_node_subscribe(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	uint32_t version;
	uint32_t crc32;
	int ret;
	static char *kwlist[] = {"prio", "node_id", "version", "crc32", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BIII", kwlist,
			&prio, &node_id, &version, &crc32)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_subscribe(session->session_id, prio, node_id, version, crc32);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_subscribe command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Destroy */
static PyObject *Session_receive_node_destroy(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_destroy(const uint8_t session_id,
		const uint32_t node_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_destroy", "(I)",
				node_id);
	return;
}

static PyObject *Session_send_node_destroy(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint32_t node_id;
	int ret;
	static char *kwlist[] = {"prio", "node_id", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BI", kwlist,
			&prio, &node_id)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_destroy(session->session_id, prio, node_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_destroy command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* Node Create */
static PyObject *Session_receive_node_create(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

static void cb_c_receive_node_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t parent_id,
		const uint16_t user_id,
		const uint16_t custom_type)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_node_create", "(IIHH)",
				node_id, parent_id, user_id, custom_type);
	return;
}

static PyObject *Session_send_node_create(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	uint16_t custom_type;
	int ret;
	static char *kwlist[] = {"prio", "custom_type", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|BH", kwlist,
			&prio, &custom_type)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_node_create(session->session_id, prio, custom_type);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send node_create command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/* FPS */
static PyObject *Session_send_fps(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	uint8_t prio;
	float fps;
	int ret;
	static char *kwlist[] = {"prio", "fps", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|Bf", kwlist,
			&prio, &fps)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_fps(session->session_id, prio, fps);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send fps command");
		return NULL;
	}

	Py_RETURN_NONE;
}


/**
 * \brief Default Python callback function for connect_accept
 */
static PyObject *Session_receive_connect_accept(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

/**
 * \brief C callback function for connect accept that call Python callback
 * function
 */
static void cb_c_receive_connect_accept(const uint8_t session_id,
		const uint16_t user_id,
		const uint32_t avatar_id)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_connect_accept", "(HI)", user_id, avatar_id);
	return;
}


/**
 * \brief Default Python callback function for connect_terminate
 */
static PyObject *Session_receive_connect_terminate(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

/* Connect Terminate */
static void cb_c_receive_connect_terminate(const uint8_t session_id,
		const uint8_t error_num)
{
	session_SessionObject *session = session_list[session_id];
	if(session != NULL)
		PyObject_CallMethod((PyObject*)session, "_receive_connect_terminate", "(B)", error_num);
	return;
}

static PyObject *Session_send_connect_terminate(PyObject *self)
{
	session_SessionObject *session = (session_SessionObject *)self;
	int ret;

	/* Call C API function */
	ret = vrs_send_connect_terminate(session->session_id);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send connect terminate command");
		return NULL;
	}
	Py_RETURN_NONE;
}

/**
 * \brief Default Python callback function for user_authenticate
 */
static PyObject *Session_receive_user_authenticate(PyObject *self, PyObject *args)
{
	return _print_callback_arguments(__FUNCTION__, self, args);
}

/* User Authenticate callback function */
static void cb_c_receive_user_authenticate(const uint8_t session_id,
		const char *username,
		const uint8_t auth_meth_count,
		const uint8_t *methods)
{
	session_SessionObject *session = session_list[session_id];
	PyObject *tuple_methods;
	PyObject *value;
	int i;

	/* Create tuple for methods */
	tuple_methods = PyTuple_New(auth_meth_count);
	if(tuple_methods == NULL)
		return;

	/* Fill tuple of methods with values from array methods */
	for (i = 0; i < auth_meth_count; i++) {
		value = PyLong_FromLong(methods[i]);
		if (!value) {
			Py_DECREF(tuple_methods);
			return;
		}
		PyTuple_SetItem(tuple_methods, i, value);
	}

	if(session != NULL) {
		if(username==NULL) {
			PyObject_CallMethod((PyObject*)session, "_receive_user_authenticate", "(sO)", "", tuple_methods);
		} else {
			PyObject_CallMethod((PyObject*)session, "_receive_user_authenticate", "(sO)", username, tuple_methods);
		}
	}

	return;
}


/**
 * \brief This method send user authenticate command to the verse server
 */
static PyObject *Session_send_user_authenticate(PyObject *self, PyObject *args, PyObject *kwds)
{
	session_SessionObject *session = (session_SessionObject *)self;
	char *username;
	char *data;
	uint8_t auth_type;
	int ret;

	static char *kwlist[] = {"username", "auth_type", "data", NULL};

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|sBs", kwlist,
			&username, &auth_type, &data)) {
		return NULL;
	}

	/* Call C API function */
	ret = vrs_send_user_authenticate(session->session_id, username, auth_type, data);

	/* Check if calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to send user authenticate command");
		return NULL;
	}

	Py_RETURN_NONE;
}

/**
 * \brief This method call register callback function, when appropriate command
 * is received from server
 */
static PyObject *Session_callback_update(PyObject *self)
{
	session_SessionObject *session = (session_SessionObject *)self;
	int ret;

	ret = vrs_callback_update(session->session_id);
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Session is not active");
		return NULL;
	}

	Py_RETURN_NONE;
}

/**
 * \brief This function decreases references on session members
 */
static void Session_dealloc(session_SessionObject* self)
{
	/* Remove session from list of sessions */
	session_list[self->session_id] = NULL;

	Py_XDECREF(self->hostname);
	Py_XDECREF(self->service);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

/**
 * \brief This function is called, when new session object is created
 */
static PyObject *Session_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	session_SessionObject *self;

	if(1) {
		printf("Session_new(): type: %p, args: %p, kwds: %p\n",
			(void*)type, (void*)args, (void*)kwds);
	}

	self = (session_SessionObject*)type->tp_alloc(type, 0);
	if (self != NULL) {

		self->hostname = PyUnicode_FromString("");
		if(self->hostname == NULL)
		{
			Py_DECREF(self);
			return NULL;
		}

		self->service = PyUnicode_FromString("");
		if(self->service == NULL)
		{
			Py_DECREF(self);
			return NULL;
		}

		self->session_id = 0;
	}

	return (PyObject*)self;
}

/**
 * \brief This function is called, when object is initialized using __init__
 */
static int Session_init(session_SessionObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *_hostname=NULL, *_service=NULL, *tmp1, *tmp2;
	char *hostname, *service;
	uint8_t flags, session_id;
	int ret;

	static char *kwlist[] = {"hostname", "service", "flags", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "|OOB", kwlist,
			&_hostname, &_service, &flags))
	{
		return -1;
	}

	/* Register basic callback functions */
	vrs_register_receive_connect_accept(cb_c_receive_connect_accept);
	vrs_register_receive_connect_terminate(cb_c_receive_connect_terminate);
	vrs_register_receive_user_authenticate(cb_c_receive_user_authenticate);

	/* Register node callback functions */
	vrs_register_receive_node_create(cb_c_receive_node_create);
	vrs_register_receive_node_destroy(cb_c_receive_node_destroy);
	vrs_register_receive_node_subscribe(cb_c_receive_node_subscribe);
	vrs_register_receive_node_unsubscribe(cb_c_receive_node_unsubscribe);
	vrs_register_receive_node_link(cb_c_receive_node_link);
	vrs_register_receive_node_perm(cb_c_receive_node_perm);
	vrs_register_receive_node_owner(cb_c_receive_node_owner);
	vrs_register_receive_node_lock(cb_c_receive_node_lock);
	vrs_register_receive_node_unlock(cb_c_receive_node_unlock);

	/* Register taggroup callback functions*/
	vrs_register_receive_taggroup_create(cb_c_receive_taggroup_create);
	vrs_register_receive_taggroup_destroy(cb_c_receive_taggroup_destroy);
	vrs_register_receive_taggroup_subscribe(cb_c_receive_taggroup_subscribe);
	vrs_register_receive_taggroup_unsubscribe(cb_c_receive_taggroup_unsubscribe);

	/* Register tag callback functions */
	vrs_register_receive_tag_create(cb_c_receive_tag_create);
	vrs_register_receive_tag_destroy(cb_c_receive_tag_destroy);
	vrs_register_receive_tag_set_value(cb_c_receive_tag_set_value);

	/* Register layer callback functions */
	vrs_register_receive_layer_create(cb_c_receive_layer_create);
	vrs_register_receive_layer_destroy(cb_c_receive_layer_destroy);
	vrs_register_receive_layer_subscribe(cb_c_receive_layer_subscribe);
	vrs_register_receive_layer_unsubscribe(cb_c_receive_layer_unsubscribe);
	vrs_register_receive_layer_set_value(cb_c_receive_layer_set_value);
	vrs_register_receive_layer_unset_value(cb_c_receive_layer_unset_value);

#if PY_MAJOR_VERSION >= 3
	tmp1 = PyUnicode_AsEncodedString(_hostname, "utf-8", "Error: encoding string");
	hostname = PyBytes_AsString(tmp1);
	tmp2 = PyUnicode_AsEncodedString(_service, "utf-8", "Error: encoding string");
	service = PyBytes_AsString(tmp2);
#else
	hostname = PyString_AsString(_hostname);
	service = PyString_AsString(_service);
#endif


	/* Send user connect request to the server */
	if(hostname != NULL && service != NULL) {
		ret = vrs_send_connect_request(hostname, service, flags, &session_id);
#if PY_MAJOR_VERSION >= 3
		Py_XDECREF(tmp1);
		Py_XDECREF(tmp2);
#endif

		if(ret != VRS_SUCCESS) {
			PyErr_SetString(VerseError, "Unable to send connect request");
			return -1;
		}
	} else {
		return -1;
	}

	/* Add this session to the list of session */
	session_list[session_id] = self;
	self->session_id = session_id;

	if(_hostname != NULL) {
		tmp1 = self->hostname;
		Py_INCREF(_hostname);
		self->hostname = _hostname;
		Py_XDECREF(tmp1);
	}

	if(_service != NULL) {
		tmp2 = self->service;
		Py_INCREF(_service);
		self->service = _service;
		Py_XDECREF(tmp2);
	}

	return 0;
}

static PyMethodDef Session_methods[] = {
		/* Session commands */
		{"_receive_connect_accept",
				(PyCFunction)Session_receive_connect_accept,
				METH_VARARGS,
				"_receive_connect_accept(self, user_id, avatar_id) -> None\n\n"
				"Callback function that is called, when connection with server is established\n"
				"user_id:	int ID of user node that represent connected user\n"
				"avatar_id:	int ID of avatar node that represent connected client"
		},
		{"_receive_connect_terminate",
				(PyCFunction)Session_receive_connect_terminate,
				METH_VARARGS,
				"_receive_connect_terminate(self, error) -> None\n\n"
				"Callback function that is called, when connection with server is terminated\n"
				"error:	int code of reason for termination of connection by server"
		},
		{"send_connect_terminate",
				(PyCFunction)Session_send_connect_terminate,
				METH_NOARGS,
				"send_connect_terminate(self, error) -> None\n\n"
				"Send connect terminate command to the server\n"
				"error:	int code of reason for termination of connection by client"
		},
		{"_receive_user_authenticate",
				(PyCFunction)Session_receive_user_authenticate,
				METH_VARARGS,
				"_receive_user_authenticate(self, username, methods) -> None\n\n"
				"Callback function that is called, when server requests user authentication\n"
				"username:	string of client's username\n"
				"methods:	tuple of supported authentication methods"
		},
		{"send_user_authenticate",
				(PyCFunction)Session_send_user_authenticate,
				METH_VARARGS | METH_KEYWORDS,
				"send_user_authenticate(self, username, method, data) -> None\n\n"
				"Send user authenticate command to the server\n"
				"username:	string of client's username\n"
				"method:	int code of authentication method\n"
				"data:		authentication data (usually password)"
		},
		{"callback_update",
				(PyCFunction)Session_callback_update,
				METH_NOARGS,
				"callback_update(self) -> None\n\n"
				"Call callback functions for received commands"
		},
		{"send_fps",
				(PyCFunction)Session_send_fps,
				METH_VARARGS | METH_KEYWORDS,
				"send_fps(self, fps) -> None\n\n"
				"Send FPS used by this client to the server\n"
				"fps:	double value of Frames per Second"
		},
		/* Node commands */
		{"send_node_create",
				(PyCFunction)Session_send_node_create,
				METH_VARARGS | METH_KEYWORDS,
				"Send node create command to the server"
		},
		{"_receive_node_create",
				(PyCFunction)Session_receive_node_create,
				METH_VARARGS,
				"Callback function for node create command received from the server"
		},
		{"send_node_destroy",
				(PyCFunction)Session_send_node_destroy,
				METH_VARARGS | METH_KEYWORDS,
				"Send node destroy command to the server"
		},
		{"_receive_node_destroy",
				(PyCFunction)Session_receive_node_destroy,
				METH_VARARGS,
				"Callback function for node destroy command received from the server"
		},
		{"send_node_subscribe",
				(PyCFunction)Session_send_node_subscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send node subscribe command to the server"
		},
		{"_receive_node_subscribe",
				(PyCFunction)Session_receive_node_subscribe,
				METH_VARARGS,
				"Callback function for node subscribe command received from the server"
		},
		{"send_node_unsubscribe",
				(PyCFunction)Session_send_node_unsubscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send node unsubscribe command to the server"
		},
		{"_receive_node_unsubscribe",
				(PyCFunction)Session_receive_node_unsubscribe,
				METH_VARARGS,
				"Callback function for node unsubscribe command received from server"
		},
		{"send_node_priority",
				(PyCFunction)Session_send_node_priority,
				METH_VARARGS | METH_KEYWORDS,
				"Send node priority command to the server"
		},
		{"send_node_link",
				(PyCFunction)Session_send_node_link,
				METH_VARARGS | METH_KEYWORDS,
				"Send node link command to the server"
		},
		{"_receive_node_link",
				(PyCFunction)Session_receive_node_link,
				METH_VARARGS | METH_KEYWORDS,
				"Callback function for node link command received from server"
		},
		{"send_node_perm",
				(PyCFunction)Session_send_node_perm,
				METH_VARARGS | METH_KEYWORDS,
				"Send node perm command to the server"
		},
		{"_receive_node_perm",
				(PyCFunction)Session_receive_node_perm,
				METH_VARARGS,
				"Callback function for node perm command received from server"
		},
		{"send_node_owner",
				(PyCFunction)Session_send_node_owner,
				METH_VARARGS | METH_KEYWORDS,
				"Send node owner command to the server"
		},
		{"_receive_node_owner",
				(PyCFunction)Session_receive_node_owner,
				METH_VARARGS,
				"Callback function for node owner command received from server"
		},
		{"send_node_lock",
				(PyCFunction)Session_send_node_lock,
				METH_VARARGS | METH_KEYWORDS,
				"Send node lock command to the server"
		},
		{"_receive_node_lock",
				(PyCFunction)Session_receive_node_lock,
				METH_VARARGS,
				"Callback function for node lock command received from server"
		},
		{"send_node_unlock",
				(PyCFunction)Session_send_node_unlock,
				METH_VARARGS | METH_KEYWORDS,
				"Send node unlock command to the server"
		},
		{"_receive_node_unlock",
				(PyCFunction)Session_receive_node_unlock,
				METH_VARARGS,
				"Callback function for node unlock command received from server"
		},
		/* TagGroup commands */
		{"send_taggroup_create",
				(PyCFunction)Session_send_taggroup_create,
				METH_VARARGS | METH_KEYWORDS,
				"Send taggroup create command to the server"
		},
		{"_receive_taggroup_create",
				(PyCFunction)Session_receive_taggroup_create,
				METH_VARARGS,
				"Callback function for taggroup create command received from server"
		},
		{"send_taggroup_destroy",
				(PyCFunction)Session_send_taggroup_destroy,
				METH_VARARGS | METH_KEYWORDS,
				"Send taggroup destroy command to the server"
		},
		{"_receive_taggroup_destroy",
				(PyCFunction)Session_receive_taggroup_destroy,
				METH_VARARGS,
				"Callback function for taggroup destroy command received from server"
		},
		{"send_taggroup_subscribe",
				(PyCFunction)Session_send_taggroup_subscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send taggroup subscribe command to the server"
		},
		{"_receive_taggroup_subscribe",
				(PyCFunction)Session_receive_taggroup_subscribe,
				METH_VARARGS,
				"Callback function for taggroup subscribe command received from server"
		},
		{"send_taggroup_unsubscribe",
				(PyCFunction)Session_send_taggroup_unsubscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send taggroup unsubscribe command to the server"
		},
		{"_receive_taggroup_unsubscribe",
				(PyCFunction)Session_receive_taggroup_unsubscribe,
				METH_VARARGS,
				"Callback function for taggroup subscribe command received from server"
		},
		/* Tag commands */
		{"send_tag_create",
				(PyCFunction)Session_send_tag_create,
				METH_VARARGS | METH_KEYWORDS,
				"Send tag create command to the server"
		},
		{"_receive_tag_create",
				(PyCFunction)Session_receive_tag_create,
				METH_VARARGS,
				"Callback function for tag create command received from server"
		},
		{"send_tag_destroy",
				(PyCFunction)Session_send_tag_destroy,
				METH_VARARGS | METH_KEYWORDS,
				"Send tag destroy command to the server"
		},
		{"_receive_tag_destroy",
				(PyCFunction)Session_receive_tag_destroy,
				METH_VARARGS,
				"Callback function for tag destroy command received from server"
		},
		{"send_tag_set_values",
				(PyCFunction)Session_send_tag_set_values,
				METH_VARARGS | METH_KEYWORDS,
				"Send tag set values command to the server"
		},
		{"_receive_tag_set_values",
				(PyCFunction)Session_receive_tag_set_values,
				METH_VARARGS,
				"Callback function for tag set values command received from server"
		},
		/* Layer commands */
		{"send_layer_create",
				(PyCFunction)Session_send_layer_create,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer create command to the server"
		},
		{"_receive_layer_create",
				(PyCFunction)Session_receive_layer_create,
				METH_VARARGS,
				"Callback function for layer create command received from server"
		},
		{"send_layer_destroy",
				(PyCFunction)Session_send_layer_destroy,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer destroy command to the server"
		},
		{"_receive_layer_destroy",
				(PyCFunction)Session_receive_layer_destroy,
				METH_VARARGS,
				"Callback function for layer destroy command received from server"
		},
		{"send_layer_subscribe",
				(PyCFunction)Session_send_layer_subscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer subscribe command to the server"
		},
		{"_receive_layer_subscribe",
				(PyCFunction)Session_receive_layer_subscribe,
				METH_VARARGS,
				"Callback function for layer subscribe command received from server"
		},
		{"send_layer_unsubscribe",
				(PyCFunction)Session_send_layer_unsubscribe,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer unsubscribe command to the server"
		},
		{"_receive_layer_unsubscribe",
				(PyCFunction)Session_receive_layer_unsubscribe,
				METH_VARARGS,
				"Callback function for layer unsubscribe command received from server"
		},
		{"send_layer_set_value",
				(PyCFunction)Session_send_layer_set_value,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer set value command to the server"
		},
		{"_receive_layer_set_value",
				(PyCFunction)Session_receive_layer_set_value,
				METH_VARARGS,
				"Callback function for layer set value command received from server"
		},
		{"send_layer_unset_value",
				(PyCFunction)Session_send_layer_unset_value,
				METH_VARARGS | METH_KEYWORDS,
				"Send layer unset value command to the server"
		},
		{"_receive_layer_unset_value",
				(PyCFunction)Session_receive_layer_unset_value,
				METH_VARARGS,
				"Callback function for layer unset value command received from server"
		},
		{NULL, NULL, 0, NULL}  /* Sentinel */
};

/* Object type for session */
static PyTypeObject session_SessionType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"session.Session",				/* tp_name */
	sizeof(session_SessionObject),	/* tp_basicsize */
	0,								/* tp_itemsize */
	(destructor)Session_dealloc,	/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	0,								/* tp_setattr */
	0,								/* tp_reserved */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash  */
	0,								/* tp_call */
	0,								/* tp_str */
	0,								/* tp_getattro */
	0,								/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_BASETYPE,		/* tp_flags */
	"Session object",				/* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	Session_methods,				/* tp_methods */
	Session_members,				/* tp_members */
	0,								/* tp_getset */
	0,								/* tp_base */
	0,								/* tp_dict */
	0,								/* tp_descr_get */
	0,								/* tp_descr_set */
	0,								/* tp_dictoffset */
	(initproc)Session_init,			/* tp_init */
	0,								/* tp_alloc */
	Session_new,					/* tp_new */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};



/* The function doc string for callback_update */
PyDoc_STRVAR(set_debug_level__doc__,
		"set_debug_level(level) -> None\n\n"
		"This method set debug level of this client");

/* This function set debug level of python client */
static PyObject *verse_set_debug_level(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *result = NULL;
	const uint8_t debug_level;
	int ret;
	static char *kwlist[] = {"level", NULL};

	(void)self;

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|B", kwlist,
			&debug_level)) {
		return NULL;
	}

	ret = vrs_set_debug_level(debug_level);

	/* Check if command was calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to set debug level");
		return NULL;
	}

	Py_INCREF(Py_None);
	result = Py_None;

	return result;
}


/* The function doc string for callback_update */
PyDoc_STRVAR(set_client_info__doc__,
		"set_client_info(name, version) -> None\n\n"
		"This method set name and version of client");

/* This function set debug level of python client */
static PyObject *verse_set_client_info(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *result = NULL;
	char *name, *version;
	int ret;
	static char *kwlist[] = {"name", "version", NULL};

	(void)self;

	/* Parse arguments */
	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|ss", kwlist,
			&name, &version)) {
		return NULL;
	}

	ret = vrs_set_client_info(name, version);

	/* Check if command was calling function was successful */
	if(ret != VRS_SUCCESS) {
		PyErr_SetString(VerseError, "Unable to set client info");
		return NULL;
	}

	Py_INCREF(Py_None);
	result = Py_None;

	return result;
}

/* Table of module methods */
static PyMethodDef VerseMethods[] = {
	{"set_debug_level",
			(PyCFunction)verse_set_debug_level,
			METH_VARARGS | METH_KEYWORDS,
			set_debug_level__doc__
	},
	{"set_client_info",
			(PyCFunction)verse_set_client_info,
			METH_VARARGS | METH_KEYWORDS,
			set_client_info__doc__
	},
	{NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3

/* Module definition */
static struct PyModuleDef verse_module = {
	PyModuleDef_HEAD_INIT,
	"verse",		/* Name of module */
	verse__doc__,	/* Module documentation */
	-1,				/* Not needed */
	VerseMethods,	/* Array of methods */
	NULL,			/* Not needed */
	NULL,			/* Not needed */
	NULL,			/* Not needed */
	NULL			/* Not needed */
};

#endif

/* Initialization of module */
#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit_verse(void)
#else
void initverse(void)
#endif
{
	PyObject *module;

	if (PyType_Ready(&session_SessionType) < 0)
#if PY_MAJOR_VERSION >= 3
		return NULL;
#else
		return;
#endif

#if PY_MAJOR_VERSION >= 3
	module = PyModule_Create(&verse_module);
#else
	module = Py_InitModule("verse", VerseMethods);
#endif

	if(module == NULL) {
#if PY_MAJOR_VERSION >= 3
		return NULL;
#else
		return;
#endif
	}

	/* Add error object to verse module */
	VerseError = PyErr_NewException("verse.VerseError", NULL, NULL);
	Py_INCREF(VerseError);
	PyModule_AddObject(module, "VerseError", VerseError);

	/* Add Session object to the module */
    Py_INCREF(&session_SessionType);
    PyModule_AddObject(module, "Session", (PyObject *)&session_SessionType);

	/* Add constants to verse module */
	PyModule_AddIntConstant(module, "VERSION", VRS_VERSION);

	/* Timeout in seconds */
	PyModule_AddIntConstant(module, "TIMEOUT", VRS_TIMEOUT);

	/* Debug levels */
	PyModule_AddIntConstant(module, "PRINT_NONE", VRS_PRINT_NONE);
	PyModule_AddIntConstant(module, "PRINT_INFO", VRS_PRINT_INFO);
	PyModule_AddIntConstant(module, "PRINT_ERROR", VRS_PRINT_ERROR);
	PyModule_AddIntConstant(module, "PRINT_WARNING", VRS_PRINT_WARNING);
	PyModule_AddIntConstant(module, "PRINT_DEBUG_MSG", VRS_PRINT_DEBUG_MSG);

	/* Flags used in connect request (verse.Session) */
	PyModule_AddIntConstant(module, "DGRAM_SEC_NONE", VRS_SEC_DATA_NONE);
	PyModule_AddIntConstant(module, "DGRAM_SEC_DTLS", VRS_SEC_DATA_TLS);
	PyModule_AddIntConstant(module, "TP_UDP", VRS_TP_UDP);
	PyModule_AddIntConstant(module, "TP_TCP", VRS_TP_TCP);
	PyModule_AddIntConstant(module, "CMD_CMPR_NONE", VRS_CMD_CMPR_NONE);
	PyModule_AddIntConstant(module, "CMD_CMPR_ADDR_SHARE", VRS_CMD_CMPR_ADDR_SHARE);

	/* Error constant used, when connection with server is closed */
	PyModule_AddIntConstant(module, "CONN_TERM_HOST_UNKNOWN", VRS_CONN_TERM_HOST_UNKNOWN);
	PyModule_AddIntConstant(module, "CONN_TERM_HOST_DOWN", VRS_CONN_TERM_HOST_DOWN);
	PyModule_AddIntConstant(module, "CONN_TERM_SERVER_DOWN", VRS_CONN_TERM_SERVER_DOWN);
	PyModule_AddIntConstant(module, "CONN_TERM_AUTH_FAILED", VRS_CONN_TERM_AUTH_FAILED);
	PyModule_AddIntConstant(module, "CONN_TERM_TIMEOUT", VRS_CONN_TERM_TIMEOUT);
	PyModule_AddIntConstant(module, "CONN_TERM_ERROR", VRS_CONN_TERM_ERROR);
	PyModule_AddIntConstant(module, "CONN_TERM_CLIENT", VRS_CONN_TERM_CLIENT);
	PyModule_AddIntConstant(module, "CONN_TERM_SERVER", VRS_CONN_TERM_SERVER);

	/* Default priority of nodes */
	PyModule_AddIntConstant(module, "DEFAULT_PRIORITY",	VRS_DEFAULT_PRIORITY);

	/* Supported authentication types */
	PyModule_AddIntConstant(module, "UA_METHOD_NONE", VRS_UA_METHOD_NONE);
	PyModule_AddIntConstant(module, "UA_METHOD_PASSWORD", VRS_UA_METHOD_PASSWORD);

	/* Types of values used in tags and layers */
	PyModule_AddIntConstant(module, "VALUE_TYPE_UINT8", VRS_VALUE_TYPE_UINT8);
	PyModule_AddIntConstant(module, "VALUE_TYPE_UINT16", VRS_VALUE_TYPE_UINT16);
	PyModule_AddIntConstant(module, "VALUE_TYPE_UINT32", VRS_VALUE_TYPE_UINT32);
	PyModule_AddIntConstant(module, "VALUE_TYPE_UINT64", VRS_VALUE_TYPE_UINT64);
	PyModule_AddIntConstant(module, "VALUE_TYPE_REAL16", VRS_VALUE_TYPE_REAL16);
	PyModule_AddIntConstant(module, "VALUE_TYPE_REAL32", VRS_VALUE_TYPE_REAL32);
	PyModule_AddIntConstant(module, "VALUE_TYPE_REAL64", VRS_VALUE_TYPE_REAL64);
	PyModule_AddIntConstant(module, "VALUE_TYPE_STRING8", VRS_VALUE_TYPE_STRING8);

	/* Access permission flags */
	PyModule_AddIntConstant(module, "PERM_NODE_READ", VRS_PERM_NODE_READ);
	PyModule_AddIntConstant(module, "PERM_NODE_WRITE", VRS_PERM_NODE_WRITE);

	/* ID od special nodes */
	PyModule_AddIntConstant(module, "ROOT_NODE_ID", VRS_ROOT_NODE_ID);
	PyModule_AddIntConstant(module, "AVATAR_PARENT_NODE_ID", VRS_AVATAR_PARENT_NODE_ID);
	PyModule_AddIntConstant(module, "USERS_PARENT_NODE_ID", VRS_USERS_PARENT_NODE_ID);
	PyModule_AddIntConstant(module, "SCENE_PARENT_NODE_ID", VRS_SCENE_PARENT_NODE_ID);

	/* Node custom_types of specila nodes */
	PyModule_AddIntConstant(module, "ROOT_NODE_CT", VRS_ROOT_NODE_CT);
	PyModule_AddIntConstant(module, "AVATAR_PARENT_NODE_CT", VRS_AVATAR_PARENT_NODE_CT);
	PyModule_AddIntConstant(module, "USERS_PARENT_NODE_CT", VRS_USERS_PARENT_NODE_CT);
	PyModule_AddIntConstant(module, "SCENE_PARENT_NODE_CT", VRS_SCENE_PARENT_NODE_CT);
	PyModule_AddIntConstant(module, "AVATAR_NODE_CT", VRS_AVATAR_NODE_CT);
	PyModule_AddIntConstant(module, "AVATAR_INFO_NODE_CT", VRS_AVATAR_INFO_NODE_CT);
	PyModule_AddIntConstant(module, "USER_NODE_CT", VRS_USER_NODE_CT);

	/* User ID os special users */
	PyModule_AddIntConstant(module, "SUPER_USER_UID", VRS_SUPER_USER_UID);
	PyModule_AddIntConstant(module, "OTHER_USERS_UID", VRS_OTHER_USERS_UID);

#if PY_MAJOR_VERSION >= 3
	return module;
#endif
}

/* Main function of verse module */
int main(int argc, char *argv[])
{
#if PY_MAJOR_VERSION >= 3
	PyImport_AppendInittab("verse", PyInit_verse);
#else
	PyImport_AppendInittab("verse", initverse);
#endif

	(void)argc;

#if PY_MAJOR_VERSION >= 3
	Py_SetProgramName((wchar_t*)argv[0]);
#else
	Py_SetProgramName(argv[0]);
#endif

	Py_Initialize();

	PyImport_ImportModule("verse");

	return 1;
}
