"""Execute the generated F Prime topology through a real local socket and SPI emulator."""
import importlib.util
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import unittest
spec=importlib.util.spec_from_file_location("bridge",Path(__file__).resolve().parents[1]/"examples/Ft232h/bridge.py")
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
class Sensor:
    def __init__(self, fail=False):
        self.reg={0:0,1:0xfb68,2:0,3:0,0x3e:0x5449,0x3f:0x2291}
        self.fail=fail
    def exchange(self,data,**kwargs):
        address=data[0]>>2
        if self.fail: raise OSError("injected USB error")
        if data[0]&1:
            raw={4:0,5:5120<<4,6:3200,7:0,8:0,9:0,10:0,11:3}.get(address,self.reg.get(address,0))
            return b"\0"+raw.to_bytes(len(data)-1,"big")
        value=int.from_bytes(data[1:],"big")
        if address==0 and value&0x8000:self.reg.update({0:0,1:0xfb68,2:0,3:0})
        else:self.reg[address]=value
        return bytes(len(data))
class Integration(unittest.TestCase):
    def run_demo(self,fail):
        with tempfile.TemporaryDirectory(prefix="ina229-",dir="/tmp") as directory:
            path=directory+"/spi.sock"
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as listener:
                listener.bind(path);listener.listen(1);listener.settimeout(3)
                errors=[]
                def serve():
                    try:
                        conn,_=listener.accept()
                        with conn:bridge.handle_connection(conn,Sensor(fail))
                    except (EOFError,OSError):pass
                    except Exception as error:errors.append(error)
                worker=threading.Thread(target=serve);worker.start()
                run=subprocess.run([os.environ["INA229_DEMO"],path,"0.01","8","1"],capture_output=True,text=True,timeout=5)
                worker.join(4)
                self.assertFalse(worker.is_alive());self.assertEqual([],errors)
                return run
    def test_full_fprime_path(self):
        run=self.run_demo(False)
        self.assertEqual(0,run.returncode,run.stderr)
        self.assertIn("bus_V",run.stdout)
        self.assertIn("0,127,1,",run.stdout)
    def test_transport_fault_is_not_valid_data(self):
        run=self.run_demo(True)
        self.assertNotEqual(0,run.returncode)
        self.assertNotIn("0,127,",run.stdout)
if __name__=="__main__":unittest.main()
