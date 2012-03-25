import xcb
import struct

__all__ = ['Struct', 'Union', 'Request', 'Response', 'Event', 'Reply', 'Error']

class Struct(xcb.Protobj):
    """XCB generic struct object"""
    pass

class Union(xcb.Protobj):
    """XCB generic union object"""
    pass

class Request(xcb.Protobj):
    def __init__(self, buffer, opcode, void, checked):
        # buffer is always generated from a StringIO, so the offset is always 0
        xcb.Protobj.__init__(self, buffer, 0)
        self.opcode = opcode
        self.is_void = void
        self.is_checked = checked

class Response(xcb.Protobj):
    """XCB generic response object"""
    def __init__(self, parent, offset):
        xcb.Protobj.__init__(self, parent, offset)
        # self is a xcb_generic_event_t
        self.response_type, self.sequence = struct.unpack_from('BxH', self)

class Event(Response):
    """XCB generic event object"""
    pass

class Reply(Response):
    """XCB generic reply object"""
    def __init__(self, parent, offset):
        Response.__init__(self, parent, offset)
        # self is a xcb_generic_reply_t
        (self.length, ) = struct.unpack_from('4xI', self)

class Error(Response):
    """XCB generic error object"""
    def __init__(self, parent, offset):
        Response.__init__(self, parent, offset)
        # self is a xcb_generic_error_t
        (self.code, ) = struct.unpack_from('xB', self)
