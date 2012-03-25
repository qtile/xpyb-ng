#include "module.h"
#include "except.h"
#include "protobj.h"

/*
 * Helpers
 */

int
xpybRequest_get_attributes(PyObject *request, int *is_void, int *opcode, int *is_checked)
{
    PyObject *tmp;

    if (is_void != NULL) {
        tmp = PyObject_GetAttrString(request, "is_void");
        *is_void = PyObject_IsTrue(tmp);
        Py_XDECREF(tmp);
    }

    if (opcode != NULL) {
        tmp = PyObject_GetAttrString(request, "opcode");
        *opcode = (int) PyInt_AsLong(tmp);
        Py_XDECREF(tmp);
    }

    if (is_checked != NULL) {
        tmp = PyObject_GetAttrString(request, "is_checked");
        *is_checked = PyObject_IsTrue(tmp);
        Py_XDECREF(tmp);
    }
    return 0;
}

int
xpybError_set(xpybConn *conn, xcb_generic_error_t *e)
{
    unsigned char opcode;
    PyObject *shim, *error, *type, *except;

    type = (PyObject *)xpybError_type;
    except = xpybExcept_proto;

    if (e) {
        opcode = e->error_code;
        if (opcode < conn->errors_len && conn->errors[opcode] != NULL) {
            type = PyTuple_GET_ITEM(conn->errors[opcode], 0);
            except = PyTuple_GET_ITEM(conn->errors[opcode], 1);
        }

        shim = PyBuffer_FromMemory(e, sizeof(*e));
        if (shim == NULL)
            return 1;

        error = PyObject_CallFunctionObjArgs(type, shim, NULL);
        if (error != NULL)
            PyErr_SetObject(except, error);
        Py_DECREF(shim);
        return 1;
    }
    return 0;
}

PyObject *
xpybEvent_create(xpybConn *conn, xcb_generic_event_t *e)
{
    unsigned char opcode = e->response_type;
    PyObject *shim, *event, *type = (PyObject *)xpybEvent_type;

    if (opcode < conn->events_len && conn->events[opcode] != NULL)
        type = conn->events[opcode];

    shim = PyBuffer_FromMemory(e, sizeof(*e));
    if (shim == NULL)
        return NULL;

    event = PyObject_CallFunction(type, "Oi", shim, 0);
    Py_DECREF(shim);
    return event;
}


/*
 * Infrastructure
 */

static PyObject *
xpybProtobj_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybProtobj_init(xpybProtobj *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "parent", "offset", "size", NULL };
    Py_ssize_t offset = 0, size = Py_END_OF_BUFFER;
    PyObject *parent;

    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|nn", kwlist,
                                     &parent, &offset, &size))
        return -1;

    self->buf = PyBuffer_FromObject(parent, offset, size);
    if (self->buf == NULL)
        return -1;

    return 0;
}

static void
xpybProtobj_dealloc(xpybProtobj *self)
{
    Py_CLEAR(self->buf);
    self->ob_type->tp_free((PyObject *)self);
}

static Py_ssize_t
xpybProtobj_readbuf(xpybProtobj *self, Py_ssize_t s, void **p)
{
    return PyBuffer_Type.tp_as_buffer->bf_getreadbuffer(self->buf, s, p);
}

static Py_ssize_t
xpybProtobj_segcount(xpybProtobj *self, Py_ssize_t *s)
{
    return PyBuffer_Type.tp_as_buffer->bf_getsegcount(self->buf, s);
}

static Py_ssize_t
xpybProtobj_charbuf(xpybProtobj *self, Py_ssize_t s, char **p)
{
    return PyBuffer_Type.tp_as_buffer->bf_getcharbuffer(self->buf, s, p);
}

static Py_ssize_t
xpybProtobj_length(xpybProtobj *self)
{
    return PyBuffer_Type.tp_as_sequence->sq_length(self->buf);
}

static PyObject *
xpybProtobj_concat(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_concat(self->buf, arg);
}

static PyObject *
xpybProtobj_repeat(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_repeat(self->buf, arg);
}

static PyObject *
xpybProtobj_item(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_item(self->buf, arg);
}

static PyObject *
xpybProtobj_slice(xpybProtobj *self, Py_ssize_t arg1, Py_ssize_t arg2)
{
    return PyBuffer_Type.tp_as_sequence->sq_slice(self->buf, arg1, arg2);
}

static int
xpybProtobj_ass_item(xpybProtobj *self, Py_ssize_t arg1, PyObject *arg2)
{
    return PyBuffer_Type.tp_as_sequence->sq_ass_item(self->buf, arg1, arg2);
}

static int
xpybProtobj_ass_slice(xpybProtobj *self, Py_ssize_t arg1, Py_ssize_t arg2, PyObject *arg3)
{
    return PyBuffer_Type.tp_as_sequence->sq_ass_slice(self->buf, arg1, arg2, arg3);
}

static int
xpybProtobj_contains(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_contains(self->buf, arg);
}

static PyObject *
xpybProtobj_inplace_concat(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_inplace_concat(self->buf, arg);
}

static PyObject *
xpybProtobj_inplace_repeat(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_inplace_repeat(self->buf, arg);
}


/*
 * Members
 */


/*
 * Methods
 */


/*
 * Definition
 */

static PyBufferProcs xpybProtobj_bufops = {
    .bf_getreadbuffer = (readbufferproc)xpybProtobj_readbuf,
    .bf_getsegcount = (segcountproc)xpybProtobj_segcount,
    .bf_getcharbuffer = (charbufferproc)xpybProtobj_charbuf
};

static PySequenceMethods xpybProtobj_seqops = {
    .sq_length = (lenfunc)xpybProtobj_length,
    .sq_concat = (binaryfunc)xpybProtobj_concat,
    .sq_repeat = (ssizeargfunc)xpybProtobj_repeat,
    .sq_item = (ssizeargfunc)xpybProtobj_item,
    .sq_slice = (ssizessizeargfunc)xpybProtobj_slice,
    .sq_ass_item = (ssizeobjargproc)xpybProtobj_ass_item,
    .sq_ass_slice = (ssizessizeobjargproc)xpybProtobj_ass_slice,
    .sq_contains = (objobjproc)xpybProtobj_contains,
    .sq_inplace_concat = (binaryfunc)xpybProtobj_inplace_concat,
    .sq_inplace_repeat = (ssizeargfunc)xpybProtobj_inplace_repeat
};

PyTypeObject xpybProtobj_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Protobj",
    .tp_basicsize = sizeof(xpybProtobj),
    .tp_init = (initproc)xpybProtobj_init,
    .tp_new = xpybProtobj_new,
    .tp_dealloc = (destructor)xpybProtobj_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic X protocol object",
    .tp_as_buffer = &xpybProtobj_bufops,
    .tp_as_sequence = &xpybProtobj_seqops
};

PyTypeObject *xpybError_type;
PyTypeObject *xpybEvent_type;
PyTypeObject *xpybReply_type;
PyTypeObject *xpybRequest_type;
PyTypeObject *xpybStruct_type;

/*
 * Module init
 */
int xpybProtobj_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybProtobj_type) < 0)
        return -1;
    Py_INCREF(&xpybProtobj_type);
    if (PyModule_AddObject(m, "Protobj", (PyObject *)&xpybProtobj_type) < 0)
        return -1;

    PyObject *module = PyImport_ImportModule("xcb.protobj");
    if (!module)
        return -1;

    xpybError_type = (PyTypeObject *) PyObject_GetAttrString(module, "Error");
    xpybEvent_type = (PyTypeObject *) PyObject_GetAttrString(module, "Event");
    xpybReply_type = (PyTypeObject *) PyObject_GetAttrString(module, "Reply");
    xpybRequest_type = (PyTypeObject *) PyObject_GetAttrString(module, "Request");
    xpybStruct_type = (PyTypeObject *) PyObject_GetAttrString(module, "Struct");
    Py_INCREF(xpybError_type);
    Py_INCREF(xpybEvent_type);
    Py_INCREF(xpybReply_type);
    Py_INCREF(xpybRequest_type);
    Py_INCREF(xpybStruct_type);

    return 0;
}
