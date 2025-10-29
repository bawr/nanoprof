# This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild
# type: ignore

import kaitaistruct
from kaitaistruct import KaitaiStruct, KaitaiStream, BytesIO


if getattr(kaitaistruct, 'API_VERSION', (0, 9)) < (0, 11):
    raise Exception("Incompatible Kaitai Struct Python API: 0.11 or later is required, but you have %s" % (kaitaistruct.__version__))

class Qnd(KaitaiStruct):
    def __init__(self, _io, _parent=None, _root=None):
        super(Qnd, self).__init__(_io)
        self._parent = _parent
        self._root = _root or self
        self._read()

    def _read(self):
        self.head = Qnd.Head(self._io, self, self._root)
        self.rec_list = []
        i = 0
        while not self._io.is_eof():
            self.rec_list.append(Qnd.Rec(self._io, self, self._root))
            i += 1



    def _fetch_instances(self):
        pass
        self.head._fetch_instances()
        for i in range(len(self.rec_list)):
            pass
            self.rec_list[i]._fetch_instances()


    class Code(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Code, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.ptr = self._io.read_u8le()
            self.filename = Qnd.Utf8(self._io, self, self._root)
            self.qualname = Qnd.Utf8(self._io, self, self._root)
            self.line = self._io.read_u2le()


        def _fetch_instances(self):
            pass
            self.filename._fetch_instances()
            self.qualname._fetch_instances()


    class Emit(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Emit, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.time = self._io.read_u8le()


        def _fetch_instances(self):
            pass


    class Head(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Head, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.tag = self._io.read_u4le()
            if not self.tag == 1885630065:
                raise kaitaistruct.ValidationNotEqualError(1885630065, self.tag, self._io, u"/types/head/seq/0")
            self.tick_mul = self._io.read_u8le()
            self.tick_div = self._io.read_u8le()


        def _fetch_instances(self):
            pass


    class Node(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Node, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.ptr = self._io.read_u8le()
            self.node_id = self._io.read_u4le()
            self.caller_id = self._io.read_u4le()


        def _fetch_instances(self):
            pass


    class Rec(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Rec, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.tag = self._io.read_u2le()
            _on = self.tag
            if _on == 4112:
                pass
                self.rec = Qnd.Emit(self._io, self, self._root)
            elif _on == 57024:
                pass
                self.rec = Qnd.Code(self._io, self, self._root)
            elif _on == 57040:
                pass
                self.rec = Qnd.Node(self._io, self, self._root)
            elif _on == 65535:
                pass
                self.rec = Qnd.Time(self._io, self, self._root)


        def _fetch_instances(self):
            pass
            _on = self.tag
            if _on == 4112:
                pass
                self.rec._fetch_instances()
            elif _on == 57024:
                pass
                self.rec._fetch_instances()
            elif _on == 57040:
                pass
                self.rec._fetch_instances()
            elif _on == 65535:
                pass
                self.rec._fetch_instances()


    class Time(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Time, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.thread_id = self._io.read_u8le()
            self.node_id = self._io.read_u4le()
            self.time_active = self._io.read_u8le()
            self.time_paused = self._io.read_u8le()
            self.time_waited = self._io.read_u8le()


        def _fetch_instances(self):
            pass


    class Utf8(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            super(Qnd.Utf8, self).__init__(_io)
            self._parent = _parent
            self._root = _root
            self._read()

        def _read(self):
            self.ptr = self._io.read_u8le()
            self.size = self._io.read_u2le()
            self.text = (self._io.read_bytes(self.size)).decode(u"UTF-8")


        def _fetch_instances(self):
            pass



