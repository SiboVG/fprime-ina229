import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location("bridge",Path(__file__).resolve().parents[1]/"examples/Ft232h/bridge.py")
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
class Port:
    def __init__(self): self.calls=[];self.short=False;self.fail=False
    def exchange(self,data,**kwargs):
        self.calls.append((data,kwargs))
        if self.fail: raise OSError("USB disconnected")
        return b"\x00\x54\x49"[:1 if self.short else len(data)]
class Tests(unittest.TestCase):
    def test_full_duplex_and_usb_failures(self):
        p=Port()
        self.assertEqual(bridge.exchange(p,b"\xf9\x00\x00"),b"\x00\x00\x54\x49")
        self.assertEqual(p.calls[0][1],dict(duplex=True,start=True,stop=True))
        p.short=True;self.assertEqual(bridge.exchange(p,b"abc"),b"\x01"+bytes(3))
        p.fail=True;self.assertEqual(bridge.exchange(p,b"abc"),b"\x01"+bytes(3))
    def test_invalid_lengths_do_not_touch_usb(self):
        p=Port()
        for data in [b"",bytes(7)]:
            with self.assertRaises(ValueError): bridge.exchange(p,data)
        self.assertEqual(p.calls,[])
if __name__=="__main__":unittest.main()
